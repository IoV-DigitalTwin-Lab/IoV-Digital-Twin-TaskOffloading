# Task Modeling Parameters - Complete Specification

## 1. DISTRIBUTION CLASSIFICATION

### Periodic Tasks (Deterministic Interval)
These tasks are generated at fixed time intervals - **NOT Poisson**

| Task Type | Interval | Rate | Distribution | Rationale |
|-----------|----------|------|--------------|-----------|
| **LOCAL_OBJECT_DETECTION** | 50 ms | 20 Hz | **Deterministic** | Camera frame rate synchronous |
| **COOPERATIVE_PERCEPTION** | 100 ms | 10 Hz | **Deterministic** | V2X cycle rate (802.11p standard) |
| **ROUTE_OPTIMIZATION** | 500 ms | 2 Hz | **Deterministic** | Path recalculation cycle |
| **SENSOR_DIAGNOSTICS** | 10 s | 0.1 Hz | **Deterministic** | Health monitoring interval |
| **INFOTAINMENT_UPDATE** | 30 s | 0.033 Hz | **Deterministic** | Background content refresh |

---

### Poisson Tasks (Random Arrival)
These tasks follow exponential inter-arrival times with parameter λ

| Task Type | Mean Interval | Lambda (λ) | Distribution | Rationale |
|-----------|--------------|-----------|--------------|-----------|
| **VOICE_ASSISTANT_PROCESSING** | 5 seconds | 0.2 | **Poisson** | User commands are sporadic |
| **ROUTE_ALTERNATIVE_CALC** | 20 seconds | 0.05 | **Poisson** | User-requested alternatives |
| **WEATHER_FETCH_AND_PROCESS** | 60 seconds | 0.0167 | **Poisson** | Occasional weather updates |
| **TRAFFIC_PREDICTION** | 10 seconds | 0.1 | **Poisson** | Analytics request arrival |

**Poisson inter-arrival formula:** Time between tasks ~ exponential(λ)
- Example: λ=0.2 means on average 5 seconds between tasks
- But can have 2 in quick succession, then 10s gap - reflects real user behavior

---

## 2. COMPREHENSIVE TASK PARAMETER LIST

### A. TASK IDENTITY & TYPE INFORMATION

**STATUS: PARTIALLY EXISTS** (need to add task type)

```cpp
// ALREADY IN CODE (Task.h / Task.cc):
✅ task_id                          // "V{vid}_T{seq}_{timestamp}"
✅ vehicle_id                       // Originating vehicle
✅ created_time                     // When generated (simtime_t)
✅ state                            // Task state machine

// NEED TO ADD:
❌ task_type                        // TaskType enum (LOCAL_DETECTION, etc.)
❌ task_type_name                   // String name ("Local Object Detection")
❌ generation_pattern               // TaskPattern enum (PERIODIC, POISSON)
```

---

### B. COMPUTATIONAL CHARACTERISTICS

**STATUS: PARTIALLY EXISTS** (uses generic min/max, need per-type specifics)

**Currently exists in omnetpp.ini:**
```ini
✅ *.node[*].appl.task_size_min = 1e5           # Generic min 100 KB
✅ *.node[*].appl.task_size_max = 5e5           # Generic max 500 KB
✅ *.node[*].appl.cpu_per_byte_min = 500        # Generic 500 cy/byte
✅ *.node[*].appl.cpu_per_byte_max = 5000       # Generic 5000 cy/byte
✅ *.node[*].appl.deadline_min = 0.5s           # Generic 500 ms
✅ *.node[*].appl.deadline_max = 2.0s           # Generic 2 seconds
```

