# Energy Model & Metrics Framework

## Overview
This framework implements three core metrics for IoV task offloading evaluation:
1. **Energy Consumption** (Joules) - From RADiT paper
2. **Latency** (seconds) - End-to-end task completion time
3. **Task Completion Rate** (%) - On-time delivery ratio

---

## Energy Model (Based on RADiT Paper)

The RADiT paper models energy consumption for three execution scenarios:

### 1. Local Execution
**Formula:** `E_local = κ_vehicle × (f_alloc - f_actual)² × d × c`

Where:
- `κ_vehicle` = Switching capacitance coefficient for vehicle (~5×10⁻²⁷)
- `f_alloc` = CPU frequency allocated (typically 1.0 GHz)
- `f_actual` = Actual CPU frequency (may be throttled)
- `d` = Task input data size (bytes)
- `c` = CPU cycles needed

**Key Insight:** Energy grows quadratically with frequency deviation. If `f_alloc ≈ f_actual`, then frequency difference is small and energy is minimized.

### 2. Transmission to RSU
**Formula:** `E_tx = P_tx × t_tx`

Where:
- `P_tx` = Transmit power (~1.0 Watt for 802.11p)
- `t_tx` = Transmission time = `(data_size_bytes × 8) / transmission_rate_bps`

**Key Insight:** Energy depends on both power output and transmission duration. Lower transmission rates = longer duration = more energy.

### 3. RSU Computation
**Formula:** `E_rsu = κ_rsu × (f_alloc - f_actual)² × d × c`

Where:
- `κ_rsu` = Switching capacitance for RSU (~2×10⁻²⁷, more efficient than vehicle)
- Uses same quadratic relationship as local

**Total Offload Energy:** `E_offload = E_tx + E_rsu`

---

## Constants Used

### Power Constants
```cpp
TX_POWER = 1.0 W          // 802.11p transmission power
RX_POWER = 0.8 W          // Listening to channel
IDLE_POWER = 0.2 W        // Device on, no computation
```

### Frequency Constants
```cpp
FREQ_NOMINAL = 1.0 GHz    // Standard CPU frequency
FREQ_MAX = 1.5 GHz        // Turbo/boost frequency
FREQ_MIN = 0.5 GHz        // Power-saving frequency
```

### Switching Capacitance (kappa)
```cpp
KAPPA_VEHICLE = 5e-27     // Jetson Nano-class processor
KAPPA_RSU = 2e-27         // High-performance server (more efficient)
```

---

## Task-Specific Energy Estimates

For your 6 tasks, here are estimated energy costs:

| Task | Local Energy | Offload Energy | Benefit |
|------|--------------|----------------|---------|
| LOCAL_OBJECT_DETECTION | ~1.1e-2 J | N/A (non-offloadable) | N/A |
| COOPERATIVE_PERCEPTION | ~5.5e-2 J | ~2.1e-3 J | 26.1x savings |
| ROUTE_OPTIMIZATION | ~1.1e-1 J | ~3.2e-3 J | 34.4x savings |
| FLEET_TRAFFIC_FORECAST | ~4.4e+0 J | ~8.8e-2 J | 50x savings |
| VOICE_COMMAND_PROCESSING | ~2.2e-2 J | ~1.4e-3 J | 15.7x savings |
| SENSOR_HEALTH_CHECK | ~2.2e-3 J | ~7e-5 J | 31.4x savings |

**Note:** Transmission overhead is small (~0.5-2 mJ for typical task sizes at 6 Mbps), so offload benefits are substantial for computation-heavy tasks.

---

## Metrics Tracked

### Per-Task-Type Metrics (6 separate sets)

**Completion Tracking:**
- `generated` - Total tasks created
- `completed_on_time` - Finished before deadline
- `completed_late` - Finished after deadline
- `failed` - Deadline missed entirely
- `completion_rate` = (on_time + late) / generated
- `on_time_rate` = on_time / generated
- `deadline_miss_rate` = (late + failed) / generated

**Energy Tracking:**
```cpp
total_energy_joules          // Sum of all executions
avg_energy_joules            // Mean energy per task
energy_local_sum             // Energy when executed locally
energy_offload_sum           // Energy when offloaded to RSU
local_execution_count        // # times executed locally
offload_execution_count      // # times offloaded
local_execution_fraction     // % of executions that were local
offload_execution_fraction   // % of executions that were offloaded
```

**Latency Tracking:**
```cpp
total_latency_seconds        // Sum of all e2e latencies
avg_latency_seconds          // Mean latency (generation → completion)
min_latency_seconds          // Best-case latency
max_latency_seconds          // Worst-case latency
latency_local_sum            // Latency when local
latency_offload_sum          // Latency when offloaded
```

### System-Wide Aggregates

```cpp
total_energy_all_tasks           // Energy across all 6 task types
avg_energy_per_task              // Average per completed task
total_latency_all_tasks          // Total latency all tasks combined
avg_latency_per_task             // Average per task
cpu_utilization_percent          // (cycles_used / cycles_available)
network_utilization_percent      // (tx_time / sim_time)
system_completion_rate           // Overall percentage on-time
system_deadline_miss_rate        // Overall percentage missed
```

### Percentile Statistics
For each task type:
- **P50 (median)** latency
- **P95** latency (95th percentile)
- **P99** latency (tail latency)

