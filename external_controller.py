#!/usr/bin/env python3
"""
External Controller Service for IoV Digital Twin Task Offloading

This service has two responsibilities:
1. Generate route-aware vehicle trajectory predictions from live Redis state
    plus predefined SUMO routes, and publish them to dt2:pred:* for the
    secondary simulation.
2. Read SINR sequences from Redis, convert them to data rates, compute task
    completion times (tau_up, tau_comp, tau_down, tau_total), and write
    per-link tau candidates back to Redis for DRL consumption.

Prediction inputs:
- vehicle:{vehicle_id}:state
- vehicle:{vehicle_id}:route_progress
- Static route/network knowledge loaded once from the active SUMO files

Prediction outputs:
- dt2:pred:{run_id}:latest
- dt2:pred:{run_id}:cycle:{cycle_id}:entries
"""

import redis
import json
import time
import logging
from typing import Dict, List, Tuple, Optional
import math
import os
import xml.etree.ElementTree as ET

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class ExternalController:
    def __init__(self,
                 redis_host: str = 'localhost',
                 redis_port: int = 6379,
                 redis_db: int = 0,
                 run_id: Optional[str] = None):
        self.redis = redis.Redis(host=redis_host, port=redis_port, db=redis_db)
        # Keep default aligned with OMNeT++ secondary config for out-of-box testing.
        self.run_id = run_id or os.getenv("DT2_RUN_ID", "DT-Secondary-MotionChannel")
        self.project_dir = os.path.dirname(os.path.abspath(__file__))

        # Link rate model parameters (match OMNeT++ LinkRateModel)
        self.bandwidth_hz = 10e6  # 10 MHz (matches inetRadioMedium.mediumLimitCache.bandwidth in omnetpp.ini)
        self.rate_efficiency = 0.8  # Efficiency factor

        # Canonical Redis key contract produced by simulation modules.
        self.key_task_request = "task:{task_id}:request"
        self.key_task_state = "task:{task_id}:state"
        self.key_vehicle_state = "vehicle:{vehicle_id}:state"
        self.key_vehicle_route_progress = "vehicle:{vehicle_id}:route_progress"

        # Prediction parameters (match OMNeT++ secondary defaults)
        self.prediction_horizon_s = float(os.getenv("DT2_PRED_HORIZON_S", "0.5"))
        self.prediction_step_s = float(os.getenv("DT2_PRED_STEP_S", "0.02"))
        self.prediction_ttl_s = int(os.getenv("DT2_PRED_TTL_S", "2"))
        self.q_cycle_scan_limit = int(os.getenv("DT2_Q_SCAN_LIMIT", "50000"))
        # Poll interval (seconds, wall-clock) used for fallback polling and short waits.
        # Heartbeat cadence is typically 0.1s, so default to that to avoid redundant scans.
        self.poll_interval_s = float(os.getenv("DT2_POLL_INTERVAL_S", "0.1"))
        # Event-driven mode triggers prediction publishing immediately on vehicle-state writes.
        self.event_driven_predictions = os.getenv("DT2_EVENT_DRIVEN_PRED", "1") == "1"
        self._pubsub = None
        self._keyspace_pattern = ""
        self.last_prediction_source_ts = -1.0
        self.prediction_cycle_id = 0

        # Static SUMO knowledge loaded once.
        self.vehicle_routes: Dict[str, List[str]] = {}
        self.edge_shapes: Dict[str, List[Tuple[float, float]]] = {}
        self.edge_lengths: Dict[str, float] = {}
        self.vehicle_route_cursor: Dict[str, int] = {}
        self._load_static_route_knowledge()
        self._init_prediction_triggering()

    def _init_prediction_triggering(self) -> None:
        if not self.event_driven_predictions:
            logger.info("Prediction trigger mode: polling every %.3fs", self.poll_interval_s)
            return

        try:
            cfg = self.redis.config_get("notify-keyspace-events")
            flags = cfg.get("notify-keyspace-events", "") if isinstance(cfg, dict) else ""
            if isinstance(flags, bytes):
                flags = flags.decode(errors="ignore")

            # Need keyspace events + hash events to react to HSET/HMSET on vehicle latest keys.
            has_keyspace = "K" in flags
            has_hash = "h" in flags
            if not (has_keyspace and has_hash):
                merged = "".join(sorted(set((flags or "") + "Kh")))
                self.redis.config_set("notify-keyspace-events", merged)
                logger.info("Enabled Redis keyspace notifications: %s", merged)

            self._pubsub = self.redis.pubsub(ignore_subscribe_messages=True)
            self._keyspace_pattern = f"__keyspace@{self.redis.connection_pool.connection_kwargs.get('db', 0)}__:dt2:vehicle:{self.run_id}:*:latest"
            self._pubsub.psubscribe(self._keyspace_pattern)
            logger.info("Prediction trigger mode: event-driven via %s", self._keyspace_pattern)
        except Exception as exc:
            # Fallback to polling if Redis permissions/config block keyspace notifications.
            logger.warning("Event-driven prediction trigger unavailable (%s), falling back to polling", exc)
            self.event_driven_predictions = False
            self._pubsub = None

    def _wait_for_vehicle_update_event(self, timeout_s: float) -> bool:
        if not self.event_driven_predictions or self._pubsub is None:
            return False

        deadline = time.time() + max(0.0, timeout_s)
        while time.time() <= deadline:
            msg = self._pubsub.get_message(timeout=0.05)
            if not msg:
                continue

            channel = msg.get("channel", b"")
            data = msg.get("data", b"")
            if isinstance(channel, bytes):
                channel = channel.decode(errors="ignore")
            if isinstance(data, bytes):
                data = data.decode(errors="ignore")

            if not isinstance(channel, str) or not isinstance(data, str):
                continue
            if not channel.startswith(f"__keyspace@"):
                continue
            if ":dt2:vehicle:" not in channel:
                continue
            if data.lower() not in {"hset", "hmset", "hincrby", "expire", "set"}:
                continue
            return True

        return False

    def sinr_to_rate_bps(self, sinr_db: float) -> float:
        """Convert SINR (dB) to effective data rate (bps) using Shannon formula."""
        if sinr_db <= 0:
            return 0.0

        gamma = 10 ** (sinr_db / 10.0)  # Linear SINR
        rate_bps = self.bandwidth_hz * math.log2(1 + gamma) * self.rate_efficiency
        return min(rate_bps, 100e6)  # Cap at 100 Mbps

    def accumulate_transmission_time(self, sinr_sequence: List[float], data_bits: int) -> float:
        """Accumulate transmission time across SINR sequence until data_bits are sent."""
        total_time = 0.0
        slot_duration = 0.02  # 20ms slots (match OMNeT++)

        for sinr_db in sinr_sequence:
            rate_bps = self.sinr_to_rate_bps(sinr_db)
            if rate_bps <= 0:
                continue

            bits_per_slot = rate_bps * slot_duration
            if bits_per_slot >= data_bits:
                # Last slot completes transmission
                remaining_time = data_bits / rate_bps
                total_time += remaining_time
                break
            else:
                # Full slot used
                data_bits -= bits_per_slot
                total_time += slot_duration

        return total_time

    def _decode_hash(self, data: Dict[bytes, bytes]) -> Dict[str, str]:
        return {
            k.decode(errors='ignore'): v.decode(errors='ignore')
            for k, v in data.items()
        }

    def _to_float(self, value: Optional[str], default: float = 0.0) -> float:
        if value is None:
            return default
        try:
            return float(value)
        except (TypeError, ValueError):
            return default

    def _resolve_sumo_inputs(self) -> Tuple[str, str]:
        omnetpp_ini = os.path.join(self.project_dir, "omnetpp.ini")
        manager_cfg = "erlangen.sumo.cfg"

        try:
            with open(omnetpp_ini, "r", encoding="utf-8") as fh:
                for line in fh:
                    stripped = line.strip()
                    if stripped.startswith("*.manager.configFile") and "=" in stripped:
                        manager_cfg = stripped.split("=", 1)[1].strip().strip('"')
                        break
        except OSError:
            logger.warning("Could not read omnetpp.ini, falling back to default SUMO config")

        manager_cfg_path = os.path.join(self.project_dir, manager_cfg)
        tree = ET.parse(manager_cfg_path)
        root = tree.getroot()
        input_node = root.find("input")
        if input_node is None:
            raise RuntimeError(f"SUMO config missing <input>: {manager_cfg_path}")

        net_file = input_node.find("net-file").attrib["value"]
        route_files = input_node.find("route-files").attrib["value"]
        route_file = route_files.split(",")[0].strip()
        return os.path.join(self.project_dir, net_file), os.path.join(self.project_dir, route_file)

    def _polyline_length(self, pts: List[Tuple[float, float]]) -> float:
        total = 0.0
        for i in range(1, len(pts)):
            dx = pts[i][0] - pts[i - 1][0]
            dy = pts[i][1] - pts[i - 1][1]
            total += math.hypot(dx, dy)
        return total

    def _parse_shape(self, shape: str) -> List[Tuple[float, float]]:
        pts: List[Tuple[float, float]] = []
        for token in shape.split():
            if "," not in token:
                continue
            sx, sy = token.split(",", 1)
            pts.append((float(sx), float(sy)))
        return pts

    def _interpolate_on_shape(self, pts: List[Tuple[float, float]], offset_m: float) -> Tuple[float, float, float]:
        if not pts:
            return 0.0, 0.0, 0.0
        if len(pts) == 1:
            return pts[0][0], pts[0][1], 0.0

        travelled = 0.0
        for i in range(1, len(pts)):
            x0, y0 = pts[i - 1]
            x1, y1 = pts[i]
            seg_len = math.hypot(x1 - x0, y1 - y0)
            if offset_m <= travelled + seg_len or i == len(pts) - 1:
                rel = 0.0 if seg_len <= 1e-9 else (offset_m - travelled) / seg_len
                rel = max(0.0, min(1.0, rel))
                x = x0 + rel * (x1 - x0)
                y = y0 + rel * (y1 - y0)
                heading = math.degrees(math.atan2(y1 - y0, x1 - x0))
                return x, y, heading
            travelled += seg_len

        x0, y0 = pts[-2]
        x1, y1 = pts[-1]
        return x1, y1, math.degrees(math.atan2(y1 - y0, x1 - x0))

    def _load_static_route_knowledge(self) -> None:
        net_path, route_path = self._resolve_sumo_inputs()

        net_tree = ET.parse(net_path)
        net_root = net_tree.getroot()
        for edge in net_root.findall("edge"):
            edge_id = edge.attrib.get("id", "")
            if not edge_id or edge.attrib.get("function") == "internal":
                continue
            lane = edge.find("lane")
            if lane is None:
                continue
            shape = self._parse_shape(lane.attrib.get("shape", ""))
            if not shape:
                continue
            self.edge_shapes[edge_id] = shape
            self.edge_lengths[edge_id] = self._polyline_length(shape)

        route_tree = ET.parse(route_path)
        route_root = route_tree.getroot()
        route_defs: Dict[str, List[str]] = {}
        for route in route_root.findall("route"):
            route_id = route.attrib.get("id", "")
            edges = route.attrib.get("edges", "").split()
            if route_id and edges:
                route_defs[route_id] = edges

        for vehicle in route_root.findall("vehicle"):
            vehicle_id = vehicle.attrib.get("id", "")
            route_id = vehicle.attrib.get("route", "")
            if vehicle_id and route_id in route_defs:
                edges = list(route_defs[route_id])
                self.vehicle_routes[vehicle_id] = edges
                # OMNeT++ application-level vehicle_id is numeric module index ("0", "1", ...)
                # while SUMO vehicle IDs in the route file are "v0", "v1", ... . Store both.
                if vehicle_id.startswith("v") and vehicle_id[1:].isdigit():
                    self.vehicle_routes[vehicle_id[1:]] = edges

        logger.info(
            "Loaded static route knowledge: vehicles=%s route-mapped, edges=%s geometries",
            len(self.vehicle_routes),
            len(self.edge_shapes),
        )

    def _get_all_vehicle_states(self) -> List[Dict[str, object]]:
        vehicles: List[Dict[str, object]] = []

        # Primary schema: dt2:vehicle:<run_id>:<vehicle_id>:latest (written by RedisDigitalTwin)
        dt2_pattern = f"dt2:vehicle:{self.run_id}:*:latest"
        for raw_key in self.redis.keys(dt2_pattern):
            key = raw_key.decode(errors="ignore")
            parts = key.split(":")
            # key format: dt2:vehicle:<run_id>:<vehicle_id>:latest  → parts[-2] is vehicle_id
            vehicle_id = parts[-2]
            state_raw = self.redis.hgetall(key)
            if not state_raw:
                continue
            state = self._decode_hash(state_raw)
            # Also try legacy route_progress if it exists (optional).
            route_raw = self.redis.hgetall(self.key_vehicle_route_progress.format(vehicle_id=vehicle_id))
            route = self._decode_hash(route_raw) if route_raw else {}
            vehicles.append({
                "vehicle_id": vehicle_id,
                "source_timestamp": self._to_float(
                    state.get("sim_time"),
                    self._to_float(route.get("source_timestamp"), 0.0)
                ),
                "pos_x": self._to_float(state.get("pos_x")),
                "pos_y": self._to_float(state.get("pos_y")),
                "speed": self._to_float(state.get("speed")),
                "acceleration": self._to_float(state.get("acceleration")),
                "heading": self._to_float(state.get("heading")),
                "current_edge_id": route.get("current_edge_id", ""),
                "lane_pos_m": self._to_float(route.get("lane_pos_m")),
            })

        if vehicles:
            return vehicles

        # Fallback: legacy schema vehicle:<vehicle_id>:state (TTL-based, may be empty)
        for raw_key in self.redis.keys("vehicle:*:state"):
            key = raw_key.decode(errors="ignore")
            parts = key.split(":")
            if len(parts) < 3:
                continue
            vehicle_id = parts[1]
            state_raw = self.redis.hgetall(key)
            route_raw = self.redis.hgetall(self.key_vehicle_route_progress.format(vehicle_id=vehicle_id))
            if not state_raw:
                continue
            state = self._decode_hash(state_raw)
            route = self._decode_hash(route_raw) if route_raw else {}
            vehicles.append({
                "vehicle_id": vehicle_id,
                "source_timestamp": self._to_float(route.get("source_timestamp"), self._to_float(state.get("source_timestamp"), 0.0)),
                "pos_x": self._to_float(state.get("pos_x")),
                "pos_y": self._to_float(state.get("pos_y")),
                "speed": self._to_float(state.get("speed")),
                "acceleration": self._to_float(state.get("acceleration")),
                "heading": self._to_float(state.get("heading")),
                "current_edge_id": route.get("current_edge_id", ""),
                "lane_pos_m": self._to_float(route.get("lane_pos_m")),
            })
        return vehicles

    def _resolve_route_edge_index(self, vehicle_id: str, current_edge_id: str, route_edges: List[str]) -> Optional[int]:
        if current_edge_id in route_edges:
            idx = route_edges.index(current_edge_id)
            self.vehicle_route_cursor[vehicle_id] = idx
            return idx

        cached = self.vehicle_route_cursor.get(vehicle_id)
        if cached is not None and 0 <= cached < len(route_edges):
            return cached
        return None

    def _predict_vehicle_constant_velocity(self, vehicle: Dict[str, object]) -> List[Dict[str, float]]:
        """Constant-velocity dead-reckoning fallback when no route data is available."""
        vehicle_id = str(vehicle["vehicle_id"])
        speed = max(0.0, float(vehicle.get("speed", 0.0)))
        acceleration = float(vehicle.get("acceleration", 0.0))
        source_ts = float(vehicle.get("source_timestamp", 0.0))
        heading_deg = float(vehicle.get("heading", 0.0))
        pos_x = float(vehicle.get("pos_x", 0.0))
        pos_y = float(vehicle.get("pos_y", 0.0))
        heading_rad = heading_deg * math.pi / 180.0
        cos_h = math.cos(heading_rad)
        sin_h = math.sin(heading_rad)
        steps = max(1, int(math.floor(self.prediction_horizon_s / self.prediction_step_s + 1e-9)))
        predictions = []
        for step_index in range(1, steps + 1):
            t = step_index * self.prediction_step_s
            delta = max(0.0, speed * t + 0.5 * acceleration * t * t)
            predictions.append({
                "vehicle_id": vehicle_id,
                "step_index": step_index,
                "predicted_time": source_ts + t,
                "pos_x": pos_x + delta * cos_h,
                "pos_y": pos_y + delta * sin_h,
                "speed": max(0.0, speed + acceleration * t),
                "heading": heading_deg,
                "acceleration": acceleration,
            })
        return predictions

    def _predict_vehicle(self, vehicle: Dict[str, object]) -> List[Dict[str, float]]:
        vehicle_id = str(vehicle["vehicle_id"])
        route_edges = self.vehicle_routes.get(vehicle_id)
        if not route_edges:
            # No static route knowledge — use constant-velocity dead-reckoning.
            return self._predict_vehicle_constant_velocity(vehicle)

        current_edge_id = str(vehicle.get("current_edge_id", ""))
        edge_index = self._resolve_route_edge_index(vehicle_id, current_edge_id, route_edges)
        if edge_index is None:
            # Have route but can't determine current position on it — fall back.
            return self._predict_vehicle_constant_velocity(vehicle)

        speed = max(0.0, float(vehicle.get("speed", 0.0)))
        acceleration = float(vehicle.get("acceleration", 0.0))
        source_ts = float(vehicle.get("source_timestamp", 0.0))
        lane_pos_m = max(0.0, float(vehicle.get("lane_pos_m", 0.0)))

        predictions: List[Dict[str, float]] = []
        steps = max(1, int(math.floor(self.prediction_horizon_s / self.prediction_step_s + 1e-9)))
        cached_edge = route_edges[edge_index]
        if current_edge_id.startswith(":"):
            lane_pos_m = self.edge_lengths.get(cached_edge, lane_pos_m)

        for step_index in range(1, steps + 1):
            t = step_index * self.prediction_step_s
            delta_s = max(0.0, speed * t + 0.5 * acceleration * t * t)
            projected_speed = max(0.0, speed + acceleration * t)

            cursor = edge_index
            offset = lane_pos_m + delta_s
            while cursor < len(route_edges):
                edge_id = route_edges[cursor]
                edge_len = self.edge_lengths.get(edge_id, 0.0)
                if offset <= edge_len or cursor == len(route_edges) - 1:
                    shape = self.edge_shapes.get(edge_id)
                    if not shape:
                        break
                    pos_x, pos_y, heading = self._interpolate_on_shape(shape, min(offset, edge_len))
                    predictions.append({
                        "vehicle_id": vehicle_id,
                        "step_index": step_index,
                        "predicted_time": source_ts + t,
                        "pos_x": pos_x,
                        "pos_y": pos_y,
                        "speed": projected_speed,
                        "heading": heading,
                        "acceleration": acceleration,
                    })
                    break
                offset -= edge_len
                cursor += 1

        return predictions

    def maybe_publish_prediction_cycle(self) -> None:
        vehicles = self._get_all_vehicle_states()
        if not vehicles:
            return

        latest_source_ts = max(float(v.get("source_timestamp", 0.0)) for v in vehicles)
        if latest_source_ts <= self.last_prediction_source_ts:
            return

        self.prediction_cycle_id += 1
        cycle_id = self.prediction_cycle_id
        stream_key = f"dt2:pred:{self.run_id}:cycle:{cycle_id}:entries"
        latest_key = f"dt2:pred:{self.run_id}:latest"
        trajectory_count = 0

        for vehicle in vehicles:
            predicted = self._predict_vehicle(vehicle)
            if not predicted:
                continue
            trajectory_count += 1
            for point in predicted:
                self.redis.xadd(
                    stream_key,
                    {
                        "cycle_id": str(cycle_id),
                        "vehicle_id": point["vehicle_id"],
                        "step_index": str(point["step_index"]),
                        "predicted_time": f"{point['predicted_time']:.6f}",
                        "pos_x": f"{point['pos_x']:.6f}",
                        "pos_y": f"{point['pos_y']:.6f}",
                        "speed": f"{point['speed']:.6f}",
                        "heading": f"{point['heading']:.6f}",
                        "acceleration": f"{point['acceleration']:.6f}",
                    },
                    maxlen=200000,
                    approximate=True,
                )

        if trajectory_count > 0:
            self.redis.hset(
                latest_key,
                mapping={
                    "cycle_id": str(cycle_id),
                    "generated_at": f"{latest_source_ts:.6f}",
                    "horizon_s": f"{self.prediction_horizon_s:.6f}",
                    "step_s": f"{self.prediction_step_s:.6f}",
                    "trajectory_count": str(trajectory_count),
                },
            )
            self.redis.expire(latest_key, max(1, self.prediction_ttl_s))
            logger.info(
                "prediction-cycle run_id=%s cycle=%s trajectories=%s source_ts=%.3f",
                self.run_id,
                cycle_id,
                trajectory_count,
                latest_source_ts,
            )
            self.last_prediction_source_ts = latest_source_ts

    def get_task_metadata(self, task_id: str) -> Optional[Dict]:
        """Retrieve task metadata from Redis using simulation key contract."""
        request_key = self.key_task_request.format(task_id=task_id)
        state_key = self.key_task_state.format(task_id=task_id)

        request_raw = self.redis.hgetall(request_key)
        state_raw = self.redis.hgetall(state_key)

        if not request_raw and not state_raw:
            return None

        request = self._decode_hash(request_raw) if request_raw else {}
        state = self._decode_hash(state_raw) if state_raw else {}

        source_vehicle = request.get('vehicle_id', '') or state.get('vehicle_id', '')

        d_in_bytes = self._to_float(request.get('input_size_bytes'), 0.0)
        d_out_bytes = self._to_float(request.get('output_size_bytes'), 0.0)
        c_cycles = self._to_float(request.get('cpu_cycles'), 0.0)

        if d_in_bytes <= 0.0:
            d_in_bytes = self._to_float(state.get('input_size_bytes'), 0.0)
        if d_out_bytes <= 0.0:
            d_out_bytes = self._to_float(state.get('output_size_bytes'), 0.0)
        if c_cycles <= 0.0:
            c_cycles = self._to_float(state.get('cpu_cycles'), 0.0)

        # Backward compatibility for old schema where d_in/d_out were in MB.
        if d_in_bytes <= 0.0 and 'd_in' in state:
            d_in_bytes = self._to_float(state.get('d_in'), 0.0) * 1e6
        if d_out_bytes <= 0.0 and 'd_out' in state:
            d_out_bytes = self._to_float(state.get('d_out'), 0.0) * 1e6
        if c_cycles <= 0.0 and 'c_cycles' in state:
            c_cycles = self._to_float(state.get('c_cycles'), 0.0)

        return {
            'd_in_bytes': d_in_bytes,
            'd_out_bytes': d_out_bytes,
            'c_cycles': c_cycles,
            'source_vehicle': source_vehicle,
        }

    def _normalize_cpu_hz(self, cpu_value: float, default_hz: float = 1e9) -> float:
        """Normalize CPU availability values to Hz.

        Some producers store CPU in GHz while others store in Hz.
        """
        if cpu_value <= 0.0:
            return default_hz
        if cpu_value < 1e6:
            return cpu_value * 1e9
        return cpu_value

    def get_compute_capacity_hz(self, target_id: str) -> float:
        """Get available compute capacity in Hz for a vehicle or RSU target."""
        # RSU resources are stored in a dedicated keyspace.
        if target_id.startswith("RSU_"):
            rsu_key = f"rsu:{target_id}:resources"
            rsu_raw = self.redis.hgetall(rsu_key)
            if rsu_raw:
                rsu = self._decode_hash(rsu_raw)
                cpu_available = self._to_float(rsu.get('cpu_available'), 0.0)
                return max(self._normalize_cpu_hz(cpu_available), 1.0)

        # Primary source: dt2 vehicle latest state for this run.
        dt2_key = f"dt2:vehicle:{self.run_id}:{target_id}:latest"
        dt2_raw = self.redis.hgetall(dt2_key)
        if dt2_raw:
            dt2_state = self._decode_hash(dt2_raw)
            cpu_available = self._to_float(dt2_state.get('cpu_available'), 0.0)
            if cpu_available > 0.0:
                return max(self._normalize_cpu_hz(cpu_available), 1.0)

        # Legacy fallback: vehicle:<id>:state.
        key = self.key_vehicle_state.format(vehicle_id=target_id)
        raw = self.redis.hgetall(key)
        if raw:
            data = self._decode_hash(raw)
            cpu_available = self._to_float(data.get('cpu_available'), 0.0)
            if cpu_available > 0.0:
                return max(self._normalize_cpu_hz(cpu_available), 1.0)

        return 1e9  # Conservative default

    def process_q_entries(self, cycle_id: int) -> Dict[str, Dict]:
        """Process Q entries for a cycle, group by link."""
        stream_key = f"dt2:q:{self.run_id}:entries"
        entries = self.redis.xrevrange(stream_key, max='+', min='-', count=self.q_cycle_scan_limit)

        links = {}  # link_key -> {'sinr_sequence': [], 'metadata': {}}

        seen_target_cycle = False
        for entry_id, fields in entries:
            entry_cycle = int(fields.get(b'cycle_index', 0))
            if entry_cycle > cycle_id:
                continue
            if entry_cycle < cycle_id:
                if seen_target_cycle:
                    break
                continue
            seen_target_cycle = True

            # Parse entry
            link_type = fields.get(b'link_type', b'').decode()
            tx_id = fields.get(b'tx_id', b'').decode()
            rx_id = fields.get(b'rx_id', b'').decode()
            step_index = int(fields.get(b'step_index', 0))
            sinr_db = float(fields.get(b'sinr_db', -100))
            predicted_time = float(fields.get(b'predicted_time', 0))

            link_key = f"{link_type}:{tx_id}:{rx_id}"

            if link_key not in links:
                links[link_key] = {
                    'link_type': link_type,
                    'tx_id': tx_id,
                    'rx_id': rx_id,
                    'sinr_sequence': [],
                    'predicted_times': []
                }

            # Store SINR in sequence (assume steps are in order)
            links[link_key]['sinr_sequence'].append(sinr_db)
            links[link_key]['predicted_times'].append(predicted_time)

        # xrevrange returns newest-first; sort each link sequence by predicted time.
        for link in links.values():
            combined = sorted(zip(link['predicted_times'], link['sinr_sequence']))
            link['sinr_sequence'] = [sinr for _, sinr in combined]
            link['predicted_times'] = [ts for ts, _ in combined]

        return links

    def get_latest_q_cycle(self) -> Optional[int]:
        """Read latest published secondary Q cycle index."""
        latest_key = f"dt2:q:{self.run_id}:latest"
        raw = self.redis.hget(latest_key, "cycle_index")
        if raw is not None:
            try:
                return int(raw)
            except (TypeError, ValueError):
                pass

        # Fallback: derive latest cycle from newest stream entry.
        stream_key = f"dt2:q:{self.run_id}:entries"
        tail = self.redis.xrevrange(stream_key, count=1)
        if not tail:
            return None
        _, fields = tail[0]
        try:
            return int(fields.get(b'cycle_index', 0))
        except (TypeError, ValueError):
            return None

    def write_tau_candidates(self, task_id: str, cycle_id: int,
                             source_vehicle: str,
                             candidates: List[Dict]) -> None:
        """Persist all per-link tau candidates for DRL policy input."""
        key = f"task:{task_id}:tau_candidates"
        payload = {
            'task_id': task_id,
            'cycle_id': str(int(cycle_id)),
            'source_vehicle': source_vehicle,
            'candidate_count': str(len(candidates)),
            'controller_ts': f"{time.time():.6f}",
            'candidates_json': json.dumps(candidates),
        }
        self.redis.hset(key, mapping=payload)

        # Append immutable audit log entry for debugging and training replay.
        self.redis.xadd(
            "controller:tau_candidates:log",
            {
                'task_id': task_id,
                'cycle_id': str(int(cycle_id)),
                'source_vehicle': source_vehicle,
                'candidate_count': str(len(candidates)),
                'controller_ts': f"{time.time():.6f}",
                'candidates_json': json.dumps(candidates),
            },
            maxlen=50000,
            approximate=True,
        )

    def _slice_from_offset(self, sinr_sequence: List[float],
                           predicted_times: List[float],
                           t_start: float) -> List[float]:
        """Return the sub-sequence of SINR values whose predicted_time >= t_start.

        This implements the downlink slot-offset offset: the downlink window
        starts at t_start = t0 + tau_up + tau_comp, so we skip all slots that
        have already elapsed before the return transmission begins.
        """
        if not predicted_times:
            return sinr_sequence  # No timing info — fall back to full sequence.

        t0 = predicted_times[0]
        sliced = [
            sinr
            for predicted_time, sinr in zip(predicted_times, sinr_sequence)
            if predicted_time >= t0 + t_start
        ]
        # If offset is beyond the end of the window, use the last available slot
        # as a best-effort estimate (channel is assumed stationary at tail).
        return sliced if sliced else sinr_sequence[-1:]

    def compute_tau_candidates(self, task_id: str, cycle_id: int) -> Optional[Dict]:
        """Compute per-link tau candidates for a task using latest Q cycle.

        Tau timeline per candidate:
          tau_up   — time to upload d_in bits over the UL link (V2V or V2RSU)
          tau_comp — time to execute c_cycles on the service node
          tau_down — time to download d_out bits over the DL link, starting
                     at slot-offset t_start = tau_up + tau_comp into the
                     predicted window (so we use the correct future SINR slots)
        """
        # Get task metadata
        task = self.get_task_metadata(task_id)
        if not task:
            logger.warning(f"No task metadata for {task_id}")
            return None

        d_in_bits = task['d_in_bytes'] * 8.0
        d_out_bits = task['d_out_bytes'] * 8.0
        c_cycles = task['c_cycles']
        source_vehicle = task['source_vehicle']

        # Get Q entries for this cycle
        links = self.process_q_entries(cycle_id)
        if not links:
            logger.warning(f"No Q entries for cycle {cycle_id}")
            return None

        candidates = []

        # Evaluate each potential uplink (source_vehicle is the transmitter)
        for link_key, link_data in links.items():
            link_type = link_data['link_type']
            tx_id = link_data['tx_id']
            rx_id = link_data['rx_id']
            ul_sinr_seq = link_data['sinr_sequence']
            ul_pred_times = link_data['predicted_times']

            if link_type == 'V2V' and tx_id == source_vehicle:
                service_vehicle = rx_id

                # --- Upload ---
                tau_up = self.accumulate_transmission_time(ul_sinr_seq, d_in_bits)

                # --- Compute ---
                f_avail_hz = self.get_compute_capacity_hz(service_vehicle)
                tau_comp = c_cycles / f_avail_hz

                # --- Download: find reverse V2V link (service -> source) ---
                dl_key = f"V2V:{service_vehicle}:{source_vehicle}"
                dl_data = links.get(dl_key)
                if dl_data:
                    dl_sinr_seq = self._slice_from_offset(
                        dl_data['sinr_sequence'],
                        dl_data['predicted_times'],
                        tau_up + tau_comp,
                    )
                else:
                    # No explicit reverse-link prediction, skip this candidate.
                    continue
                tau_down = self.accumulate_transmission_time(dl_sinr_seq, d_out_bits)

            elif link_type == 'V2RSU' and tx_id == source_vehicle:
                service_vehicle = rx_id  # RSU id

                # --- Upload ---
                tau_up = self.accumulate_transmission_time(ul_sinr_seq, d_in_bits)

                # --- Compute (RSU has higher capacity; read from vehicle state or default) ---
                f_avail_hz = self.get_compute_capacity_hz(service_vehicle)
                tau_comp = c_cycles / f_avail_hz

                # --- Download: RSU -> Vehicle (RSU2V reverse link) ---
                dl_key = f"RSU2V:{service_vehicle}:{source_vehicle}"
                dl_data = links.get(dl_key)
                if dl_data:
                    dl_sinr_seq = self._slice_from_offset(
                        dl_data['sinr_sequence'],
                        dl_data['predicted_times'],
                        tau_up + tau_comp,
                    )
                else:
                    # No explicit reverse-link prediction, skip this candidate.
                    continue
                tau_down = self.accumulate_transmission_time(dl_sinr_seq, d_out_bits)

            else:
                continue

            tau_total = tau_up + tau_comp + tau_down

            candidates.append({
                'link_type': link_type,
                'tx_id': tx_id,
                'target_id': service_vehicle,
                'tau_up': tau_up,
                'tau_comp': tau_comp,
                'tau_down': tau_down,
                'tau_total': tau_total,
                'f_avail_hz': f_avail_hz,
            })

        if not candidates:
            return None

        return {
            'task_id': task_id,
            'cycle_id': cycle_id,
            'source_vehicle': source_vehicle,
            'candidates': sorted(candidates, key=lambda x: x['tau_total']),
            'timestamp': time.time(),
        }

    def run(self):
        """Main service loop."""
        logger.info("External Controller started (run_id=%s)", self.run_id)

        while True:
            try:
                # Publish predictions immediately when fresh heartbeat/vehicle state arrives.
                if self.event_driven_predictions:
                    if self._wait_for_vehicle_update_event(self.poll_interval_s):
                        self.maybe_publish_prediction_cycle()
                else:
                    self.maybe_publish_prediction_cycle()

                task_id_raw = self.redis.lpop("offloading_requests:queue")
                if not task_id_raw:
                    time.sleep(self.poll_interval_s)
                    continue

                task_id = task_id_raw.decode(errors='ignore')
                latest_cycle = self.get_latest_q_cycle()
                if latest_cycle is None:
                    # Re-queue for retry when Q cycle arrives.
                    self.redis.rpush("offloading_requests:queue", task_id)
                    if not self.event_driven_predictions:
                        self.maybe_publish_prediction_cycle()
                    time.sleep(self.poll_interval_s)
                    continue

                tau_pack = self.compute_tau_candidates(task_id, latest_cycle)
                if not tau_pack:
                    # Re-queue if inputs are not ready yet.
                    self.redis.rpush("offloading_requests:queue", task_id)
                    if not self.event_driven_predictions:
                        self.maybe_publish_prediction_cycle()
                    time.sleep(self.poll_interval_s)
                    continue

                self.write_tau_candidates(
                    task_id=task_id,
                    cycle_id=int(tau_pack.get('cycle_id', latest_cycle)),
                    source_vehicle=str(tau_pack.get('source_vehicle', '')),
                    candidates=tau_pack.get('candidates', []),
                )

                # Notify DRL agent that tau candidates are ready for this task.
                self.redis.rpush("drl_pending:queue", task_id)

                best_tau = float(tau_pack['candidates'][0]['tau_total']) if tau_pack['candidates'] else -1.0
                logger.info(
                    "tau-candidates task=%s cycle=%s candidates=%s best_tau=%.6f → queued for DRL",
                    task_id,
                    tau_pack.get('cycle_id', latest_cycle),
                    len(tau_pack.get('candidates', [])),
                    best_tau,
                )

            except Exception as e:
                logger.error(f"Controller error: {e}")
                time.sleep(5)

if __name__ == "__main__":
    controller = ExternalController()
    controller.run()