**Need to add PER-TASK-TYPE specifics:**
```cpp
// NEW in Task.h:
struct TaskProfile {
    TaskType type;
    std::string name;
    
    // Fixed or distribution for each task type
    struct IOCharacteristics {
        uint64_t input_size_bytes;        // Fixed or (min, max)
        uint64_t output_size_bytes;       // For dependent tasks
    } io_char;
    
    struct ComputationCharacteristics {
        uint64_t cpu_cycles_required;     // Fixed or (min, max)
        double estimated_memory_bytes;    // Peak memory during execution
    } compute_char;
    
    struct TimingCharacteristics {
        double deadline_seconds;          // Hard deadline
        double qos_value;                 // Priority weight
    } timing_char;
    
    struct OffloadingCharacteristics {
        bool is_offloadable;              // Can be sent to RSU?
        bool is_safety_critical;          // Must execute locally?
        double offloading_benefit_ratio;  // Time saved if offloaded (1.0-10.0x)
    } offload_char;
};

// NEW in omnetpp.ini (per task type):
# Task Type-Specific Parameters

# LOCAL_OBJECT_DETECTION
*.node[*].appl.task[LOCAL_OBJECT_DETECTION].input_size = 2e6        # 2 MB sensor data
*.node[*].appl.task[LOCAL_OBJECT_DETECTION].cpu_cycles = 5e8        # 500M cycles @ 2GHz = 250ms
*.node[*].appl.task[LOCAL_OBJECT_DETECTION].deadline = 0.05         # 50ms hard deadline
*.node[*].appl.task[LOCAL_OBJECT_DETECTION].output_size = 0.1e6     # 100 KB object list
*.node[*].appl.task[LOCAL_OBJECT_DETECTION].qos_value = 0.95        # Safety critical
*.node[*].appl.task[LOCAL_OBJECT_DETECTION].is_offloadable = false  # Cannot offload
*.node[*].appl.task[LOCAL_OBJECT_DETECTION].is_safety_critical = true

# COOPERATIVE_PERCEPTION
*.node[*].appl.task[COOPERATIVE_PERCEPTION].input_size = 0.3e6      # 300 KB aggregated
*.node[*].appl.task[COOPERATIVE_PERCEPTION].cpu_cycles = 2.5e9      # 2.5G cycles
*.node[*].appl.task[COOPERATIVE_PERCEPTION].deadline = 0.15         # 150ms
*.node[*].appl.task[COOPERATIVE_PERCEPTION].output_size = 0.15e6    # 150 KB perception map
*.node[*].appl.task[COOPERATIVE_PERCEPTION].qos_value = 0.85        # High priority
*.node[*].appl.task[COOPERATIVE_PERCEPTION].is_offloadable = true
*.node[*].appl.task[COOPERATIVE_PERCEPTION].is_safety_critical = false

# ROUTE_OPTIMIZATION
*.node[*].appl.task[ROUTE_OPTIMIZATION].input_size = 1e6            # 1 MB road graph
*.node[*].appl.task[ROUTE_OPTIMIZATION].cpu_cycles = 5e9            # 5G cycles
*.node[*].appl.task[ROUTE_OPTIMIZATION].deadline = 1.0              # 1 second
*.node[*].appl.task[ROUTE_OPTIMIZATION].output_size = 0.2e6         # 200 KB path
*.node[*].appl.task[ROUTE_OPTIMIZATION].qos_value = 0.65            # Medium priority
*.node[*].appl.task[ROUTE_OPTIMIZATION].is_offloadable = true
*.node[*].appl.task[ROUTE_OPTIMIZATION].is_safety_critical = false
*.node[*].appl.task[ROUTE_OPTIMIZATION].offloading_benefit = 2.0    # 2x faster on RSU

# TRAFFIC_PREDICTION
*.node[*].appl.task[TRAFFIC_PREDICTION].input_size = 10e6           # 10 MB fleet data
*.node[*].appl.task[TRAFFIC_PREDICTION].cpu_cycles = 2e10           # 20G cycles (LSTM)
*.node[*].appl.task[TRAFFIC_PREDICTION].deadline = 300.0            # 5 minutes (soft)
*.node[*].appl.task[TRAFFIC_PREDICTION].output_size = 1e6           # 1 MB predictions
*.node[*].appl.task[TRAFFIC_PREDICTION].qos_value = 0.45            # Low priority
*.node[*].appl.task[TRAFFIC_PREDICTION].is_offloadable = true
*.node[*].appl.task[TRAFFIC_PREDICTION].is_safety_critical = false
*.node[*].appl.task[TRAFFIC_PREDICTION].offloading_benefit = 5.0    # 5x faster on RSU

# SENSOR_DIAGNOSTICS
*.node[*].appl.task[SENSOR_DIAGNOSTICS].input_size = 0.1e6          # 100 KB
*.node[*].appl.task[SENSOR_DIAGNOSTICS].cpu_cycles = 1e8            # 100M cycles
*.node[*].appl.task[SENSOR_DIAGNOSTICS].deadline = 10.0             # 10 seconds
*.node[*].appl.task[SENSOR_DIAGNOSTICS].output_size = 0.05e6        # 50 KB report
*.node[*].appl.task[SENSOR_DIAGNOSTICS].qos_value = 0.3             # Low priority
*.node[*].appl.task[SENSOR_DIAGNOSTICS].is_offloadable = true

# INFOTAINMENT_UPDATE
*.node[*].appl.task[INFOTAINMENT_UPDATE].input_size = 0.5e6         # 500 KB
*.node[*].appl.task[INFOTAINMENT_UPDATE].cpu_cycles = 5e8           # 500M cycles
*.node[*].appl.task[INFOTAINMENT_UPDATE].deadline = 30.0            # 30 seconds
*.node[*].appl.task[INFOTAINMENT_UPDATE].output_size = 1e6          # 1 MB media list
*.node[*].appl.task[INFOTAINMENT_UPDATE].qos_value = 0.2            # Low priority
*.node[*].appl.task[INFOTAINMENT_UPDATE].is_offloadable = true

# VOICE_ASSISTANT_PROCESSING
*.node[*].appl.task[VOICE_ASSISTANT].input_size = 0.2e6             # 200 KB audio
*.node[*].appl.task[VOICE_ASSISTANT].cpu_cycles = 1e9               # 1G cycles
*.node[*].appl.task[VOICE_ASSISTANT].deadline = 0.5                 # 500ms response
*.node[*].appl.task[VOICE_ASSISTANT].output_size = 0.05e6           # 50 KB response
*.node[*].appl.task[VOICE_ASSISTANT].qos_value = 0.5                # Medium (interactive)
*.node[*].appl.task[VOICE_ASSISTANT].is_offloadable = true

# ROUTE_ALTERNATIVE_CALC
*.node[*].appl.task[ROUTE_ALTERNATIVE].input_size = 1.5e6           # 1.5 MB
*.node[*].appl.task[ROUTE_ALTERNATIVE].cpu_cycles = 8e9             # 8G cycles
*.node[*].appl.task[ROUTE_ALTERNATIVE].deadline = 2.0               # 2 seconds
*.node[*].appl.task[ROUTE_ALTERNATIVE].output_size = 0.5e6          # 500 KB paths
*.node[*].appl.task[ROUTE_ALTERNATIVE].qos_value = 0.55             # Medium-low
*.node[*].appl.task[ROUTE_ALTERNATIVE].is_offloadable = true

# WEATHER_FETCH_AND_PROCESS
*.node[*].appl.task[WEATHER_FETCH].input_size = 0.3e6               # 300 KB
*.node[*].appl.task[WEATHER_FETCH].cpu_cycles = 2e8                 # 200M cycles
*.node[*].appl.task[WEATHER_FETCH].deadline = 60.0                  # 1 minute
*.node[*].appl.task[WEATHER_FETCH].output_size = 0.2e6              # 200 KB weather
*.node[*].appl.task[WEATHER_FETCH].qos_value = 0.25                 # Low (informational)
*.node[*].appl.task[WEATHER_FETCH].is_offloadable = true
```