---

## Usage in Code

### 1. Record Task Generation
```cpp
MetricsManager& metrics = MetricsManager::getInstance();
metrics.recordTaskGenerated(TaskType::COOPERATIVE_PERCEPTION);
```

### 2. Calculate Energy
```cpp
EnergyCalculator energy_calc;

// Local execution
double e_local = energy_calc.calcLocalExecutionEnergy(
    cpu_cycles,              // 2.5e9
    input_size_bytes,        // 300,000
    execution_time_sec       // 0.025
);

// Offload execution
double e_tx = energy_calc.calcTransmissionEnergy(
    input_size_bytes,        // 300,000
    transmission_rate_bps    // 6e6 (6 Mbps)
);

double e_rsu = energy_calc.calcRSUComputationEnergy(
    cpu_cycles,              // 2.5e9
    input_size_bytes,        // 300,000
    execution_time_sec       // 0.005 (faster on RSU)
);

double e_offload = e_tx + e_rsu;
```

### 3. Record Task Completion
```cpp
simtime_t generation_time = task->getCreationTime();
simtime_t completion_time = simTime();
double e2e_latency = (completion_time - generation_time).dbl();

metrics.recordTaskCompletion(
    TaskType::COOPERATIVE_PERCEPTION,        // task type
    MetricsManager::RSU_OFFLOAD,             // where executed
    0.005,                                    // execution time (local/rsu time)
    e2e_latency,                             // end-to-end latency
    e_offload,                               // energy consumed (Joules)
    0.150,                                    // deadline (150ms)
    completion_time.dbl()                    // when it finished
);
```

### 4. Report Metrics
```cpp
// Per-task metrics
auto coop_metrics = metrics.getTaskMetrics(TaskType::COOPERATIVE_PERCEPTION);
EV << "COOP_PERCEP Completion Rate: " << (coop_metrics.completion_rate * 100) << "%" << endl;
EV << "COOP_PERCEP Avg Energy: " << coop_metrics.avg_energy_joules << " J" << endl;

// System-wide
auto sys_metrics = metrics.getSystemMetrics();
EV << "System Deadline Miss Rate: " << (sys_metrics.system_deadline_miss_rate * 100) << "%" << endl;

// Export to file
metrics.exportToCSV("results/metrics.csv");

// Print to stdout
metrics.printReport();
```

---

## Integration Points

Add to your **PayloadVehicleApp.cc**:

```cpp
// In handleTaskCompletion():
EnergyCalculator energy_calc;
MetricsManager& metrics = MetricsManager::getInstance();

// Decide execution location
if (task_was_local) {
    double energy = energy_calc.calcLocalExecutionEnergy(...);
} else {
    double energy = energy_calc.calcOffloadEnergy(...);
}

// Record metrics
metrics.recordTaskCompletion(
    task->type,
    location,
    exec_time,
    e2e_lat,
    energy,
    task->deadline_sec,
    simTime().dbl()
);
```

---

## Comparing Local vs Offload

The framework makes it easy to evaluate offloading benefit:

```cpp
auto breakdown = metrics.getEnergyBreakdown(TaskType::ROUTE_OPTIMIZATION);

double local_avg = breakdown.local_avg_joules;        // ~0.11 J
double offload_avg = breakdown.offload_avg_joules;    // ~0.003 J
double savings = breakdown.savings_per_offload;       // ~0.107 J

cout << "Offloading saves " << (savings/local_avg*100) << "% energy" << endl;
// Output: Offloading saves 97.3% energy
```

---

## Sample Output

```
========== SYSTEM-WIDE SUMMARY ==========
Total Tasks Generated:      15847
Total Tasks Completed:      15203
System Completion Rate:     95.94%
System Deadline Miss Rate:  4.06%

Energy Statistics:
  Total Energy (all tasks):  2.433e+02 J
  Avg Energy per Task:       0.016 J

Latency Statistics:
  Total Latency (all tasks): 3421.45 s
  Avg Latency per Task:      0.2252 s

Resource Utilization:
  CPU Utilization:           78.34%
  Network Utilization:       12.56%
```

---

## Key Formulas Reference

| Metric | Formula | Unit |
|--------|---------|------|
| Local Energy | κ_v × (Δf)² × d × c | Joules |
| Tx Energy | 1.0 × (d×8 / rate) | Joules |
| RSU Energy | κ_m × (Δf)² × d × c | Joules |
| Transmission Time | data_bits / rate | seconds |
| Execution Time | cycles / frequency | seconds |
| E2E Latency | generation → completion | seconds |
| Completion Rate | completed / generated | % |
| CPU Util | cycles_used / cycles_avail | % |

---

## Files Created

1. **EnergyModel.h/cc** - Energy calculation engine (RADiT-based)
2. **MetricsManager.h/cc** - Centralized metrics collection
3. **ENERGY_MODEL_METRICS.md** (this file) - Integration guide

---

**Next Steps:**
1. Include `#include "MetricsManager.h"` and `#include "EnergyModel.h"` in PayloadVehicleApp.cc
2. Call `recordTaskGenerated()` when creating a task
3. Call `recordTaskCompletion()` when task finishes or fails
4. Call `printReport()` at simulation end (in finish method)
5. Run QUICKSTART_IMPLEMENTATION.md Phase 3 to integrate these
