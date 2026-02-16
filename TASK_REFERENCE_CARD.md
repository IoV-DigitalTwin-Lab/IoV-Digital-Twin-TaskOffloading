# TASK TAXONOMY REFERENCE CARD
## Quick Lookup - Keep Handy During Coding

---

## The 6 Tasks at a Glance

| # | Name | Type | Period/λ | Input | CPU | Deadline | Offload? | Notes |
|---|------|------|----------|-------|-----|----------|----------|-------|
| 1 | LOCAL_OBJECT_DETECTION | Periodic | 50ms | 2MB | 5×10⁸ | 50ms | ❌ NO | Camera sync, safety-critical |
| 2 | COOPERATIVE_PERCEPTION | Periodic | 100ms | 0.3MB | 2.5×10⁹ | 150ms | ✅ YES | V2X cycle, 1.5x benefit |
| 3 | ROUTE_OPTIMIZATION | Periodic | 500ms | 1MB | 5×10⁹ | 1s | ✅ YES | Path planning, 2x benefit |
| 4 | FLEET_TRAFFIC_FORECAST | Batch | 60s | 10MB | 2×10¹⁰ | 5min | ✅ MUST | Heavy ML, 10x benefit |
| 5 | VOICE_COMMAND_PROCESSING | Poisson | λ=0.2 | 0.2MB | 1×10⁹ | 500ms | ✅ YES | User-driven, interactive |
| 6 | SENSOR_HEALTH_CHECK | Periodic | 10s | 0.1MB | 1×10⁸ | 10s | ✅ YES | Background, droppable |

---

## Periodic Tasks (4 types)

```cpp
// Fixed intervals, deterministic
Task 1: 50ms    (20 Hz)
Task 2: 100ms   (10 Hz)
Task 3: 500ms   (2 Hz)
Task 6: 10s     (0.1 Hz)

// Generate at: current_time % period == 0
// Apply jitter: ±period_jitter_ratio
```

---

## Poisson Task (1 type)

```cpp
// Random exponential inter-arrival
Task 5: λ = 0.2 tasks/second
        Mean interval = 5 seconds
        Arrive times: 1.2s, 3.4s, 7.8s, 9.1s, ...

// Generate at: simTime() + exponential(1.0/λ)
// Reschedule: next += exponential(1.0/λ)
```

---

## Batch Task (1 type)

```cpp
// Periodic collection
Task 4: Every 60 seconds
        Collects data from fleet
        Runs heavy ML (20G cycles)
        Must offload (too big for vehicle)
```

---

## Dependencies Chain

```
LOCAL_OBJECT_DETECTION (50ms)
    ↓ outputs object list
    └─→ COOPERATIVE_PERCEPTION (100ms)
            ↓ outputs perception map
            └─→ ROUTE_OPTIMIZATION (500ms)

Independent:
    FLEET_TRAFFIC_FORECAST (60s batch)
    VOICE_COMMAND_PROCESSING (Poisson)
    SENSOR_HEALTH_CHECK (10s periodic)
```

---

## TaskProfile Access

```cpp
// Get database
auto& db = TaskProfileDatabase::getInstance();

// Get profile for a type
const auto& profile = db.getProfile(TaskType::COOPERATIVE_PERCEPTION);

// Common fields:
profile.computation.input_size_bytes       // 300KB
profile.computation.cpu_cycles             // 2.5G
profile.timing.deadline_seconds            // 0.150
profile.timing.qos_value                   // 0.85
profile.timing.priority                    // HIGH
profile.generation.pattern                 // PERIODIC
profile.generation.period_seconds          // 0.100
profile.offloading.is_offloadable          // true
profile.offloading.offloading_benefit_ratio // 1.5
profile.dependencies.depends_on            // LOCAL_OBJECT_DETECTION
profile.dependencies.spawns_children       // [ROUTE_OPTIMIZATION]
```

---

## Create & Schedule Task

```cpp
// From profile
Task* task = Task::createFromProfile(
    TaskType::COOPERATIVE_PERCEPTION,
    "Vehicle1",
    seq_num++
);

// Schedule periodic
double period = profile.generation.period_seconds;
double jitter = period * profile.generation.period_jitter_ratio;
scheduleAt(simTime() + period + uniform(-jitter, jitter), event);

// Schedule Poisson
double lambda = profile.generation.lambda;  // 0.2
scheduleAt(simTime() + exponential(1.0/lambda), event);
```

---

## Offloading Decision Logic

```cpp
// Check these in order:
if (!task->is_offloadable) {
    return EXECUTE_LOCALLY;  // Non-negotiable
}

if (task->is_safety_critical) {
    return EXECUTE_LOCALLY;  // Safety must be local
}

if (task->priority_level == SAFETY_CRITICAL || HIGH) {
    // Prefer local, offload only if CPU overloaded
    if (cpu_available > 0) return EXECUTE_LOCALLY;
}

if (task->priority_level == MEDIUM) {
    // Hybrid: compare local vs offload time
    if (local_time < offload_time * 1.2) {
        return EXECUTE_LOCALLY;
    }
    return OFFLOAD_TO_RSU;
}

if (task->priority_level == LOW || BACKGROUND) {
    // Aggressive offloading
    if (rsu_available && link_quality_good) {
        return OFFLOAD_TO_RSU;
    }
    return EXECUTE_LOCALLY;  // Fallback
}
```

