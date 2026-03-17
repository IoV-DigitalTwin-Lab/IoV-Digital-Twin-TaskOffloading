# Task Distribution Summary - Direct Answer

## Which Tasks Use Poisson Distribution?

```
POISSON TASKS (Random Arrival, Suitable for User-Driven & Analytics)
════════════════════════════════════════════════════════════════════

1. ✅ VOICE_ASSISTANT_PROCESSING
   λ = 0.2 tasks/sec (mean 5 seconds between arrivals)
   Reasoning: User voice commands are random, bursty
   Real-world analog: Cellular network call arrivals
   
2. ✅ ROUTE_ALTERNATIVE_CALC  
   λ = 0.05 tasks/sec (mean 20 seconds between arrivals)
   Reasoning: User occasionally requests alternatives
   Real-world analog: User request arrivals to web server
   
3. ✅ TRAFFIC_PREDICTION
   λ = 0.1 tasks/sec (mean 10 seconds between arrivals)
   Reasoning: Analytics algorithms trigger probabilistically
   Real-world analog: Sensor reading queues
   Applied Workload Multiplier: Can scale (1.0x, 2.0x, 5.0x) for stress tests
   
4. ✅ WEATHER_FETCH_AND_PROCESS
   λ = 0.0167 tasks/sec (mean 60 seconds between arrivals)
   Reasoning: Weather updates are infrequent, independent events
   Real-world analog: Weather API request arrivals

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

PERIODIC TASKS (Deterministic Fixed Intervals)
═══════════════════════════════════════════════

1. ⏱️ LOCAL_OBJECT_DETECTION
   Period: 50 ms (20 Hz)
   Reasoning: Synchronized with camera frame rate
   
2. ⏱️ COOPERATIVE_PERCEPTION
   Period: 100 ms (10 Hz)
   Reasoning: V2X message cycle time (802.11p standard)
   
3. ⏱️ ROUTE_OPTIMIZATION
   Period: 500 ms (2 Hz)
   Reasoning: Periodic route recalculation cycle
   
4. ⏱️ SENSOR_DIAGNOSTICS
   Period: 10 s
   Reasoning: Background health checks every 10 seconds
   
5. ⏱️ INFOTAINMENT_UPDATE
   Period: 30 s
   Reasoning: Content refresh every 30 seconds

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Distribution in Simulation (by task count):
  Periodic: 5 types → 13 tasks per second (deterministic timing)
  Poisson: 4 types → ~0.35 tasks/second (with intensity=1.0, variable timing)
  Total: 9 task types generating realistic mixed workload
```

---

## Complete Task Parameter List

### A. TASK IDENTIFICATION (5 parameters)

| # | Parameter Name | Current Status | Type | Value | Notes |
|----|---|---|---|---|---|
| 1 | `task_id` | ✅ EXISTS | String | Auto-generated | "V1_T0_1.234" format |
| 2 | `vehicle_id` | ✅ EXISTS | String | From vehicle | Source vehicle ID |
| 3 | `task_type` | ❌ MISSING | Enum(TaskType) | LOCAL_OBJ_DET, etc. | NEW: differentiates task types |
| 4 | `task_type_name` | ❌ MISSING | String | "Local Object Detection" | Human-readable |
| 5 | `generation_pattern` | ❌ MISSING | Enum(PERIODIC, POISSON) | PERIODIC or POISSON | Determines arrival |

**Existing Code:** [Task.h](Task.h) lines 23-34  
**Modification Needed:** Add TaskType enum, GenerationPattern enum  

---

### B. COMPUTATIONAL CHARACTERISTICS (6 parameters)

**For each task type:**

| # | Parameter Name | Current Status | Type | Range/Value | Example (Coop Perception) |
|----|---|---|---|---|---|
| 6 | `input_size_bytes` | ⚠️ GENERIC | uint64_t | Per-task specific | 0.3 × 10⁶ |
| 7 | `output_size_bytes` | ❌ MISSING | uint64_t | Per-task specific | 0.15 × 10⁶ |
| 8 | `cpu_cycles_required` | ⚠️ GENERIC | uint64_t | Per-task specific | 2.5 × 10⁹ |
| 9 | `memory_peak_bytes` | ❌ MISSING | double | Per-task specific | 50 × 10⁶ |
| 10 | `is_parallelizable` | ❌ MISSING | bool | true/false | true (LSTM can parallelize) |
| 11 | `cpu_efficiency_ratio` | ❌ MISSING | double | 0.0-1.0 | 0.85 (85% of theoretical max) |

