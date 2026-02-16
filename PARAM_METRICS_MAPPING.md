# Task Parameters → Metrics Mapping (Complete Flow)

## System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    TASK LIFECYCLE                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  TASK PARAMETERS (8)        EXECUTION FLOW      METRICS (3)    │
│  ━━━━━━━━━━━━━━━━━━━━━━    ━━━━━━━━━━━━━━━    ━━━━━━━━━━━━  │
│                                                                 │
│  CPU Cycles ━┐                               ┌→ LATENCY       │
│             │                Queuing         │   (seconds)    │
│  Deadline ──┼──→ Task ─→ Transmission ─→ Execution ─→          │
│             │    Params   (if offload)       │                 │
│  Input Size │               Execution       ├→ ENERGY        │
│             │              (Local/RSU)      │   (Joules)      │
│  Output Size┤                               │                 │
│             │             Return           ├→ COMPLETION    │
│  Priority ──┼──→         Transmission       │   RATE (%)      │
│             │           (if offload)       │                 │
│  QoS ───────┤                              │                 │
│             │            Record            └→ MetricsManager  │
│  Task Type  │            Metrics                              │
│             │                                                 │
│  Offloadable┘                                                 │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Detailed Parameter Flow to Each Metric

### LATENCY = f(deadline, cpu_cycles, input_size, priority, task_type)

```cpp
// Pseudocode: How latency is calculated

simtime_t generation_time = task.creation_time;          // When generated
simtime_t execution_start = generation_time + queue_delay;

// Queue delay depends on:
// - priority (HIGH = shorter queue)
// - task_type (arrival pattern affects queue)
simtime_t queue_delay = computeQueueDelay(
    task.priority,           // Parameter 5
    task.task_type           // Parameter 7
);

// Execution time depends on cpu_cycles and execution location
double execution_time;
if (execute_locally) {
    // execution_time = cpu_cycles / cpu_frequency
    execution_time = task.cpu_cycles / 1.0e9;  // At 1 GHz
} else {
    // Transmission time depends on input_size
    double tx_time = task.input_size_bytes * 8.0 / transmission_rate_bps;
    // RSU execution time more efficient
    double rsu_exec_time = task.cpu_cycles / 1.5e9;  // RSU ~1.5x faster
    // Return transmission of output
    double return_time = task.output_size_bytes * 8.0 / transmission_rate_bps;
    execution_time = tx_time + rsu_exec_time + return_time;
}

simtime_t completion_time = execution_start + execution_time;

// METRIC 1: LATENCY
double latency_seconds = (completion_time - generation_time).dbl();

// Threshold check uses deadline
bool on_time = (latency_seconds <= task.deadline_seconds);  // Parameter 2
```

**MetricsManager captures:**
```cpp
MetricsManager.recordTaskCompletion(
    task.task_type,              // For per-task-type tracking
    execution_location,          // LOCAL or RSU_OFFLOAD
    execution_time,              // Executor time (not E2E)
    latency_seconds,             // ← METRIC 1: E2E Latency
    energy_joules,
    task.deadline_seconds,       // For on-time check
    completion_time.dbl()        // Used for on-time calculation
);
```

---

### ENERGY = f(cpu_cycles, input_size, output_size, offloadable, task_type)

```cpp
// Pseudocode: How energy is calculated

double energy_consumed;

if (execute_locally) {
    // Formula: E = κ_vehicle × (f_alloc - f_actual)² × d × c
    // Parameters used: cpu_cycles, input_size
    
    energy_consumed = EnergyCalculator::calcLocalExecutionEnergy(
        task.cpu_cycles,                // Parameter 1 (quadratic factor)
        task.input_size_bytes,          // Parameter 3 (quadratic factor)
        execution_time
    );
    // E ≈ κ_v × input_size × cpu_cycles (main factors)
    
} else if (task.offloadable) {  // Parameter 8
    
    // Transmission energy: E_tx = P_tx × t_tx
    double tx_energy = EnergyCalculator::calcTransmissionEnergy(
        task.input_size_bytes,          // Parameter 3: Size determines tx time
        transmission_rate_bps           // 6 Mbps typical
    );
    // E_tx ≈ 1W × (input_size × 8 / 6e6)
    
    // RSU computation energy: E_rsu = κ_rsu × (f_alloc - f_actual)² × d × c
    double rsu_energy = EnergyCalculator::calcRSUComputationEnergy(
        task.cpu_cycles,                // Parameter 1 (quadratic factor)
        task.input_size_bytes,          // Parameter 3 (quadratic factor)
        rsu_execution_time
    );
    // E_rsu ≈ κ_m × input_size × cpu_cycles (more efficient than local)
    
    // Return transmission energy (often negligible)
    double return_energy = EnergyCalculator::calcTransmissionEnergy(
        task.output_size_bytes,         // Parameter 4: Return size
        transmission_rate_bps
    );
    
    energy_consumed = tx_energy + rsu_energy + return_energy;
    
} else {
    // Non-offloadable tasks must execute locally
    // Same as local execution above
    energy_consumed = EnergyCalculator::calcLocalExecutionEnergy(...);
}

// METRIC 2: ENERGY
double energy_joules = energy_consumed;
```

