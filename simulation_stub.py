#!/usr/bin/env python3
"""
simulation_stub.py
==================
IoV Task Offloading Simulation Stub for end-to-end DRL pipeline testing.

Mimics what the OMNeT++ / Veins / INET simulation does with Redis:
  1. Continuously publishes vehicle states, RSU resource states, and SINR values.
  2. Periodically generates task offloading requests and pushes them to the DRL queue.
  3. Polls for the DRL's offloading decision (which node to offload to).
  4. Simulates task execution on the chosen node and writes results back to Redis,
     including processing_time, total_latency, status (COMPLETED_ON_TIME /
     COMPLETED_LATE / FAILED), and an energy placeholder.

Redis Key Schema (identical to IoV-Digital-Twin-TaskOffloading C++ implementation):
    vehicle:{id}:state          HASH  — position, speed, resources, SINR
    rsu:{id}:resources          HASH  — CPU/memory availability, queue, SINR
    rsu:{id}:metadata           HASH  — static: position, totals, bandwidth
    service_vehicles:available  ZSET  — sorted by cpu_score for fast DRL lookup
    offloading_requests:queue   LIST  — task IDs awaiting DRL decision
    task:{id}:request           HASH  — full task parameters
    task:{id}:decision          HASH  — DRL's chosen offloading target (written by DRL)
    task:{id}:state             HASH  — execution result (written by this stub)

Usage:
    python simulation_stub.py [--config stub_config.json]
    python simulation_stub.py --redis-host 127.0.0.1 --redis-port 6379 --vehicles 30
"""

import argparse
import json
import logging
import math
import os
import random
import signal
import threading
import time
from dataclasses import dataclass
from typing import Dict, List, Optional

import redis

# ─────────────────────────────────────────────────────────────────────────────
# LOGGING
# ─────────────────────────────────────────────────────────────────────────────

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(threadName)-18s] %(levelname)-8s %(message)s',
)
log = logging.getLogger('SimStub')

# ─────────────────────────────────────────────────────────────────────────────
# DEFAULT CONFIGURATION  (overridden by stub_config.json or CLI flags)
# ─────────────────────────────────────────────────────────────────────────────

DEFAULT_CONFIG: dict = {
    "redis": {
        "host": "127.0.0.1",
        "port": 6379,
    },
    "network": {
        "num_rsus": 3,
        "rsu_spacing_m": 1000.0,       # Distance between adjacent RSUs
        "rsu_start_x_m": 500.0,        # X position of RSU_0
        "rsu_range_m": 500.0,          # Coverage radius (meters)
        "rsu_cpu_total_mips": 10000.0, # RSU compute capacity (MIPS)
        "rsu_memory_total_mb": 10000.0,
        "rsu_bandwidth_mbps": 100.0,
        "num_vehicles": 20,
        "map_width_m": 3000.0,
        "map_height_m": 200.0,         # ±100 m from y=0
        "min_speed_mps": 5.0,
        "max_speed_mps": 25.0,
        "vehicle_cpu_max_mips": 4000.0,
        "vehicle_mem_max_mb": 4096.0,
        "vehicle_cpu_util_range": [0.1, 0.7],
        "vehicle_mem_util_range": [0.2, 0.8],
    },
    "task": {
        "generation_interval_s": 0.5,
        # Raw byte sizes pushed to Redis (matches OMNeT++ pushOffloadingRequest)
        "mem_footprint_bytes_range": [524288, 10485760],   # 0.5 MB – 10 MB
        "cpu_cycles_range":          [100000000, 1000000000],  # 100 M – 1 B cycles
        "deadline_range_s":          [1.0, 5.0],
        "qos_range":                 [1.0, 3.0],
    },
    "timing": {
        "state_update_interval_s": 0.2,   # How often vehicle/RSU states are refreshed
        "decision_poll_interval_s": 0.05, # How often to check for a DRL decision
        "decision_timeout_s": 10.0,       # Give up waiting for DRL after this long
        "physics_dt_s": 0.2,              # Simulation time step
    },
    "sinr": {
        # Free-space path-loss model: SINR(dB) = P_tx - PL(d) - noise_floor
        # PL(d) = 10 * n * log10(d / d0)
        "tx_power_dbm": 23.0,
        "path_loss_exponent": 3.5,
        "reference_distance_m": 10.0,
        "noise_floor_dbm": -95.0,
        "min_sinr_db": -10.0,
        "max_sinr_db": 35.0,
    },
}

