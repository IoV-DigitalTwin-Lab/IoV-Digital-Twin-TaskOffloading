# Metrics Integration Checklist

Quick step-by-step guide to add energy/latency/completion tracking to your existing code.

---

## Phase: Add Metrics to PayloadVehicleApp

### Step 1: Include Headers in PayloadVehicleApp.h
```cpp
// Add to includes section:
#include "MetricsManager.h"
#include "EnergyModel.h"
```

### Step 2: Add Member Variables in PayloadVehicleApp.h
```cpp
private:
    // Metrics tracking
    EnergyCalculator energy_calculator;
```

### Step 3: Initialize in PayloadVehicleApp::initialize()

**Location:** After existing resource initialization
```cpp
// Existing line:
cpu_available = par("cpu_per_vehicle");

// Add this:
MetricsManager& metrics = MetricsManager::getInstance();
EV << "Metrics manager initialized" << endl;
```

### Step 4: Record Task Generation

**Location:** In `generateTask()` method (or wherever tasks are created)

```cpp
Task* PayloadVehicleApp::generateTask(TaskType type) {
    // ... existing task creation code ...
    
    // Add THIS LINE after creating task:
    MetricsManager::getInstance().recordTaskGenerated(type);
    
    return task;
}
```

### Step 5: Calculate Energy on Task Completion

**Location:** In `handleTaskCompletion()` method

```cpp
void PayloadVehicleApp::handleTaskCompletion(Task* task) {
    // ... existing completion logic ...
    
    // ADD THIS SECTION:
    // ==================================================================
    // METRICS: Calculate and record energy consumption
    // ==================================================================
    
    double e2e_latency = (simTime() - task->getCreationTime()).dbl();
    
    double energy_consumed = 0.0;
    if (task->execution_location == Task::LOCAL) {
        energy_consumed = energy_calculator.calcLocalExecutionEnergy(
            task->computation.cpu_cycles,
            task->computation.input_size_bytes,
            task->execution_time  // Actual execution time on vehicle
        );
    } else if (task->execution_location == Task::RSU_OFFLOAD) {
        energy_consumed = energy_calculator.calcOffloadEnergy(
            task->computation.input_size_bytes,
            getCurrentLinkRate(),  // Get actual transmission rate
            task->computation.cpu_cycles,
            task->rsu_execution_time  // Time it took at RSU
        );
    }
    
    MetricsManager::getInstance().recordTaskCompletion(
        task->type,
        (task->execution_location == Task::LOCAL) 
            ? MetricsManager::LOCAL_EXECUTION 
            : MetricsManager::RSU_OFFLOAD,
        task->execution_time,
        e2e_latency,
        energy_consumed,
        task->deadline_sec,
        simTime().dbl()
    );
    
    // ==================================================================
    
    // ... rest of existing completion code ...
}
```

### Step 6: Record Task Failures

**Location:** In task timeout/deadline miss handler

```cpp
void PayloadVehicleApp::handleTaskTimeout(Task* task) {
    // ... existing failure logic ...
    
    // ADD THIS:
    double e2e_latency = (simTime() - task->getCreationTime()).dbl();
    MetricsManager::getInstance().recordTaskFailed(task->type, e2e_latency);
    
    // ... rest of code ...
}
```

### Step 7: Record CPU Utilization (Optional but Recommended)

**Location:** At end of each simulation slot or periodically

```cpp
void PayloadVehicleApp::recordCpuStats() {
    // Example: every 0.1 seconds
    uint64_t cycles_used_this_period = cpu_per_vehicle - cpu_available;
    uint64_t cycles_available = cpu_per_vehicle;
    
    MetricsManager::getInstance().recordCPUUtilization(
        cycles_used_this_period,
        cycles_available
    );
    
    // Reschedule this method
    scheduleAt(simTime() + 0.1, new cMessage("cpu_stats_event"));
}
```

### Step 8: Print Metrics at Simulation End

**Location:** In `finish()` method

```cpp
void PayloadVehicleApp::finish() {
    // ... existing finish code ...
    
    // ADD THIS:
    MetricsManager::getInstance().printReport();
    MetricsManager::getInstance().exportToCSV("results/metrics.csv");
    
    // ... rest of code ...
}
```

---

## Integration Summary

### Files to Modify
1. **PayloadVehicleApp.h** - Add includes, member variables
2. **PayloadVehicleApp.cc** - Add initialization, record calls