**MetricsManager captures:**
```cpp
MetricsManager.recordTaskCompletion(
    task.task_type,
    execution_location,
    execution_time,
    latency_seconds,
    energy_joules,              // ← METRIC 2: Energy Consumption
    task.deadline_seconds,
    completion_time.dbl()
);

// Later, user can query:
auto energy_breakdown = MetricsManager.getEnergyBreakdown(TaskType::ROUTE_OPTIMIZATION);
// energy_breakdown.local_avg_joules     = avg for local execs
// energy_breakdown.offload_avg_joules   = avg for offload execs
// energy_breakdown.savings_per_offload  = local_avg - offload_avg
```

---

### COMPLETION_RATE = f(deadline, cpu_cycles, input_size, priority, task_type)

```cpp
// Pseudocode: How completion rate is calculated

// Track per task type:
struct PerTaskMetrics {
    uint32_t generated = 0;           // Total created
    uint32_t completed_on_time = 0;   // ✓ latency <= deadline
    uint32_t completed_late = 0;      // latency > deadline but recovered
    uint32_t failed = 0;              // ✗ deadline passed
};

// When a task completes:
if (completion_time <= task.deadline_seconds) {  // Parameter 2
    metrics[task.task_type].completed_on_time++;   // Parameter 7
} else {
    metrics[task.task_type].completed_late++;
}

// When a task times out:
if (simtime() > task.deadline_seconds) {
    metrics[task.task_type].failed++;
}

// METRIC 3: COMPLETION RATE
double on_time_rate = completed_on_time / generated;
double deadline_miss_rate = (completed_late + failed) / generated;
double completion_rate = (completed_on_time + completed_late) / generated;
// ↑ METRIC 3A: On-Time Rate (%)
// ↑ METRIC 3B: Deadline Miss Rate (%)
// ↑ METRIC 3C: Overall Completion Rate (%)
```

**MetricsManager captures:**
```cpp
// Upon successful completion:
MetricsManager.recordTaskCompletion(
    task.task_type,              // Parameter 7: Which task type?
    execution_location,
    execution_time,
    latency_seconds,
    energy_joules,
    task.deadline_seconds,       // Parameter 2: Threshold check
    completion_time.dbl()        // Used in on-time check
);

// Upon timeout/failure:
MetricsManager.recordTaskFailed(
    task.task_type,              // Parameter 7
    latency_seconds
);

// Later, user can query:
auto metrics = MetricsManager.getTaskMetrics(TaskType::COOPERATIVE_PERCEPTION);
// metrics.completion_rate        = (on_time + late) / generated
// metrics.on_time_rate           = on_time / generated
// metrics.deadline_miss_rate     = (late + failed) / generated

auto system = MetricsManager.getSystemMetrics();
// system.system_completion_rate  = overall percentage on-time
// system.system_deadline_miss_rate = overall deadline misses
```

---

## Quick Reference: Which Parameters Affect Which Metrics?

| Parameter | Affects Latency | Affects Energy | Affects Completion |
|-----------|-----------------|----------------|-------------------|
| 1. CPU Cycles | ✅ execution_time | ✅ quadratic factor | ✅ execution_time |
| 2. Deadline | ✅ threshold | ❌ no | ✅ on-time check |
| 3. Input Size | ✅ tx_time (if offload) | ✅ quadratic factor | ✅ affects offload decision |
| 4. Output Size | ✅ return_tx_time | ✅ return energy | ✅ return affects total time |
| 5. Priority | ✅ queue_delay | ❌ no | ✅ queue_delay |
| 6. QoS | ❌ no | ❌ no | ❌ no (informational) |
| 7. Task Type | ✅ arrival pattern, scheduling | ✅ categorizes in metrics | ✅ breakdown per type |
| 8. Offloadable | ✅ routing decision | ✅ determines tx+rsu vs local | ✅ affects overall strategy |

---

## Real-World Example: FLEET_TRAFFIC_FORECAST

**Task Parameters (from TaskProfile.cc):**
```cpp
cpu_cycles:        2e10              // 20 billion cycles (heavy ML)
deadline:          300.0             // 5 minutes (soft deadline)
input_size_bytes:  10_000_000        // 10 MB fleet data
output_size_bytes: 1_000_000         // 1 MB predictions
priority:          LOW               // Can defer
qos_value:         0.45              // Non-critical
task_type:         FLEET_TRAFFIC_FORECAST
offloadable:       true              // MUST offload (too heavy for vehicle)
```

