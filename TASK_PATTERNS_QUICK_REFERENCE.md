# Task Generation Pattern Summary - Quick Reference

## Distribution Summary

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        TASK GENERATION PATTERNS                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  PERIODIC TASKS (Deterministic, Fixed Interval)                            │
│  ═══════════════════════════════════════════════════════════                │
│                                                                              │
│  📊 LOCAL_OBJECT_DETECTION                                                  │
│     Interval: 50 ms (20 Hz)      |  Pattern: T(n) = n * 50ms              │
│     Input: 2 MB  |  CPU: 5×10⁸ cycles  |  Deadline: 50ms                  │
│     🔴 SAFETY CRITICAL - Cannot offload                                     │
│                                                                              │
│  📊 COOPERATIVE_PERCEPTION                                                  │
│     Interval: 100 ms (10 Hz)     |  Pattern: T(n) = n * 100ms             │
│     Input: 0.3 MB  |  CPU: 2.5×10⁹ cycles  |  Deadline: 150ms             │
│     ✅ Can offload - High priority                                          │
│     ↓ Spawns: ROUTE_OPTIMIZATION, TRAFFIC_PREDICTION                       │
│                                                                              │
│  📊 ROUTE_OPTIMIZATION                                                      │
│     Interval: 500 ms (2 Hz)      |  Pattern: T(n) = n * 500ms             │
│     Input: 1 MB  |  CPU: 5×10⁹ cycles  |  Deadline: 1s                    │
│     ✅ Can offload - Medium priority                                        │
│                                                                              │
│  📊 SENSOR_DIAGNOSTICS                                                      │
│     Interval: 10 s               |  Pattern: T(n) = n * 10s                │
│     Input: 0.1 MB  |  CPU: 1×10⁸ cycles  |  Deadline: 10s                 │
│     ✅ Can offload - Low priority                                           │
│                                                                              │
│  📊 INFOTAINMENT_UPDATE                                                     │
│     Interval: 30 s               |  Pattern: T(n) = n * 30s                │
│     Input: 0.5 MB  |  CPU: 5×10⁸ cycles  |  Deadline: 30s                 │
│     ✅ Can offload - Low priority                                           │
│                                                                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  POISSON TASKS (Random Arrival, Exponential Inter-arrival)                 │
│  ═════════════════════════════════════════════════════════════              │
│  Formula: P(T ≤ t) = 1 - e^(-λt)   where λ = rate parameter               │
│                                                                              │
│  🎲 VOICE_ASSISTANT_PROCESSING                                              │
│     λ = 0.2 tasks/sec (mean: 5s between tasks)                             │
│     Input: 0.2 MB  |  CPU: 1×10⁹ cycles  |  Deadline: 500ms               │
│     ✅ Can offload - Medium priority (interactive)                          │
│     Sample arrival times: 2.3s, 5.1s, 12.4s, 14.2s, ...                   │
│                                                                              │
│  🎲 ROUTE_ALTERNATIVE_CALC                                                  │
│     λ = 0.05 tasks/sec (mean: 20s between tasks)                           │
│     Input: 1.5 MB  |  CPU: 8×10⁹ cycles  |  Deadline: 2s                  │
│     ✅ Can offload - Medium-low priority (user-driven)                      │
│                                                                              │
│  🎲 TRAFFIC_PREDICTION                                                      │
│     λ = 0.1 tasks/sec (mean: 10s between tasks)                            │
│     Input: 10 MB  |  CPU: 2×10¹⁰ cycles  |  Deadline: 5 min (fluid)       │
│     ✅ MUST offload - Heavy ML workload, low priority                       │
│                                                                              │
│  🎲 WEATHER_FETCH_AND_PROCESS                                               │
│     λ = 0.0167 tasks/sec (mean: 60s between tasks)                         │
│     Input: 0.3 MB  |  CPU: 2×10⁸ cycles  |  Deadline: 60s                 │
│     ✅ Can offload - Low priority (informational)                           │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Task Load Timeline Example