### Minimal Integration (5 lines)
```cpp
// In generateTask():
MetricsManager::getInstance().recordTaskGenerated(type);

// In handleTaskCompletion():
double energy = energy_calculator.calcLocalExecutionEnergy(...);
MetricsManager::getInstance().recordTaskCompletion(...);
```

### Full Integration (20 lines)
- Initialize in constructor
- Record generation, completion, failure
- Calculate energy for both local and offload
- Print report at end

---

## Testing the Integration

### Test 1: Verify Metrics are Recording
```
Add this to initialize():
    MetricsManager& metrics = MetricsManager::getInstance();
    metrics.recordTaskGenerated(TaskType::COOPERATIVE_PERCEPTION);
    metrics.recordTaskGenerated(TaskType::ROUTE_OPTIMIZATION);
    
    // Simulate completion
    metrics.recordTaskCompletion(
        TaskType::COOPERATIVE_PERCEPTION,
        MetricsManager::LOCAL_EXECUTION,
        0.025,   // exec time
        0.050,   // e2e latency
        1.5e-2,  // energy
        0.150,   // deadline
        simTime().dbl()
    );
    
    metrics.printReport();
```

Expected output should show:
- COOPERATIVE_PERCEPTION: 1 generated, 1 completed on-time
- Energy ~1.5e-2 J
- Latency ~50ms

### Test 2: Verify Energy Calculations
```cpp
EnergyCalculator calc;
auto est = calc.estimateTaskEnergy(TaskType::FLEET_TRAFFIC_FORECAST, 6e6);
cout << "Local: " << est.energy_local_joules << " J" << endl;
cout << "Offload: " << est.energy_offload_joules << " J" << endl;
cout << "Ratio: " << est.energy_savings_ratio << endl;
```

Expected: Offload energy < Local energy (especially for FLEET_TRAFFIC_FORECAST)

### Test 3: End-to-End Verification
Run simulation for 300 seconds, then check:
```
- Total tasks generated > 0 for each task type
- Completion rate 80-95% (depends on load)
- Deadline miss rate < 10% (for normal scenarios)
- Energy stats show offload benefit > 1.5x for heavy tasks
- CSV file is created with all metrics
```

---

## Common Integration Errors

### Error 1: `undefined reference to 'MetricsManager'`
**Fix:** Add `#include "MetricsManager.h"` at top of .cc file and ensure .cc is compiled

### Error 2: Energy values are 0
**Fix:** Ensure `cpu_cycles` and `input_size_bytes` are non-zero from TaskProfile

### Error 3: Segmentation fault on getInstance()
**Fix:** Ensure MetricsManager::getInstance() is called after OMNeT++ initialization

### Error 4: CSV file is empty
**Fix:** Ensure `finish()` is called by OMNeT++ (should be automatic)
      Verify tasks are being recorded with `recordTaskCompletion()`

---

## Minimal Code Needed

If you want MINIMAL integration (just the core 3 metrics):

### PayloadVehicleApp.h (2 lines)
```cpp
#include "MetricsManager.h"
#include "EnergyModel.h"
```

### PayloadVehicleApp.cc (10 lines total)

```cpp
// In generateTask():
MetricsManager::getInstance().recordTaskGenerated(type);

// In handleTaskCompletion():
double energy = energy_calculator.calcLocalExecutionEnergy(
    task->cpu_cycles, task->input_size, task->exec_time);
MetricsManager::getInstance().recordTaskCompletion(
    task->type, LOCAL_EXECUTION, task->exec_time, 
    simTime().dbl() - task->creation.dbl(), energy,
    task->deadline, simTime().dbl());

// In finish():
MetricsManager::getInstance().printReport();
```

---

## Integration Timeline

**Day 1-2:** Add includes and member variables (5 min)
**Day 2-3:** Integrate record calls in task generation (15 min)
**Day 3-4:** Integrate record calls in completion handlers (20 min)
**Day 4-5:** Test and verify metrics output (30 min)

**Total Time: ~1.5-2 hours for full integration**

---

## Next: Run Your First Metrics Experiment

After integration is complete:

1. Run simulation with `workload_intensity = 1.0` (normal load)
2. Check console output for metrics report
3. Open `results/metrics.csv` in Excel/Python
4. Plot: Completion Rate vs Offload Fraction for each task type
5. Compare: Local vs Offload energy savings

Expected Results:
- LOCAL_OBJECT_DETECTION: 100% local (non-offloadable)
- COOPERATIVE_PERCEPTION: 50-70% offload, 2-3x energy savings
- FLEET_TRAFFIC_FORECAST: 90%+ offload, 50x energy savings
- System completion rate: 85-95%