---

### C. TASK GENERATION PATTERN PARAMETERS

**STATUS: PARTIALLY EXISTS** (only generic arrival, no per-type control)

**Currently in code:**
```cpp
// PayloadVehicleApp.h:
✅ task_arrival_mean = 2.0s         // Generic mean inter-arrival
```

**Need to add DISTRIBUTION-SPECIFIC parameters:**

```cpp
// NEW struct in PayloadVehicleApp.h:
struct TaskGenerationPattern {
    TaskType type;
    
    // For PERIODIC tasks:
    double period_seconds;              // E.g., 0.050 for 50ms
    double period_jitter_ratio;         // ±jitter (0.0-1.0)
    
    // For POISSON tasks:
    double lambda;                      // Rate parameter for Poisson
    double intensity_multiplier;        // For workload scenarios (light=0.5, peak=2.0)
    
    // General:
    bool enabled;                       // Can disable task generation
    double start_time_sec;              // When to start generating
    double end_time_sec;                // When to stop generating (0=forever)
};

// NEW in omnetpp.ini:

# ============================================================================
# TASK GENERATION PATTERNS (by distribution)
# ============================================================================

# PERIODIC TASK PATTERNS
*.node[*].appl.LocalObjDet_pattern = "periodic"
*.node[*].appl.LocalObjDet_period = 0.050          # 50ms (20 Hz)
*.node[*].appl.LocalObjDet_jitter = 0.01           # ±10% jitter

*.node[*].appl.CoopPercept_pattern = "periodic"
*.node[*].appl.CoopPercept_period = 0.100          # 100ms (10 Hz)
*.node[*].appl.CoopPercept_jitter = 0.02           # ±20% jitter

*.node[*].appl.RouteOpt_pattern = "periodic"
*.node[*].appl.RouteOpt_period = 0.500             # 500ms (2 Hz)
*.node[*].appl.RouteOpt_jitter = 0.05              # ±50% jitter (flexible)

*.node[*].appl.SensorDiag_pattern = "periodic"
*.node[*].appl.SensorDiag_period = 10.0            # 10s
*.node[*].appl.SensorDiag_jitter = 0.1             # ±100% allowed (very flexible)

*.node[*].appl.Infotainment_pattern = "periodic"
*.node[*].appl.Infotainment_period = 30.0          # 30s
*.node[*].appl.Infotainment_jitter = 0.2           # ±200% allowed

# POISSON TASK PATTERNS (rate-based, in tasks/second)
*.node[*].appl.VoiceAssist_pattern = "poisson"
*.node[*].appl.VoiceAssist_lambda = 0.2            # 1 task per 5 seconds
*.node[*].appl.VoiceAssist_intensity = 1.0         # Normal intensity

*.node[*].appl.RouteAlt_pattern = "poisson"
*.node[*].appl.RouteAlt_lambda = 0.05              # 1 task per 20 seconds
*.node[*].appl.RouteAlt_intensity = 1.0            # Normal intensity

*.node[*].appl.Weather_pattern = "poisson"
*.node[*].appl.Weather_lambda = 0.0167             # 1 task per 60 seconds
*.node[*].appl.Weather_intensity = 1.0             # Normal intensity

*.node[*].appl.TrafficPred_pattern = "poisson"
*.node[*].appl.TrafficPred_lambda = 0.1            # 1 task per 10 seconds
*.node[*].appl.TrafficPred_intensity = 1.0         # Normal intensity (can increase for stress test)

# WORKLOAD SCENARIO MULTIPLIERS (apply to all Poisson tasks)
# Light traffic: 0.5x normal
# Normal traffic: 1.0x
# Peak traffic: 2.0-3.0x
# Stress test: 5.0x+
*.node[*].appl.workload_intensity = 1.0            # Global multiplier
```