```
Timeline: Rush Hour Vehicle Workload (first 10 seconds)

0.0s  |🔴 LOCAL_OBJ_DET_1 starts
0.05s |🔴 LOCAL_OBJ_DET_1 ends → 🔴 LOCAL_OBJ_DET_2 starts
       |🎲 VOICE_ASSIST_1 arrives (Poisson)
0.1s  |🔴 LOCAL_OBJ_DET_2 ends → 🔴 LOCAL_OBJ_DET_3 starts
       |📊 COOP_PERCEPTION_1 starts (depends on LOCAL_OBJ_DET)
0.15s |🔴 LOCAL_OBJ_DET_3 ends → 🔴 LOCAL_OBJ_DET_4 starts
       |📊 COOP_PERCEPTION_1 ends → spawns ROUTE_OPT_1
0.2s  |🔴 LOCAL_OBJ_DET_4 ends → 🔴 LOCAL_OBJ_DET_5 starts
0.25s |🔴 LOCAL_OBJ_DET_5 ends → 🔴 LOCAL_OBJ_DET_6 starts
       |🎲 VOICE_ASSIST_1 ends (completed locally)
0.5s  |🔴 LOCAL_OBJ_DET_10 starts
       |📊 ROUTE_OPTIMIZATION_1 starts (deadline: 1.5s)
       |📊 SENSOR_DIAGNOSTICS_1 arrives (periodic)
       |🎲 VOICE_ASSIST_2 arrives (Poisson)
1.0s  |🔴 LOCAL_OBJ_DET starts cycle #20
       |📊 TRAFFIC_PREDICTION_1 arrives (Poisson, λ=0.1)
       |🎲 ROUTE_ALTERNATIVE_1 arrives (Poisson, λ=0.05)
...
3.0s  |📊 ROUTE_OPTIMIZATION_2 arrives (periodic, 3x so far)
       |🎲 VOICE_ASSIST_3 arrives (Poisson)
...
10.0s |📊 TRAFFIC_PREDICTION_2 arrives (≈1 per 10s)
       |📊 SENSOR_DIAGNOSTICS_2 arrives (≈1 per 10s)
       |📊 WEATHER_FETCH_1 arrives (rare, Poisson)

CPU UTILIZATION DURING THIS PERIOD:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 100% |█████████████████████████████████████████████████  ← SATURATED (peak)
  90% |████████████████████████████████████████████████
  80% |███████████████████████████████████████████████
  70% |██████████████████████████████████████████████
  60% |█████████████████████████████████████████  ← WITHOUT OFFLOADING
  50% |████████████████████████
  40% |███████████████████
  30% |██████████████  ← WITH INTELLIGENT OFFLOADING
  20% |███████████
  10% |█████
   0% └────────────────────────────────────────────────────
     0s       2s       4s       6s       8s       10s
```

## Parameter Usage Example (omnetpp.ini)