**Existing Code:** [omnetpp.ini](omnetpp.ini) lines 211-217 (generic only)  
**Modification Needed:** Create per-task profiles in omnetpp.ini  

---

### C. TIMING & DEADLINE PARAMETERS (4 parameters)

| # | Parameter Name | Current Status | Type | Range/Value | Example |
|----|---|---|---|---|---|
| 12 | `deadline_seconds` | ⚠️ GENERIC | double | Per-task specific | 0.15 |
| 13 | `created_time` | ✅ EXISTS | simtime_t | Auto-assigned | simTime() |
| 14 | `qos_value` | ✅ EXISTS | double | 0.0-1.0 | 0.85 |
| 15 | `priority_level` | ❌ MISSING | Enum(SAFETY, HIGH, MEDIUM, LOW) | Per-task specific | HIGH |

**Existing Code:** [Task.h](Task.h) lines 41-51  
**Modification Needed:** Map qos_value to PriorityLevel enum  

---

### D. GENERATION PATTERN PARAMETERS (6 parameters for Periodic, 3 for Poisson)

**For Periodic Tasks:**

| # | Parameter Name | Current Status | Type | Example (Local Detection) |
|----|---|---|---|---|
| 16 | `period_seconds` | ❌ MISSING | double | 0.050 |
| 17 | `period_jitter_percent` | ❌ MISSING | double | 10 (±10%) |
| 18 | `start_time` | ❌ MISSING | double | 0.0 |
| 19 | `end_time` | ❌ MISSING | double | 0.0 (forever) |
| 20 | `enabled` | ❌ MISSING | bool | true |
| 21 | `generation_count` | ❌ MISSING | int | -1 (unlimited) |

**For Poisson Tasks:**

| # | Parameter Name | Current Status | Type | Example (Voice Assistant) |
|----|---|---|---|---|
| 22 | `lambda` | ❌ MISSING | double | 0.2 (1/5 sec) |
| 23 | `intensity_multiplier` | ❌ MISSING | double | 1.0 (can be 0.5, 2.0, 5.0) |
| 24 | `start_time` | ❌ MISSING | double | 0.0 |

**Existing Code:** [PayloadVehicleApp.h](PayloadVehicleApp.h) line 65 (only task_arrival_mean=generic)  
**Modification Needed:** Create TaskGenerationPattern struct, implement per-task pattern generators  

---

### E. OFFLOADING & FEASIBILITY PARAMETERS (6 parameters)

| # | Parameter Name | Current Status | Type | Example (Cooperative Perception) |
|----|---|---|---|---|
| 25 | `is_offloadable` | ❌ MISSING | bool | true |
| 26 | `is_safety_critical` | ❌ MISSING | bool | false |
| 27 | `offloading_benefit_ratio` | ❌ MISSING | double | 2.0 (2x faster on RSU) |
| 28 | `min_rssi_threshold` | ⚠️ CODE ONLY | double | -85 dBm |
| 29 | `min_link_stability_required` | ⚠️ CODE ONLY | double | 0.8 (80% uptime) |
| 30 | `max_acceptable_packet_loss` | ⚠️ CODE ONLY | double | 0.1 (10% max) |

**Existing Code:** [omnetpp.ini](omnetpp.ini) lines 235-244, [TaskOffloadingDecision.cc](TaskOffloadingDecision.cc) line 111  
**Modification Needed:** Integrate into DecisionContext, use in HeuristicDecisionMaker  

---

### F. DEPENDENCY PARAMETERS (3 parameters)