**Latency Calculation (Offload Path):**
```
Queue wait:           ~2 seconds (low priority, batch)
Transmission (10MB):  10e6 × 8 / 6e6 bps ≈ 13.3 seconds
RSU execution:        20e9 cycles / 1.5e9 Hz ≈ 13.3 seconds
Return (1MB):         1e6 × 8 / 6e6 bps ≈ 1.3 seconds
                      ─────────────────
Total E2E Latency:    ~30 seconds

vs Deadline:          300 seconds
Status:               ✅ ON-TIME (30 << 300)
```

**Energy Calculation (Offload):**
```
Transmission:         E_tx = 1W × 13.3s = 13.3 Joules
RSU computation:      E_rsu = κ_rsu × 10e6 × 20e9 
                            = 2e-27 × 10e6 × 20e9
                            = 400 Joules
Return:               E_return = 1W × 1.3s = 1.3 Joules
                      ──────────────────────
Total Energy:         ~414.6 Joules

vs Local:             E_local = κ_v × 10e6 × 20e9
                             = 5e-27 × 10e6 × 20e9
                             = 1000 Joules (roughly)
                      
Savings:              (1000 - 414.6) / 1000 ≈ 59% energy saved by offloading
```

**Completion Rate:**
```
100 batch jobs generated over 1 hour simulation
Deadline: 300 seconds per task
All 100 completed in < 40 seconds each
Status:               ✅ 100% ON-TIME = 100% COMPLETION_RATE
```

**Metrics Recorded:**
```cpp
MetricsManager::getInstance().recordTaskCompletion(
    TaskType::FLEET_TRAFFIC_FORECAST,      // Type 3
    MetricsManager::RSU_OFFLOAD,           // Offloaded
    13.3,                                   // Execution time at RSU
    30.0,                                   // E2E latency ← METRIC 1
    414.6,                                  // Energy ← METRIC 2
    300.0,                                  // Deadline
    simTime().dbl()
);

// Then MetricsManager updates:
task_metrics[FLEET_TRAFFIC_FORECAST].completed_on_time++;         // ← METRIC 3
task_metrics[FLEET_TRAFFIC_FORECAST].total_latency_seconds += 30.0;
task_metrics[FLEET_TRAFFIC_FORECAST].total_energy_joules += 414.6;
```

---

## Integration Points in Code

**Where task parameters enter:**
- TaskProfile.cc (constants for each task type)
- PayloadVehicleApp.cc (task generation)

**Where metrics are recorded:**
- EnergyModel.cc (energy is calculated)
- MetricsManager.cc (all three metrics collected)

**Where metrics are exported:**
- MetricsManager.printReport() → stdout
- MetricsManager.exportToCSV() → CSV file
- getTaskMetrics() → C++ queries
- getSystemMetrics() → C++ queries

---

## Data Flow Diagram

```
TaskProfile.h ─┐
TaskProfile.cc │──→ TaskProfileDatabase ──→ PayloadVehicleApp
               └──→                         │
               
               Task Generation ───────────→ Queue
               │
               ├─ type (Param 7)
               ├─ cpu_cycles (Param 1)
               ├─ deadline_seconds (Param 2)
               ├─ input_size (Param 3)
               ├─ output_size (Param 4)
               ├─ priority (Param 5)
               ├─ qos (Param 6)
               └─ offloadable (Param 8)
               
                      ↓
                      
               Execution Path Decision
               (local vs RSU)
               
                      ↓
                ┌─────┴─────┐
                ↓           ↓
            LOCAL        OFFLOAD
            Exec         to RSU
            │            │
            ├──→ Compute Latency ──→ Compare to Deadline
            │            │
            ├──→ Calculate Energy ──→ EnergyCalculator
            │            │
            └──→ Record Metrics ────→ MetricsManager
                         │
              ┌──────────┼──────────┐
              ↓          ↓          ↓
          LATENCY    ENERGY    COMPLETION
          (seconds)  (Joules)     RATE (%)
          
              ↓          ↓          ↓
              └──────────┼──────────┘
                         ↓
              MetricsManager Storage
                         ↓
              printReport() or exportToCSV()
```

---

## Summary

**8 Task Parameters → 3 Metrics:**

| Task Param | Latency | Energy | Completion |
|-----------|---------|--------|-----------|
| CPU Cycles | exec_time | quadratic | affects timing |
| Deadline | threshold | no | on-time check |
| Input Size | tx_time | quadratic | affects offload |
| Output Size | return_time | return_energy | affects total |
| Priority | queue_delay | no | via latency |
| QoS | no | no | informational |
| Task Type | scheduling | categorization | breakdown |
| Offloadable | route choice | path choice | strategy |

**MetricsManager collects all three metrics and provides:**
- ✅ Per-task-type metrics (6 separate sets)
- ✅ System-wide aggregates
- ✅ Energy breakdowns (local vs offload)
- ✅ Latency percentiles (P50, P95, P99)
- ✅ CSV export for analysis
- ✅ Formatted console reports