```ini
# ============================================================================
# SCENARIO: Rush Hour / Peak Load
# ============================================================================

# Vehicle CPU Resources
*.node[*].appl.cpu_total = 4e9                    # 4 GHz typical
*.node[*].appl.cpu_reservation_ratio = 0.3        # 30% reserved (safety)
*.node[*].appl.memory_total = 2048e6              # 2 GB

# ────────────────────────────────────────────────────────────────────────────
# PERIODIC TASK DEFINITIONS
# ────────────────────────────────────────────────────────────────────────────

# Local Object Detection (MANDATORY, REAL-TIME)
*.node[*].appl.LocalObjDet_enabled = true
*.node[*].appl.LocalObjDet_pattern = "periodic"
*.node[*].appl.LocalObjDet_period = 0.050
*.node[*].appl.LocalObjDet_jitter = 0.01
*.node[*].appl.LocalObjDet_input_size = 2e6
*.node[*].appl.LocalObjDet_cpu_cycles = 5e8
*.node[*].appl.LocalObjDet_deadline = 0.05
*.node[*].appl.LocalObjDet_qos_value = 0.95
*.node[*].appl.LocalObjDet_is_offloadable = false
*.node[*].appl.LocalObjDet_is_safety_critical = true

# Cooperative Perception (PERIODIC, OFFLOADABLE)
*.node[*].appl.CoopPercept_enabled = true
*.node[*].appl.CoopPercept_pattern = "periodic"
*.node[*].appl.CoopPercept_period = 0.100
*.node[*].appl.CoopPercept_jitter = 0.02
*.node[*].appl.CoopPercept_input_size = 0.3e6
*.node[*].appl.CoopPercept_cpu_cycles = 2.5e9
*.node[*].appl.CoopPercept_deadline = 0.15
*.node[*].appl.CoopPercept_qos_value = 0.85
*.node[*].appl.CoopPercept_is_offloadable = true
*.node[*].appl.CoopPercept_is_safety_critical = false
*.node[*].appl.CoopPercept_depends_on = "LOCAL_OBJECT_DETECTION"
*.node[*].appl.CoopPercept_output_type = "perception_map"
*.node[*].appl.CoopPercept_spawn_children = "ROUTE_OPTIMIZATION,TRAFFIC_PREDICTION"

# Route Optimization (PERIODIC, OFFLOADABLE)
*.node[*].appl.RouteOpt_enabled = true
*.node[*].appl.RouteOpt_pattern = "periodic"
*.node[*].appl.RouteOpt_period = 0.500
*.node[*].appl.RouteOpt_jitter = 0.05
*.node[*].appl.RouteOpt_input_size = 1e6
*.node[*].appl.RouteOpt_cpu_cycles = 5e9
*.node[*].appl.RouteOpt_deadline = 1.0
*.node[*].appl.RouteOpt_qos_value = 0.65
*.node[*].appl.RouteOpt_is_offloadable = true
*.node[*].appl.RouteOpt_offloading_benefit = 2.0

# Sensor Diagnostics (PERIODIC, LOW PRIORITY)
*.node[*].appl.SensorDiag_enabled = true
*.node[*].appl.SensorDiag_pattern = "periodic"
*.node[*].appl.SensorDiag_period = 10.0
*.node[*].appl.SensorDiag_jitter = 0.1
*.node[*].appl.SensorDiag_input_size = 0.1e6
*.node[*].appl.SensorDiag_cpu_cycles = 1e8
*.node[*].appl.SensorDiag_deadline = 10.0
*.node[*].appl.SensorDiag_qos_value = 0.30
*.node[*].appl.SensorDiag_is_offloadable = true

# Infotainment Update (PERIODIC, BACKGROUND)
*.node[*].appl.Infotainment_enabled = true
*.node[*].appl.Infotainment_pattern = "periodic"
*.node[*].appl.Infotainment_period = 30.0
*.node[*].appl.Infotainment_jitter = 0.2
*.node[*].appl.Infotainment_input_size = 0.5e6
*.node[*].appl.Infotainment_cpu_cycles = 5e8
*.node[*].appl.Infotainment_deadline = 30.0
*.node[*].appl.Infotainment_qos_value = 0.20
*.node[*].appl.Infotainment_is_offloadable = true

# ────────────────────────────────────────────────────────────────────────────
# POISSON TASK DEFINITIONS
# ────────────────────────────────────────────────────────────────────────────

# Voice Assistant (POISSON, USER-DRIVEN)
*.node[*].appl.VoiceAssist_enabled = true
*.node[*].appl.VoiceAssist_pattern = "poisson"
*.node[*].appl.VoiceAssist_lambda = 0.2
*.node[*].appl.VoiceAssist_input_size = 0.2e6
*.node[*].appl.VoiceAssist_cpu_cycles = 1e9
*.node[*].appl.VoiceAssist_deadline = 0.5
*.node[*].appl.VoiceAssist_qos_value = 0.50
*.node[*].appl.VoiceAssist_is_offloadable = true

# Route Alternatives (POISSON, USER-DRIVEN)
*.node[*].appl.RouteAlt_enabled = true
*.node[*].appl.RouteAlt_pattern = "poisson"
*.node[*].appl.RouteAlt_lambda = 0.05
*.node[*].appl.RouteAlt_input_size = 1.5e6
*.node[*].appl.RouteAlt_cpu_cycles = 8e9
*.node[*].appl.RouteAlt_deadline = 2.0
*.node[*].appl.RouteAlt_qos_value = 0.55
*.node[*].appl.RouteAlt_is_offloadable = true

# Traffic Prediction (POISSON, ANALYTICS - HEAVY)
*.node[*].appl.TrafficPred_enabled = true
*.node[*].appl.TrafficPred_pattern = "poisson"
*.node[*].appl.TrafficPred_lambda = 0.1
*.node[*].appl.TrafficPred_input_size = 10e6
*.node[*].appl.TrafficPred_cpu_cycles = 2e10
*.node[*].appl.TrafficPred_deadline = 300.0
*.node[*].appl.TrafficPred_qos_value = 0.45
*.node[*].appl.TrafficPred_is_offloadable = true
*.node[*].appl.TrafficPred_offloading_benefit = 5.0
*.node[*].appl.TrafficPred_must_offload = true

# Weather Fetch (POISSON, INFORMATIONAL - RARE)
*.node[*].appl.Weather_enabled = true
*.node[*].appl.Weather_pattern = "poisson"
*.node[*].appl.Weather_lambda = 0.0167
*.node[*].appl.Weather_input_size = 0.3e6
*.node[*].appl.Weather_cpu_cycles = 2e8
*.node[*].appl.Weather_deadline = 60.0
*.node[*].appl.Weather_qos_value = 0.25
*.node[*].appl.Weather_is_offloadable = true

# ────────────────────────────────────────────────────────────────────────────
# WORKLOAD SCENARIO MULTIPLIERS
# ────────────────────────────────────────────────────────────────────────────

# Light traffic (50% of normal)
#*.node[*].appl.workload_intensity = 0.5

# Normal traffic (100% - baseline)
*.node[*].appl.workload_intensity = 1.0

# Peak traffic (200% - rush hour)
#*.node[*].appl.workload_intensity = 2.0

# Stress test (500% - highway congestion)
#*.node[*].appl.workload_intensity = 5.0

# ────────────────────────────────────────────────────────────────────────────
# OFFLOADING & LINK QUALITY
# ────────────────────────────────────────────────────────────────────────────

*.node[*].appl.offloadingEnabled = true
*.node[*].appl.rssi_threshold_offloading = -85dBm
*.node[*].appl.min_link_stability = 0.8
*.node[*].appl.max_packet_loss_ratio = 0.1
*.node[*].appl.rsuDecisionTimeout = 1.0s
*.node[*].appl.offloadedTaskTimeout = 5.0s
```

## How to Use This for Different Scenarios

**Scenario 1: Baseline with Cooperative Perception**
```ini
# Enable only periodic tasks for basic demo
CoopPercept_enabled = true
TrafficPred_enabled = false    # Disable heavy ML
VoiceAssist_enabled = false    # Disable user interaction
workload_intensity = 1.0
```

**Scenario 2: Rush Hour Heavy Load**
```ini
# Enable all tasks at full intensity
CoopPercept_enabled = true
TrafficPred_enabled = true
VoiceAssist_enabled = true
workload_intensity = 2.0  # 2x normal Poisson rates
```

**Scenario 3: Stress Test (to show offloading necessity)**
```ini
# All tasks at maximum
CoopPercept_enabled = true
TrafficPred_enabled = true
VoiceAssist_enabled = true
RouteAlt_enabled = true
workload_intensity = 5.0  # 5x normal - vehicles will be SATURATED without offloading
```

