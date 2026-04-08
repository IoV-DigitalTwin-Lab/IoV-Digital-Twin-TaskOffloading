#!/usr/bin/env python3
"""
DRL Policy Agent for IoV Task Offloading

Sits between the External Controller and the RSU:
  Controller writes: task:{id}:tau_candidates   (per-link tau features)
  This agent reads:  task:{id}:tau_candidates + vehicle:{id}:state
  This agent writes: task:{id}:decision          (consumed by RSU checkDecisionMsg)

Pipeline:
  offloading_requests:queue → [ExternalController] → drl_pending:queue
  → [DRLPolicyAgent] → task:{id}:decision → [RSU dispatch to vehicle]

State vector (per candidate, padded to MAX_CANDIDATES):
  tau features  : tau_up, tau_comp, tau_down, tau_total, f_avail_hz
  capacity      : cpu_available, cpu_utilization, mem_available,
                  mem_utilization, queue_length, processing_count
  mobility      : speed

Replace select_action() with a trained model call when ready.
"""

import json
import logging
import os
import time
from typing import Dict, List, Optional

import redis

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# Maximum number of candidates padded into the state vector.
# Must match the value used during DRL training.
MAX_CANDIDATES = 8

# Redis key contract
KEY_TAU_CANDIDATES = "task:{task_id}:tau_candidates"
KEY_VEHICLE_STATE  = "vehicle:{vehicle_id}:state"
KEY_DECISION       = "task:{task_id}:decision"
QUEUE_PENDING      = "drl_pending:queue"
STREAM_DECISIONS   = "drl:decisions:log"

# TTL for decision hashes in Redis (seconds).
# Must outlive the RSU checkDecisionMsg polling window.
DECISION_TTL_S = 600


