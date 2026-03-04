# Event-Driven RSU Task Processing Implementation

## Overview

The RSU task processing has been refactored from a placeholder stub (instant 100ms hardcoding) to a realistic **event-driven compute simulation** using `cpu_cycles` and `task_size_bytes` as abstract workload signals.

## Architecture

### Key Data Structure: `RSUActiveTask`

```cpp
struct RSUActiveTask {
    std::string task_id;
    std::string vehicle_id;
    LAddress::L2Type vehicle_mac;
    
    uint64_t cpu_cycles;        // Task computational workload
    uint64_t task_size_bytes;   // Task data size
    double deadline_seconds;    // Hard deadline
    
    double arrival_time;        // When arrived at RSU queue
    double start_time;          // When processing began
    double allocated_cpu_ghz;   // Dynamic CPU share
    double estimated_completion_time;
    
    enum State { QUEUED, PROCESSING, COMPLETED, FAILED } state;
    cMessage* completion_msg;   // Self-message for completion event
};
```

### Processing Flow

```
1. Task Arrives (handleTaskOffloadPacket)
   └─> processTaskOnRSU()
       ├─> Check capacity (queue + processing count)
       ├─> Create RSUActiveTask
       └─> queueTaskForProcessing()
       
2. Queue Management (processNextQueuedTask)
   ├─> Dequeue FIFO if processing capacity available
   ├─> Allocate CPU: cpu_per_task = available_cpu / (count + 1)
   ├─> Calculate service time using cpu_cycles
   └─> Schedule completion event
   
3. Completion (handleTaskCompletionEvent)
   ├─> Record timing metrics (queue wait + compute time)
   ├─> Check deadline (completed_on_time flag)
   ├─> Deallocate resources
   ├─> Send result to vehicle
   └─> Process next queued task
```

## Processing Time Calculation

**Service time** is computed from abstract task parameters:

$$t_{service} = \underbrace{\frac{\text{cpu\_cycles}}{\text{allocated\_ghz} \times 10^9}}_{\text{compute}} + \underbrace{\frac{\text{size\_kb}}{100000}}_{\text{I/O overhead}} + \underbrace{\frac{\text{processingDelay\_ms}}{1000}}_{\text{base overhead}}$$

**Example:**
- Task: 1000 GCycles, 2000 KB, allocated 2 GHz
- $t_{compute} = 1000 \times 10^9 / (2 \times 10^9) = 0.5$ s
- $t_{io} = 2000 / 100000 = 0.02$ s  
- $t_{base} = 10 / 1000 = 0.01$ s
- **Total ≈ 0.53 s**

## Resource Management

### CPU Allocation Strategy

- **Fair Share**: When multiple tasks are processing, available CPU is divided equally
- **Recalculation**: CPU share is recalculated when tasks start/complete
- **Headroom Reserve**: 20% CPU reserved for system operations when tasks are processing

```cpp
allocated_cpu = available_cpu / (processing_count + 1)
available_cpu = total_cpu - (processing_count * allocated_cpu)
```

### Queue Management

- **FIFO ordering**: Tasks processed in arrival order
- **Capacity limit**: `rsu_max_concurrent` (default 10 tasks)
- **Backpressure**: Tasks queued when limit reached (configurable as reject vs queue)

## Metrics Tracked

Per task:
- `queue_wait_time`: Seconds spent waiting in queue
- `processing_time`: Actual compute + I/O time
- `total_time`: Queue + processing
- `deadline_met`: Boolean flag (total_time ≤ deadline_seconds)

RSU aggregates:
- `rsu_tasks_processed`: Count of completed tasks
- `rsu_total_processing_time`: Sum of all processing times
- `rsu_total_queue_time`: Sum of all queue waits
- `rsu_processing_count`: Current count of active tasks
- `rsu_queue_length`: Current queue depth

## Console Output Format

```
RSU_QUEUE: Task queued, queue_depth=2
RSU_PROCESS_START: Task v30_T0_0.149621 started (queue_depth=1, processing=2)
RSU_COMPLETE: Task v30_T0_0.149621 completed in 0.52 s (queue=0.03 s, deadline=MET)
```

## Configuration Parameters (omnetpp.ini)

```ini
*.rsu[*].appl.edgeCPU_GHz = 8.0              # RSU CPU capacity
*.rsu[*].appl.maxVehicles = 10               # Max concurrent tasks
*.rsu[*].appl.processingDelay_ms = 10        # Base overhead
```

## Testing Notes

- Simulation should now run **significantly faster** than before
- Database timeouts were the main bottleneck; now CPU time drives simulation speed
- Each task completion triggers automatic next-task processing (no idle gaps)
- Event log will show realistic task timing and deadline compliance

## Future Enhancements

1. **Per-task CPU tracking**: Store allocated CPU for each task instead of recalculating
2. **Priority queues**: Support task priorities (QoS-based)
3. **Preemption**: Allow high-priority tasks to preempt low-priority ones
4. **DRL integration**: Feed real queue/CPU metrics to ML model for better decision-making
5. **Memory simulation**: Add memory allocation/deallocation tracking
6. **Power model**: Track energy consumption based on CPU utilization

## Files Modified

- `MyRSUApp.h`: Added `RSUActiveTask` struct, queue/tracking members, method declarations
- `MyRSUApp.cc`: Implemented event-driven processing core logic