# ─────────────────────────────────────────────────────────────────────────────
# DATA CLASSES
# ─────────────────────────────────────────────────────────────────────────────

@dataclass
class VehicleState:
    vehicle_id: str
    pos_x: float
    pos_y: float
    speed: float
    heading: float          # degrees (0° = east, 180° = west)
    cpu_total: float        # MIPS
    mem_total: float        # MB
    cpu_utilization: float  # 0.0 – 1.0
    mem_utilization: float  # 0.0 – 1.0
    queue_length: int = 0
    processing_count: int = 0

    @property
    def cpu_available(self) -> float:
        return self.cpu_total * (1.0 - self.cpu_utilization)

    @property
    def mem_available(self) -> float:
        return self.mem_total * (1.0 - self.mem_utilization)


@dataclass
class RSUState:
    rsu_id: str
    pos_x: float
    pos_y: float
    cpu_total: float        # MIPS
    memory_total: float     # MB
    bandwidth_mbps: float
    cpu_utilization: float = 0.0
    memory_utilization: float = 0.0
    queue_length: int = 0
    processing_count: int = 0

    @property
    def cpu_available(self) -> float:
        return self.cpu_total * (1.0 - self.cpu_utilization)

    @property
    def memory_available(self) -> float:
        return self.memory_total * (1.0 - self.memory_utilization)


@dataclass
class PendingTask:
    task_id: str
    vehicle_id: str
    rsu_id: str
    mem_footprint_bytes: int
    cpu_cycles: int
    deadline_s: float
    qos: float
    request_time: float
    decision_deadline: float  # wall-clock time after which we declare timeout


# ─────────────────────────────────────────────────────────────────────────────
# SINR CALCULATION
# ─────────────────────────────────────────────────────────────────────────────

def compute_sinr_db(distance_m: float, sinr_cfg: dict) -> float:
    """
    Compute channel SINR (dB) using a simple log-distance path-loss model.
    Clamped to [min_sinr_db, max_sinr_db].
    """
    d0 = sinr_cfg["reference_distance_m"]
    n  = sinr_cfg["path_loss_exponent"]
    if distance_m <= 0:
        return sinr_cfg["max_sinr_db"]
    pl    = 10.0 * n * math.log10(max(distance_m, d0) / d0)
    sinr  = sinr_cfg["tx_power_dbm"] - pl - sinr_cfg["noise_floor_dbm"]
    return max(sinr_cfg["min_sinr_db"], min(sinr_cfg["max_sinr_db"], sinr))


# ─────────────────────────────────────────────────────────────────────────────
# SIMULATION STUB
# ─────────────────────────────────────────────────────────────────────────────