| # | Parameter Name | Current Status | Type | Example (Coop Perception) |
|----|---|---|---|---|
| 31 | `depends_on` | ❌ MISSING | vector<TaskType> | [LOCAL_OBJECT_DETECTION] |
| 32 | `dependency_ratio` | ❌ MISSING | vector<double> | [0.6] (needs 60% of parent output) |
| 33 | `spawn_children_on_completion` | ❌ MISSING | vector<TaskType> | [ROUTE_OPTIMIZATION, TRAFFIC_PREDICTION] |

**Existing Code:** None  
**Modification Needed:** Create TaskDependency struct, implement DAG traversal  

---

### G. RESOURCE ALLOCATION PARAMETERS (5 parameters)

| # | Parameter Name | Current Status | Type | Value |
|----|---|---|---|---|
| 34 | `cpu_total_hz` | ✅ EXISTS | double | uniform(3e9, 7e9) |
| 35 | `cpu_reservation_ratio` | ✅ EXISTS | double | uniform(0.2, 0.4) |
| 36 | `memory_total_bytes` | ✅ EXISTS | double | uniform(512e6, 4e9) |
| 37 | `cpu_allocation_policy` | ❌ MISSING | Enum(FAIR_SHARE, PRIORITY, PREEMPTIVE) | PRIORITY |
| 38 | `context_switch_overhead_ms` | ❌ MISSING | double | 1.0 |

**Existing Code:** [omnetpp.ini](omnetpp.ini) lines 218-226  
**Modification Needed:** Add allocation policy definition  

---

### SUMMARY: PARAMETER COUNT

```
✅ Already Exists: 12 parameters
   - Task identification: 2 (task_id, vehicle_id, created_time, state)
   - Computation: 2 (task_size_bytes, cpu_cycles - GENERIC)
   - Timing: 2 (created_time, deadline - GENERIC, qos_value)
   - Resources: 3 (cpu_total, cpu_reservation, memory_total)
   - Events: 2+ (completion_event, deadline_event)

❌ Completely Missing: 19 parameters
   - Task type differentiation: 3
   - Per-task computation specifics: 4
   - Periodic/Poisson generation patterns: 9
   - Dependency system: 3

⚠️ Partially Implemented: 7 parameters
   - Offloading characteristics in code but not integrated: 6
   - Deadline/QoS mappings: 1

TOTAL: 38 parameters to model complete task system
```

---

## How Code Location Maps to Parameters

| Parameter Group | File | Lines | Status |
|---|---|---|---|
| Task Identity | [Task.h](Task.h) | 23-34 | ✅ Complete |
| Computational (generic) | [omnetpp.ini](omnetpp.ini) | 211-217 | ⚠️ Generic |
| Timing | [Task.h](Task.h) | 41-51 | ✅ Basic |
| Generation | [PayloadVehicleApp.h](PayloadVehicleApp.h) | 65, 75-82 | ⚠️ Generic only |
| Offloading | [TaskOffloadingDecision.h](TaskOffloadingDecision.h) | 20-50 | ⚠️ Partial |
| Resources | [omnetpp.ini](omnetpp.ini) | 218-226 | ✅ OK |
| Dependencies | NOWHERE | - | ❌ Missing |
| Link Quality | [TaskOffloadingDecision.cc](TaskOffloadingDecision.cc) | 111-150 | ⚠️ Partial |

---

## Next Steps to Implement

**Phase 1 (Week 1):** Add task types and per-task profiles
- [ ] Create `TaskType` enum (9 types)
- [ ] Create `TaskProfile` structure
- [ ] Create per-task parameters in omnetpp.ini

**Phase 2 (Week 1-2):** Implement generation patterns
- [ ] Create `TaskGenerationPattern` struct
- [ ] Implement periodic task generator (Deterministic)
- [ ] Implement Poisson task generator (Exponential inter-arrival)
- [ ] Update `generateTask()` to use patterns

**Phase 3 (Week 2):** Add dependency system
- [ ] Create `TaskDependency` struct
- [ ] Implement dependency DAG
- [ ] Implement `spawnChildren()` on task completion

**Phase 4 (Week 3):** Enhance offloading decisions
- [ ] Link link quality from VEINS/INET
- [ ] Add RSI threshold gate
- [ ] Add stability/packet loss gates