---

### D. TASK DEPENDENCIES & OUTPUT

**STATUS: DOES NOT EXIST**

```cpp
// NEW in Task.h:
struct TaskDependency {
    TaskType required_task;           // Which task must complete first
    double output_ratio_needed;       // 0.0-1.0 (% of parent output)
    bool is_mandatory;                // false = optional (best effort)
};

struct OutputProfile {
    uint64_t output_size_bytes;       // How much data output
    std::string output_type;          // "object_list", "perception_map", etc.
    double utility_to_others;         // Score for how useful to other tasks
};

class Task {
    std::vector<TaskDependency> depends_on;
    std::vector<TaskType> can_spawn;       // Child tasks to create
    OutputProfile output_profile;
};

// NEW in omnetpp.ini (dependency configuration):
# Task Dependencies and Output

# LOCAL_OBJECT_DETECTION produces object list
*.node[*].appl.LocalObjDet_output_size = 0.1e6
*.node[*].appl.LocalObjDet_output_type = "object_list"

# COOPERATIVE_PERCEPTION depends on LOCAL_OBJECT_DETECTION
*.node[*].appl.CoopPercept_depends_on = "LOCAL_OBJECT_DETECTION"
*.node[*].appl.CoopPercept_dependency_ratio = 0.6   # needs 60% of detection output
*.node[*].appl.CoopPercept_output_size = 0.15e6
*.node[*].appl.CoopPercept_output_type = "perception_map"
# When CoopPercept completes, spawn:
*.node[*].appl.CoopPercept_spawn_children = "ROUTE_OPTIMIZATION,TRAFFIC_PREDICTION,VOICE_ASSISTANT"

# ROUTE_OPTIMIZATION depends on both detection and perception
*.node[*].appl.RouteOpt_depends_on = "LOCAL_OBJECT_DETECTION,COOPERATIVE_PERCEPTION"
*.node[*].appl.RouteOpt_dependency_ratio = "0.5,0.8"  # needs 50% detection, 80% perception
```