---

## QoS Priority Mapping

```
0.95  LOCAL_OBJECT_DETECTION      ⭐ Safety-critical
0.85  COOPERATIVE_PERCEPTION      🔴 High
0.50  VOICE_COMMAND_PROCESSING    🟡 Medium
0.65  ROUTE_OPTIMIZATION          🟡 Medium
0.45  FLEET_TRAFFIC_FORECAST      🟢 Low
0.30  SENSOR_HEALTH_CHECK         🟢 Low

Decision rule: Lower QoS = more likely to offload
```

---

## Metrics to Track

```cpp
// Per task type:
std::map<TaskType, uint32_t> generated_count;
std::map<TaskType, uint32_t> completed_on_time;
std::map<TaskType, uint32_t> completed_late;
std::map<TaskType, uint32_t> failed_count;
std::map<TaskType, double> avg_execution_time;

// Compute:
std::map<TaskType, double> completion_rate;  // completed/generated
std::map<TaskType, double> deadline_miss_rate; // (late+failed)/generated

// Summary:
double total_cpu_utilization;
double total_deadline_miss_rate;
double network_bandwidth_used;
```

---

## Key Constants

```cpp
// Periods
constexpr double LOD_PERIOD = 0.050;     // 50ms
constexpr double COOP_PERIOD = 0.100;    // 100ms
constexpr double ROUTE_PERIOD = 0.500;   // 500ms
constexpr double HEALTH_PERIOD = 10.0;   // 10s
constexpr double BATCH_PERIOD = 60.0;    // 60s

// Poisson rates
constexpr double VOICE_LAMBDA = 0.2;     // 1/5s

// QoS values
constexpr double QOS_LOCAL_OBJ = 0.95;
constexpr double QOS_COOP_PERCEP = 0.85;
constexpr double QOS_ROUTE = 0.65;
constexpr double QOS_VOICE = 0.50;
constexpr double QOS_FLEET = 0.45;
constexpr double QOS_HEALTH = 0.30;

// Offload benefits
constexpr double BENEFIT_COOP = 1.5;
constexpr double BENEFIT_ROUTE = 2.0;
constexpr double BENEFIT_FLEET = 10.0;
```

---

## Implementation Checklist

- [ ] Include TaskProfile.h in all app files
- [ ] Create task generation events in initialize()
- [ ] Implement schedulePeriodicTask() for 4 periodic types
- [ ] Implement schedulePoissonTask() for voice commands
- [ ] Implement scheduleBatchTask() for fleet forecast
- [ ] Handle each task type in handleSelfMsg()
- [ ] Implement generateTask(TaskType) factory
- [ ] Add task type to logging
- [ ] Implement dependency spawning on completion
- [ ] Track metrics per task type
- [ ] Test with omnetpp.ini workload_intensity param
- [ ] Verify log output shows correct arrival patterns

---

## Common Bugs to Watch For

❌ **All tasks arriving at same time** 
   → Check event initialization order; use simTime() += delays

❌ **Poisson task only generates once**
   → Must reschedule in handleSelfMsg() after generating

❌ **Periodic task rate drifts over time**
   → Don't accumulate drift; reschedule from simTime() each time

❌ **Dependencies Never Satisfied**
   → Check parent_id matches in dependency check
   → Verify spawning adds new tasks to queue

❌ **Task metrics all zeros**
   → Check you're actually recording statistics
   → Verify simTime > 0 before checking counters

---

## Quick Debug Commands

```cpp
// Log task arrival
cout << "TaskGen: " << profile.name << " @ " << simTime() << endl;

// Log dependency resolution
cout << "DepCheck: " << task->task_id 
     << " depends on " << taskTypeName(profile.dependencies.depends_on) << endl;

// Log offloading decision
cout << "Decision: " << taskTypeName(task->task_type) 
     << " -> " << (task->is_offloadable ? "OFFLOAD" : "LOCAL") << endl;

// Log metrics at sim end
for (auto& [type, count] : task_completed) {
    cout << taskTypeName(type) << ": " 
         << count << "/" << task_generated[type] 
         << " (" << (100*count/task_generated[type]) << "%)" << endl;
}
```

---

**Reference:** [SIMPLIFIED_TASK_TAXONOMY.md](SIMPLIFIED_TASK_TAXONOMY.md) | [IMPLEMENTATION_READY.md](IMPLEMENTATION_READY.md) | [QUICKSTART_IMPLEMENTATION.md](QUICKSTART_IMPLEMENTATION.md)
