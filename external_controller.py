#!/usr/bin/env python3
"""
External Controller Service for IoV Digital Twin Task Offloading

This service is responsible for generating route-aware vehicle trajectory
predictions from live Redis state plus predefined SUMO routes, and publishing
them to dt2:pred:* for the secondary simulation.

Prediction inputs:
- vehicle:{vehicle_id}:state
- vehicle:{vehicle_id}:route_progress
- Static route/network knowledge loaded once from the active SUMO files

Prediction outputs:
- dt2:pred:{run_id}:latest
- dt2:pred:{run_id}:cycle:{cycle_id}:entries

Note:
- Tau computation is owned by Task-Offloading-Algorithm (Phase 2 architecture).
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


    def run(self):
        """Main service loop.
        
        Phase 1: Publish SINR sequences from secondary DT
        Phase 2: (Disabled) Task-Offloading-Algorithm now owns tau computation
        
        OLD FLOW (now disabled):
          offloading_requests:queue → [compute tau] → write tau_candidates → drl_pending:queue
        
        NEW FLOW:
          offloading_requests:queue ← [Task-Offloading-Algorithm consumes directly]
          dt2:q:*:entries ← [SINR from secondary DT] ← [Task-Offloading-Algorithm reads for tau]
        """
        logger.info("External Controller started (run_id=%s)", self.run_id)

        while True:
            try:
                # Publish predictions immediately when fresh heartbeat/vehicle state arrives.
                if self.event_driven_predictions:
                    if self._wait_for_vehicle_update_event(self.poll_interval_s):
                        self.maybe_publish_prediction_cycle()
                else:
                    self.maybe_publish_prediction_cycle()

                # NOTE: Do NOT consume offloading_requests:queue here.
                # Task-Offloading-Algorithm now owns that queue and tau computation.
                time.sleep(self.poll_interval_s)

            except Exception as e:
                logger.error(f"Controller error: {e}")
                time.sleep(5)

if __name__ == "__main__":
    controller = ExternalController()
    controller.run()