---

### E. RESOURCE & EXECUTION PARAMETERS

**STATUS: EXISTS** (but needs enhancement)

```cpp
// ALREADY EXISTS in omnetpp.ini:
✅ *.node[*].appl.cpu_total = uniform(3e9, 7e9)
✅ *.node[*].appl.cpu_reservation_ratio = uniform(0.2, 0.4)
✅ *.node[*].appl.memory_total = uniform(512e6, 4e9)
✅ *.node[*].appl.max_queue_size = 50
✅ *.node[*].appl.max_concurrent_tasks = 4

// NEED TO ADD:
❌ *.node[*].appl.cpu_allocation_policy = "fair_share"  # or "priority_based"
❌ *.node[*].appl.memory_per_task_min = 10e6            # 10 MB minimum per task
❌ *.node[*].appl.task_context_switch_overhead = 0.001  # 1ms overhead
```

---

### F. OFFLOADING & NETWORK PARAMETERS

**STATUS: EXISTS** (but link quality not integrated)

```cpp
// ALREADY EXISTS:
✅ *.node[*].appl.offloadingEnabled = true
✅ *.node[*].appl.serviceVehicleEnabled = false
✅ *.node[*].appl.rsuDecisionTimeout = 1.0s
✅ *.node[*].appl.offloadedTaskTimeout = 5.0s

// NEED TO ADD (Link quality integration):
❌ *.node[*].appl.rssi_threshold_offloading = -85dBm
❌ *.node[*].appl.min_link_stability = 0.8        # 80% uptime requirement
❌ *.node[*].appl.max_packet_loss_ratio = 0.1     # 10% max acceptable
❌ *.node[*].appl.offload_link_overhead_ratio = 0.2  # 20% network overhead margin
```

---

### G. QoS & PRIORITY PARAMETERS

**STATUS: PARTIALLY EXISTS** (qos_value exists but not well-defined)

```cpp
// ALREADY EXISTS:
✅ double qos_value;  // In Task class (0.0-1.0)

// NEED TO DEFINE PROPERLY:

// Priority Level Mapping:
enum class PriorityLevel {
    SAFETY_CRITICAL = 0.9-1.0,     // LOCAL_OBJECT_DETECTION
    HIGH = 0.7-0.89,               // COOPERATIVE_PERCEPTION
    MEDIUM = 0.5-0.69,             // ROUTE_OPTIMIZATION, VOICE_ASSISTANT
    LOW = 0.2-0.49,                // TRAFFIC_PREDICTION, SENSOR_DIAGNOSTICS
    BACKGROUND = 0.0-0.19          // INFOTAINMENT_UPDATE
};

✅ Priority scheduling implemented in TaskComparator
```

---

## 3. SUMMARY TABLE: WHAT EXISTS vs WHAT NEEDS TO BE ADDED

| Parameter Category | Current Status | What Exists | What Needs Adding |
|-------------------|--------|---------|-----------------|
| **Task Type** | ❌ Missing | None | TaskType enum + per-type profiles |
| **Task Identity** | ✅ Complete | task_id, vehicle_id, created_time | - |
| **Computational Characteristics** | ⚠️ Generic | Generic min/max ranges | Per-task-type specifics, output_size |
| **Timing** | ⚠️ Generic | Generic deadline ranges | Per-task-type deadlines, QoS mapping |
| **Generation Patterns** | ⚠️ Generic | task_arrival_mean (Poisson-like) | Per-task distribution control, periodicity |
| **Dependencies** | ❌ Missing | None | Dependency graph, output profiles |
| **Resources** | ✅ Adequate | CPU, memory, queue parameters | Allocation policies, per-task minimums |
| **Offloading** | ⚠️ Partial | Basic flags | Link quality integration, RSSI thresholds |
| **QoS/Priority** | ⚠️ Partial | qos_value field | Clear priority level definitions |

---

## 4. IMPLEMENTATION PHASES

**Phase 1:** Add TaskType enum and TaskProfile structure  
**Phase 2:** Add generation pattern control (periodic vs Poisson)  
**Phase 3:** Add per-task parameters to omnetpp.ini  
**Phase 4:** Add dependency system  
**Phase 5:** Integrate link quality into offloading decisions  