class DRLPolicyAgent:
    def __init__(self,
                 redis_host: str = 'localhost',
                 redis_port: int = 6379,
                 redis_db: int = 0):
        self.redis = redis.Redis(host=redis_host, port=redis_port, db=redis_db)

    # ------------------------------------------------------------------
    # Redis I/O helpers
    # ------------------------------------------------------------------

    def read_tau_candidates(self, task_id: str) -> Optional[Dict]:
        """Read tau candidate hash written by ExternalController."""
        key = KEY_TAU_CANDIDATES.format(task_id=task_id)
        raw = self.redis.hgetall(key)
        if not raw:
            return None
        data = {k.decode(): v.decode() for k, v in raw.items()}
        try:
            data['candidates'] = json.loads(data.get('candidates_json', '[]'))
        except (json.JSONDecodeError, KeyError):
            data['candidates'] = []
        return data

    def read_vehicle_state(self, vehicle_id: str) -> Dict[str, float]:
        """Read vehicle state hash written by RSU/DigitalTwin."""
        key = KEY_VEHICLE_STATE.format(vehicle_id=vehicle_id)
        raw = self.redis.hgetall(key)
        if not raw:
            return {}
        state = {}
        for k, v in raw.items():
            try:
                state[k.decode()] = float(v.decode())
            except (ValueError, AttributeError):
                pass
        return state

    def write_decision(self, task_id: str, candidate: Dict, cycle_id: int) -> None:
        """Write decision hash in the format RSU checkDecisionMsg expects."""
        link_type  = candidate.get('link_type', '')
        target_id  = str(candidate.get('target_id', ''))
        decision_type = "SERVICE_VEHICLE" if link_type == "V2V" else "RSU"

        key = KEY_DECISION.format(task_id=task_id)
        payload = {
            'decision_type':    decision_type,
            'target_id':        target_id,
            'confidence_score': f"{candidate.get('confidence', 0.8):.6f}",
            'cycle_id':         str(int(cycle_id)),
            'tau_total':        f"{float(candidate.get('tau_total', 0.0)):.9f}",
            'tau_up':           f"{float(candidate.get('tau_up',    0.0)):.9f}",
            'tau_comp':         f"{float(candidate.get('tau_comp',  0.0)):.9f}",
            'tau_down':         f"{float(candidate.get('tau_down',  0.0)):.9f}",
            'controller_ts':    f"{time.time():.6f}",
        }
        self.redis.hset(key, mapping=payload)
        self.redis.expire(key, DECISION_TTL_S)

        # Persistent audit trail — never expires unless stream maxlen reached.
        self.redis.xadd(
            STREAM_DECISIONS,
            {'task_id': task_id, **payload},
            maxlen=100000,
            approximate=True,
        )

    # ------------------------------------------------------------------
    # Feature assembly
    # ------------------------------------------------------------------

    def _candidate_features(self, c: Dict, vehicle_state: Dict) -> List[float]:
        """Return a fixed-length feature vector for one candidate."""
        return [
            float(c.get('tau_up',          0.0)),
            float(c.get('tau_comp',        0.0)),
            float(c.get('tau_down',        0.0)),
            float(c.get('tau_total',       0.0)),
            float(c.get('f_avail_hz',      1e9)),
            float(vehicle_state.get('cpu_available',    0.0)),
            float(vehicle_state.get('cpu_utilization',  0.0)),
            float(vehicle_state.get('mem_available',    0.0)),
            float(vehicle_state.get('mem_utilization',  0.0)),
            float(vehicle_state.get('queue_length',     0.0)),
            float(vehicle_state.get('processing_count', 0.0)),
            float(vehicle_state.get('speed',            0.0)),
        ]

    def assemble_state_vector(self,
                               candidates: List[Dict],
                               vehicle_states: Dict[str, Dict]) -> List[float]:
        """
        Build flat state vector for DRL input.

        Layout: [candidate_0_features, candidate_1_features, ..., padding_zeros]
        Length: MAX_CANDIDATES * len(_candidate_features)
        """
        feature_len = 12  # must match _candidate_features length above
        state: List[float] = []

        for c in candidates[:MAX_CANDIDATES]:
            target_id = str(c.get('target_id', ''))
            vstate = vehicle_states.get(target_id, {})
            state.extend(self._candidate_features(c, vstate))

        # Zero-pad remaining slots
        padding = (MAX_CANDIDATES - min(len(candidates), MAX_CANDIDATES)) * feature_len
        state.extend([0.0] * padding)

        return state  # shape: (MAX_CANDIDATES * feature_len,)

    # ------------------------------------------------------------------
    # Policy
    # ------------------------------------------------------------------

    def select_action(self,
                      state_vector: List[float],
                      candidates: List[Dict]) -> int:
        """
        Select the index of the best candidate.

        PLACEHOLDER: picks the candidate with lowest tau_total.
        Replace the body of this method with a real DRL model call:

            obs = torch.tensor(state_vector, dtype=torch.float32)
            action, _ = self.model.predict(obs, deterministic=True)
            return int(action)
        """
        if not candidates:
            return 0

        best_idx = min(range(len(candidates)),
                       key=lambda i: float(candidates[i].get('tau_total', float('inf'))))
        return best_idx

    # ------------------------------------------------------------------
    # Main loop
    # ------------------------------------------------------------------

    def run(self) -> None:
        logger.info("DRL Policy Agent started (queue=%s)", QUEUE_PENDING)

        while True:
            try:
                raw = self.redis.lpop(QUEUE_PENDING)
                if not raw:
                    time.sleep(0.05)
                    continue

                task_id = raw.decode(errors='ignore')

                # 1. Load tau candidates written by the controller
                tau_data = self.read_tau_candidates(task_id)
                if not tau_data or not tau_data.get('candidates'):
                    logger.warning("No tau candidates found for task %s — discarding", task_id)
                    continue

                candidates   = tau_data['candidates']
                cycle_id     = int(tau_data.get('cycle_id', 0))

                # 2. Load vehicle state for each candidate target
                vehicle_states: Dict[str, Dict] = {}
                for c in candidates:
                    vid = str(c.get('target_id', ''))
                    if vid and vid not in vehicle_states:
                        vehicle_states[vid] = self.read_vehicle_state(vid)

                # 3. Build state vector
                state_vector = self.assemble_state_vector(candidates, vehicle_states)

                # 4. Select action (placeholder → replace with model.predict)
                action = self.select_action(state_vector, candidates)
                chosen = candidates[action]

                # 5. Write decision for RSU to consume
                self.write_decision(task_id, chosen, cycle_id)

                logger.info(
                    "decision task=%s type=%s target=%s tau=%.6f (action=%d/%d)",
                    task_id,
                    "SERVICE_VEHICLE" if chosen.get('link_type') == "V2V" else "RSU",
                    chosen.get('target_id'),
                    float(chosen.get('tau_total', 0.0)),
                    action,
                    len(candidates),
                )

            except Exception as e:
                logger.error("DRL agent error: %s", e)
                time.sleep(5)


if __name__ == "__main__":
    agent = DRLPolicyAgent()
    agent.run()