class SimulationStub:
    """
    Simulates the OMNeT++ side of the IoV task offloading pipeline by:
      - Continuously maintaining vehicle and RSU state in Redis
      - Generating task requests for the DRL to decide on
      - Fetching DRL decisions and reporting execution outcomes
    """

    def __init__(self, config: dict):
        self.cfg       = config
        self.net       = config["network"]
        self.task_cfg  = config["task"]
        self.timing    = config["timing"]
        self.sinr_cfg  = config["sinr"]

        self.r = redis.Redis(
            host=config["redis"]["host"],
            port=config["redis"]["port"],
            decode_responses=True,
        )
        self.r.ping()
        log.info("✓ Redis connected at %s:%d",
                 config["redis"]["host"], config["redis"]["port"])

        self.sim_time: float = 0.0
        self._stop_event     = threading.Event()
        self._lock           = threading.Lock()

        # Tasks generated but not yet handed off to a handler thread
        self._pending: Dict[str, PendingTask] = {}

        self.rsus:     List[RSUState]     = self._init_rsus()
        self.vehicles: List[VehicleState] = self._init_vehicles()

        self._publish_rsu_metadata()

    # ─────────────────────────────────────────────────────────────────────────
    # INITIALIZATION
    # ─────────────────────────────────────────────────────────────────────────

    def _init_rsus(self) -> List[RSUState]:
        rsus = []
        for i in range(self.net["num_rsus"]):
            rsu = RSUState(
                rsu_id=f"RSU_{i}",
                pos_x=self.net["rsu_start_x_m"] + i * self.net["rsu_spacing_m"],
                pos_y=0.0,
                cpu_total=self.net["rsu_cpu_total_mips"],
                memory_total=self.net["rsu_memory_total_mb"],
                bandwidth_mbps=self.net["rsu_bandwidth_mbps"],
                cpu_utilization=random.uniform(0.1, 0.4),
                memory_utilization=random.uniform(0.1, 0.3),
                queue_length=random.randint(0, 5),
                processing_count=random.randint(0, 3),
            )
            rsus.append(rsu)
        log.info("Initialized %d RSUs", len(rsus))
        return rsus

    def _init_vehicles(self) -> List[VehicleState]:
        vehicles = []
        for i in range(self.net["num_vehicles"]):
            cpu_total = random.uniform(1000.0, self.net["vehicle_cpu_max_mips"])
            mem_total = random.uniform(1024.0, self.net["vehicle_mem_max_mb"])
            vehicles.append(VehicleState(
                vehicle_id=f"vehicle_{i}",
                pos_x=random.uniform(0, self.net["map_width_m"]),
                pos_y=random.uniform(
                    -self.net["map_height_m"] / 2,
                     self.net["map_height_m"] / 2),
                speed=random.uniform(
                    self.net["min_speed_mps"], self.net["max_speed_mps"]),
                heading=random.choice([0.0, 180.0]) + random.uniform(-5.0, 5.0),
                cpu_total=cpu_total,
                mem_total=mem_total,
                cpu_utilization=random.uniform(*self.net["vehicle_cpu_util_range"]),
                mem_utilization=random.uniform(*self.net["vehicle_mem_util_range"]),
                queue_length=random.randint(0, 3),
                processing_count=random.randint(0, 2),
            ))
        log.info("Initialized %d vehicles", len(vehicles))
        return vehicles

    def _publish_rsu_metadata(self):
        """Write static RSU metadata (positions, total capacities) once at startup."""
        pipe = self.r.pipeline()
        for rsu in self.rsus:
            key = f"rsu:{rsu.rsu_id}:metadata"
            pipe.hset(key, mapping={
                "rsu_id":        rsu.rsu_id,
                "pos_x":         rsu.pos_x,
                "pos_y":         rsu.pos_y,
                "cpu_total":     rsu.cpu_total,
                "memory_total":  rsu.memory_total,
                "bandwidth":     rsu.bandwidth_mbps,
            })
        pipe.execute()
        log.info("✓ RSU metadata published (%d RSUs)", len(self.rsus))

    # ─────────────────────────────────────────────────────────────────────────
    # PHYSICS
    # ─────────────────────────────────────────────────────────────────────────

    def _update_physics(self):
        """Advance vehicle kinematics and drift RSU/vehicle loads."""
        dt = self.timing["physics_dt_s"]
        w  = self.net["map_width_m"]
        h  = self.net["map_height_m"] / 2.0

        for v in self.vehicles:
            rad = math.radians(v.heading)
            v.pos_x += v.speed * math.cos(rad) * dt
            v.pos_y += v.speed * math.sin(rad) * dt
            # Wrap-around highway
            if v.pos_x > w: v.pos_x -= w
            if v.pos_x < 0: v.pos_x += w
            v.pos_y = max(-h, min(h, v.pos_y))
            # Slowly drift utilization
            v.cpu_utilization = max(0.05, min(0.95,
                v.cpu_utilization + random.uniform(-0.02, 0.02)))
            v.mem_utilization = max(0.05, min(0.95,
                v.mem_utilization + random.uniform(-0.01, 0.01)))

        for rsu in self.rsus:
            rsu.cpu_utilization    = max(0.05, min(0.90,
                rsu.cpu_utilization    + random.uniform(-0.03, 0.03)))
            rsu.memory_utilization = max(0.05, min(0.80,
                rsu.memory_utilization + random.uniform(-0.01, 0.01)))
            rsu.queue_length     = max(0, rsu.queue_length     + random.randint(-1, 1))
            rsu.processing_count = max(0, rsu.processing_count + random.randint(-1, 1))

        self.sim_time += dt

    # ─────────────────────────────────────────────────────────────────────────
    # REDIS STATE PUBLISHING
    # ─────────────────────────────────────────────────────────────────────────

    def _publish_vehicle_states(self):
        """Batch-write all vehicle states to Redis."""
        pipe = self.r.pipeline()
        for v in self.vehicles:
            # SINR to the nearest RSU
            nearest = min(
                self.rsus,
                key=lambda r: (v.pos_x - r.pos_x) ** 2 + (v.pos_y - r.pos_y) ** 2,
            )
            dist = math.sqrt((v.pos_x - nearest.pos_x) ** 2 +
                             (v.pos_y - nearest.pos_y) ** 2)
            sinr = compute_sinr_db(dist, self.sinr_cfg)

            key = f"vehicle:{v.vehicle_id}:state"
            pipe.hset(key, mapping={
                "pos_x":            round(v.pos_x, 2),
                "pos_y":            round(v.pos_y, 2),
                "speed":            round(v.speed, 2),
                "heading":          round(v.heading, 2),
                "cpu_available":    round(v.cpu_available, 2),
                "cpu_utilization":  round(v.cpu_utilization, 4),
                "mem_available":    round(v.mem_available, 2),
                "mem_utilization":  round(v.mem_utilization, 4),
                "queue_length":     v.queue_length,
                "processing_count": v.processing_count,
                "sinr":             round(sinr, 2),
                "last_update":      round(self.sim_time, 3),
            })
            pipe.expire(key, 30)  # auto-expire if not refreshed (vehicle left sim)

            # Service-vehicle sorted set: score = effective CPU score
            cpu_score = v.cpu_available * (1.0 - v.cpu_utilization)
            pipe.zadd("service_vehicles:available", {v.vehicle_id: cpu_score})

        pipe.execute()

    def _publish_rsu_states(self):
        """Batch-write all RSU resource states to Redis."""
        pipe = self.r.pipeline()
        for rsu in self.rsus:
            # Compute average SINR across vehicles in range (channel quality indicator)
            sinr_samples = []
            for v in self.vehicles:
                dist = math.sqrt((v.pos_x - rsu.pos_x) ** 2 +
                                 (v.pos_y - rsu.pos_y) ** 2)
                if dist <= self.net["rsu_range_m"]:
                    sinr_samples.append(compute_sinr_db(dist, self.sinr_cfg))
            avg_sinr = (sum(sinr_samples) / len(sinr_samples)) if sinr_samples else 0.0

            key = f"rsu:{rsu.rsu_id}:resources"
            pipe.hset(key, mapping={
                "cpu_available":    round(rsu.cpu_available, 2),
                "cpu_total":        rsu.cpu_total,       # needed for util calc in DRL env
                "memory_available": round(rsu.memory_available, 2),
                "memory_total":     rsu.memory_total,
                "queue_length":     rsu.queue_length,
                "processing_count": rsu.processing_count,
                "cpu_utilization":  round(rsu.cpu_utilization, 4),
                "pos_x":            rsu.pos_x,
                "pos_y":            rsu.pos_y,
                "sinr":             round(avg_sinr, 2),  # avg channel quality
                "update_time":      round(self.sim_time, 3),
            })
        pipe.execute()

    # ─────────────────────────────────────────────────────────────────────────
    # TASK GENERATION
    # ─────────────────────────────────────────────────────────────────────────

    def _nearest_rsu_in_range(self, v: VehicleState) -> Optional[RSUState]:
        """Return the closest RSU within coverage range, or None."""
        best, best_dist = None, float("inf")
        for rsu in self.rsus:
            dist = math.sqrt((v.pos_x - rsu.pos_x) ** 2 +
                             (v.pos_y - rsu.pos_y) ** 2)
            if dist <= self.net["rsu_range_m"] and dist < best_dist:
                best, best_dist = rsu, dist
        return best

    def _generate_task(self) -> Optional[str]:
        """
        Pick a random vehicle in RSU range, generate a task offloading request,
        write it to Redis, and push the task_id to the DRL request queue.
        Returns the task_id or None if no vehicle is in range.
        """
        in_range = [v for v in self.vehicles
                    if self._nearest_rsu_in_range(v) is not None]
        if not in_range:
            log.debug("No vehicles in RSU range — skipping task generation")
            return None

        vehicle = random.choice(in_range)
        rsu     = self._nearest_rsu_in_range(vehicle)

        task_id     = f"task_{int(self.sim_time * 1000):010d}_{random.randint(1000, 9999)}"
        mem_bytes   = random.randint(*self.task_cfg["mem_footprint_bytes_range"])
        cpu_cycles  = random.randint(*self.task_cfg["cpu_cycles_range"])
        deadline_s  = round(random.uniform(*self.task_cfg["deadline_range_s"]), 2)
        qos         = round(random.uniform(*self.task_cfg["qos_range"]), 1)
        req_time    = time.time()

        req_key = f"task:{task_id}:request"
        self.r.hset(req_key, mapping={
            "task_id":             task_id,
            "vehicle_id":          vehicle.vehicle_id,
            "rsu_id":              rsu.rsu_id,
            "mem_footprint_bytes": mem_bytes,
            "cpu_cycles":          cpu_cycles,
            "deadline_seconds":    deadline_s,
            "qos_value":           qos,
            "request_time":        req_time,
        })
        self.r.expire(req_key, 120)

        # Push to DRL request queue (DRL does blpop on this list)
        self.r.rpush("offloading_requests:queue", task_id)

        with self._lock:
            self._pending[task_id] = PendingTask(
                task_id=task_id,
                vehicle_id=vehicle.vehicle_id,
                rsu_id=rsu.rsu_id,
                mem_footprint_bytes=mem_bytes,
                cpu_cycles=cpu_cycles,
                deadline_s=deadline_s,
                qos=qos,
                request_time=req_time,
                decision_deadline=time.time() + self.timing["decision_timeout_s"],
            )

        log.info("→ Task %s | vehicle=%s rsu=%s cpu=%dM deadline=%.1fs qos=%.1f",
                 task_id, vehicle.vehicle_id, rsu.rsu_id,
                 cpu_cycles // 1_000_000, deadline_s, qos)
        return task_id

    # ─────────────────────────────────────────────────────────────────────────
    # EXECUTION SIMULATION + RESULT REPORTING
    # ─────────────────────────────────────────────────────────────────────────

    def _simulate_execution(
        self, task: PendingTask, decision: dict
    ) -> dict:
        """
        Simulate task execution on the DRL-chosen node.
        Returns a result dict matching what RedisDigitalTwin::updateTaskCompletion writes.
        """
        decision_type = decision.get("decision_type", "RSU")
        target_id     = decision.get("target_id", task.rsu_id)

        if decision_type == "RSU":
            node_rsu = next(
                (r for r in self.rsus if r.rsu_id == target_id), self.rsus[0]
            )
            cpu_hz   = max(node_rsu.cpu_available * 1e6, 1.0)   # MIPS → Hz
            # Transmission over RSU–vehicle V2I link (higher bandwidth)
            tx_time  = (task.mem_footprint_bytes * 8) / (node_rsu.bandwidth_mbps * 1e6)
            overhead = random.uniform(0.005, 0.03)
        else:
            # SERVICE_VEHICLE: task is relayed through RSU then processed on target vehicle
            node_v   = next(
                (v for v in self.vehicles if v.vehicle_id == target_id), None
            )
            cpu_hz   = max(node_v.cpu_available * 1e6, 1.0) if node_v else 500e6
            # V2V relay adds latency (lower effective bandwidth)
            tx_time  = (task.mem_footprint_bytes * 8) / 50e6   # ~50 Mbps V2V
            overhead = random.uniform(0.02, 0.10)

        proc_time     = task.cpu_cycles / cpu_hz
        total_latency = (tx_time + proc_time + overhead) * random.uniform(0.9, 1.1)

        completed_on_time = total_latency <= task.deadline_s
        status = "COMPLETED_ON_TIME" if completed_on_time else "COMPLETED_LATE"

        # Energy placeholder (nJ/cycle) — will be replaced by real simulator values later
        energy_j = task.cpu_cycles * 1e-9

        return {
            "status":          status,
            "decision_type":   decision_type,
            "processor_id":    target_id,
            "processing_time": round(proc_time, 4),
            "total_latency":   round(total_latency, 4),
            "completion_time": round(time.time() - task.request_time, 4),
            "energy_j":        round(energy_j, 6),   # placeholder for future use
        }

    def _handle_task(self, task_id: str, task: PendingTask):
        """
        Worker thread: poll for multi-agent DRL decisions, deduplicate simulation,
        and write per-agent results to Redis.

        Decision schema (written atomically by IoVRedisEnv.step_multi):
            task:{id}:decisions  HASH
                agents           → comma-separated agent names
                {agent}_type     → "RSU" or "SERVICE_VEHICLE"
                {agent}_target   → target node id

        Result schema (read by IoVRedisEnv._wait_for_multi_results):
            task:{id}:results  HASH
                {agent}_status   → COMPLETED_ON_TIME | COMPLETED_LATE | FAILED
                {agent}_latency  → float seconds
                {agent}_energy   → float joules
        """
        decisions_key = f"task:{task_id}:decisions"
        results_key   = f"task:{task_id}:results"
        poll_interval = self.timing["decision_poll_interval_s"]

        while time.time() < task.decision_deadline:
            if self._stop_event.is_set():
                return

            data = self.r.hgetall(decisions_key)
            agents_str = data.get('agents', '')
            if not agents_str:
                time.sleep(poll_interval)
                continue

            agent_names = agents_str.split(',')
            agent_decisions = {}
            for agent in agent_names:
                dtype = data.get(f'{agent}_type', '')
                tid   = data.get(f'{agent}_target', '')
                if dtype and tid:
                    agent_decisions[agent] = (dtype, tid)

            if len(agent_decisions) < len(agent_names):
                # Decisions not fully written yet (race guard)
                time.sleep(poll_interval)
                continue

            # ── Dedup: simulate each unique (type, target) only once ──────────
            target_to_result = {}
            for dtype, tid in set(agent_decisions.values()):
                target_to_result[(dtype, tid)] = self._simulate_execution(
                    task, {'decision_type': dtype, 'target_id': tid}
                )

            # ── Map results back to each agent ────────────────────────────────
            result_mapping = {}
            for agent, (dtype, tid) in agent_decisions.items():
                r = target_to_result[(dtype, tid)]
                result_mapping[f'{agent}_status']  = r['status']
                result_mapping[f'{agent}_latency'] = r['total_latency']
                result_mapping[f'{agent}_energy']  = r['energy_j']

            self.r.hset(results_key, mapping=result_mapping)
            self.r.expire(results_key, 120)

            # Log only the ddqn decision to keep output clean
            ddqn_dtype, ddqn_tid = agent_decisions.get('ddqn', next(iter(agent_decisions.values())))
            ddqn_r = target_to_result[(ddqn_dtype, ddqn_tid)]
            log.info("← %s | %s → %s | lat=%.3fs deadline=%.1fs [%s]",
                     task_id, ddqn_dtype, ddqn_tid,
                     ddqn_r['total_latency'], task.deadline_s, ddqn_r['status'])
            return

        # Decision timeout: write a sentinel so DRL side can detect it
        self.r.hset(results_key, mapping={'_timeout': '1'})
        self.r.expire(results_key, 120)
        log.warning("✗ Timeout: no multi-agent decisions for task %s", task_id)

    # ─────────────────────────────────────────────────────────────────────────
    # BACKGROUND THREADS
    # ─────────────────────────────────────────────────────────────────────────

    def _state_update_loop(self):
        """Periodically advance physics and publish vehicle/RSU states to Redis."""
        interval = self.timing["state_update_interval_s"]
        log.info("[StateUpdater] started — update interval %.2fs", interval)
        while not self._stop_event.is_set():
            try:
                self._update_physics()
                self._publish_vehicle_states()
                self._publish_rsu_states()
            except Exception as exc:
                log.error("[StateUpdater] %s", exc)
            time.sleep(interval)

    def _task_generator_loop(self):
        """Periodically generate and push new task offloading requests."""
        interval = self.task_cfg["generation_interval_s"]
        log.info("[TaskGenerator] started — task interval %.2fs", interval)
        while not self._stop_event.is_set():
            try:
                self._generate_task()
            except Exception as exc:
                log.error("[TaskGenerator] %s", exc)
            time.sleep(interval)

    def _decision_dispatcher_loop(self):
        """
        Dispatch a short-lived handler thread for each newly pending task.
        Each handler independently polls for the DRL decision and writes the result.
        """
        log.info("[DecisionDisp] started")
        while not self._stop_event.is_set():
            with self._lock:
                snapshot = dict(self._pending)
                for task_id in snapshot:
                    self._pending.pop(task_id, None)

            for task_id, task in snapshot.items():
                t = threading.Thread(
                    target=self._handle_task,
                    args=(task_id, task),
                    name=f"T-{task_id[-8:]}",
                    daemon=True,
                )
                t.start()

            time.sleep(0.05)

    # ─────────────────────────────────────────────────────────────────────────
    # PUBLIC INTERFACE
    # ─────────────────────────────────────────────────────────────────────────

    def start(self):
        """Start all background threads and block until stopped."""
        log.info("═══ IoV Simulation Stub Starting ═══")
        log.info("  RSUs: %d  |  Vehicles: %d  |  Task interval: %.2fs",
                 len(self.rsus), len(self.vehicles),
                 self.task_cfg["generation_interval_s"])

        threads = [
            threading.Thread(target=self._state_update_loop,
                             name="StateUpdater", daemon=True),
            threading.Thread(target=self._task_generator_loop,
                             name="TaskGenerator", daemon=True),
            threading.Thread(target=self._decision_dispatcher_loop,
                             name="DecisionDisp", daemon=True),
        ]
        for t in threads:
            t.start()

        try:
            while not self._stop_event.is_set():
                time.sleep(0.5)
        except KeyboardInterrupt:
            self.stop()

    def stop(self):
        log.info("Stopping simulation stub...")
        self._stop_event.set()


# ─────────────────────────────────────────────────────────────────────────────
# CONFIG LOADING
# ─────────────────────────────────────────────────────────────────────────────

def _deep_update(base: dict, override: dict) -> dict:
    for key, val in override.items():
        if isinstance(val, dict) and isinstance(base.get(key), dict):
            _deep_update(base[key], val)
        else:
            base[key] = val
    return base


def load_config(path: Optional[str]) -> dict:
    import copy
    cfg = copy.deepcopy(DEFAULT_CONFIG)
    if path and os.path.exists(path):
        with open(path) as f:
            override = json.load(f)
        _deep_update(cfg, override)
        log.info("Loaded config from %s", path)
    else:
        log.info("Using default configuration (no config file found at %s)",
                 path or "stub_config.json")
    return cfg


# ─────────────────────────────────────────────────────────────────────────────
# ENTRY POINT
# ─────────────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="IoV Task Offloading Simulation Stub",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--config", default="stub_config.json",
        help="Path to stub configuration JSON (overrides DEFAULT_CONFIG)")
    parser.add_argument("--redis-host", help="Override Redis host")
    parser.add_argument("--redis-port", type=int, help="Override Redis port")
    parser.add_argument("--vehicles",   type=int, help="Override number of vehicles")
    parser.add_argument("--rsus",       type=int, help="Override number of RSUs")
    parser.add_argument("--task-interval", type=float,
                        help="Override task generation interval (seconds)")
    args = parser.parse_args()

    config = load_config(args.config)
    if args.redis_host:    config["redis"]["host"]                    = args.redis_host
    if args.redis_port:    config["redis"]["port"]                    = args.redis_port
    if args.vehicles:      config["network"]["num_vehicles"]          = args.vehicles
    if args.rsus:          config["network"]["num_rsus"]              = args.rsus
    if args.task_interval: config["task"]["generation_interval_s"]    = args.task_interval

    stub = SimulationStub(config)
    # Handle Docker/systemd SIGTERM gracefully
    signal.signal(signal.SIGTERM, lambda *_: stub.stop())
    stub.start()
