# QUICK START: From Design to Code (5 Days)

## Day 1: Understand the Design

**Morning (2 hours):**
- [ ] Read [SIMPLIFIED_TASK_TAXONOMY.md](SIMPLIFIED_TASK_TAXONOMY.md) - Understand the 6 tasks
- [ ] Read [TASK_TAXONOMY_VISUAL_SUMMARY.md](TASK_TAXONOMY_VISUAL_SUMMARY.md) - See the metrics

**Afternoon (2 hours):**
- [ ] Review [TaskProfile.h](TaskProfile.h) - See the C++ structures
- [ ] Review [TaskProfile.cc](TaskProfile.cc) - See the 6 profiles

**Result:** You understand what each task does and why it matters.

---

## Day 2-3: Integrate TaskProfile into Existing Code

**Step 1: Update Task.h**
```cpp
// Add to Task class:
class Task {
    // ... existing fields ...
    
    // NEW FIELDS:
    TaskType task_type = TaskType::LOCAL_OBJECT_DETECTION;
    PriorityLevel priority_level = PriorityLevel::MEDIUM;
    bool is_offloadable = true;
    
    // Factory method:
    static Task* createFromProfile(TaskType type, const std::string& vehicle_id, 
                                   uint64_t seq_num);
};
```

**Step 2: Update Task.cc**
```cpp
// Add factory method implementation:
Task* Task::createFromProfile(TaskType type, const std::string& vehicle_id, uint64_t seq_num) {
    const auto& profile = TaskProfileDatabase::getInstance().getProfile(type);
    
    Task* task = new Task(
        vehicle_id,
        seq_num,
        profile.computation.input_size_bytes,
        profile.computation.cpu_cycles,
        profile.timing.deadline_seconds,
        profile.timing.qos_value
    );
    
    task->task_type = type;
    task->priority_level = profile.timing.priority;
    task->is_offloadable = profile.offloading.is_offloadable;
    
    return task;
}
```

**Step 3: Update PayloadVehicleApp.h**
```cpp
// Add includes and data members to PayloadVehicleApp:

#include "TaskProfile.h"

class PayloadVehicleApp : public DemoBaseApplLayer {
private:
    // Task generation events (one per task type)
    cMessage* localObjDetEvent = nullptr;
    cMessage* coopPercepEvent = nullptr;
    cMessage* routeOptEvent = nullptr;
    cMessage* fleetForecastEvent = nullptr;
    cMessage* voiceCommandEvent = nullptr;
    cMessage* sensorHealthEvent = nullptr;
    
    // Task profile database reference
    TaskProfileDatabase* profileDB = nullptr;
    
    // Methods to add:
    void initializeTaskGeneration();
    void handleLocalObjDetEvent();
    void handleCoopPercepEvent();
    // ... one handler per task type ...
    void schedulePeriodicTask(TaskType type);
    void schedulePoissonTask(TaskType type);
    void scheduleBatchTask(TaskType type);
};
```

**Step 4: Update PayloadVehicleApp.cc - initialize()**
```cpp
void PayloadVehicleApp::initialize(int stage) {
    DemoBaseApplLayer::initialize(stage);
    
    if (stage != 0) return;
    
    // Existing initialization...
    
    // NEW: Initialize task generation
    profileDB = &TaskProfileDatabase::getInstance();
    initializeTaskGeneration();
}

void PayloadVehicleApp::initializeTaskGeneration() {
    // Create self-messages for each task type
    localObjDetEvent = new cMessage("localObjDet", LOCAL_OBJECT_DETECTION);
    coopPercepEvent = new cMessage("coopPercep", COOPERATIVE_PERCEPTION);
    routeOptEvent = new cMessage("routeOpt", ROUTE_OPTIMIZATION);
    fleetForecastEvent = new cMessage("fleetForecast", FLEET_TRAFFIC_FORECAST);
    voiceCommandEvent = new cMessage("voiceCmd", VOICE_COMMAND_PROCESSING);
    sensorHealthEvent = new cMessage("sensorHealth", SENSOR_HEALTH_CHECK);
    
    // Schedule all initial events
    schedulePeriodicTask(TaskType::LOCAL_OBJECT_DETECTION);
    schedulePeriodicTask(TaskType::COOPERATIVE_PERCEPTION);
    schedulePeriodicTask(TaskType::ROUTE_OPTIMIZATION);
    scheduleBatchTask(TaskType::FLEET_TRAFFIC_FORECAST);
    schedulePoissonTask(TaskType::VOICE_COMMAND_PROCESSING);
    schedulePeriodicTask(TaskType::SENSOR_HEALTH_CHECK);
}
```

**Step 5: Implement Task Generation Methods**
```cpp
void PayloadVehicleApp::schedulePeriodicTask(TaskType type) {
    const auto& profile = profileDB->getProfile(type);
    cMessage* event = nullptr;
    
    switch(type) {
        case TaskType::LOCAL_OBJECT_DETECTION: event = localObjDetEvent; break;
        case TaskType::COOPERATIVE_PERCEPTION: event = coopPercepEvent; break;
        case TaskType::ROUTE_OPTIMIZATION: event = routeOptEvent; break;
        case TaskType::SENSOR_HEALTH_CHECK: event = sensorHealthEvent; break;
        default: return;
    }
    
    // Apply jitter
    double jitter = profile.generation.period_seconds * profile.generation.period_jitter_ratio;
    double actual_period = profile.generation.period_seconds + uniform(-jitter, jitter);
    
    scheduleAt(simTime() + actual_period, event);
}

void PayloadVehicleApp::schedulePoissonTask(TaskType type) {
    const auto& profile = profileDB->getProfile(type);
    cMessage* event = nullptr;
    
    if (type == TaskType::VOICE_COMMAND_PROCESSING) {
        event = voiceCommandEvent;
    }
    
    double next_arrival = exponential(1.0 / profile.generation.lambda);
    scheduleAt(simTime() + next_arrival, event);
}

void PayloadVehicleApp::scheduleBatchTask(TaskType type) {
    const auto& profile = profileDB->getProfile(type);
    
    if (type == TaskType::FLEET_TRAFFIC_FORECAST) {
        scheduleAt(simTime() + profile.generation.batch_interval_seconds, fleetForecastEvent);
    }
}
```

**Step 6: Update handleSelfMsg()**
```cpp
void PayloadVehicleApp::handleSelfMsg(cMessage* msg) {
    if (msg == localObjDetEvent) {
        generateTask(TaskType::LOCAL_OBJECT_DETECTION);
        schedulePeriodicTask(TaskType::LOCAL_OBJECT_DETECTION);
        return;
    }
    else if (msg == coopPercepEvent) {
        generateTask(TaskType::COOPERATIVE_PERCEPTION);
        schedulePeriodicTask(TaskType::COOPERATIVE_PERCEPTION);
        return;
    }
    // ... handle other task types ...
    else if (msg == voiceCommandEvent) {
        generateTask(TaskType::VOICE_COMMAND_PROCESSING);
        schedulePoissonTask(TaskType::VOICE_COMMAND_PROCESSING);
        return;
    }
    // ... existing message handling ...
    DemoBaseApplLayer::handleSelfMsg(msg);
}
```

**Step 7: Implement generateTask()**
```cpp
void PayloadVehicleApp::generateTask(TaskType type) {
    const auto& profile = profileDB->getProfile(type);
    
    // Create task from profile
    Task* task = Task::createFromProfile(type, std::to_string(getParentModule()->getIndex()), 
                                          task_sequence_number++);
    
    EV_INFO << "Generated task: " << profile.name << " (ID: " << task->task_id << ")" << endl;
    
    // Add to queue
    pending_tasks.push(task);
    tasks_generated++;
}
```

**Compile & Test:**
```bash
cd /path/to/project
make clean
make
# Should compile without errors
```

---

## Day 4: Test Task Generation

**Create omnetpp.ini configuration:**
```ini
# Test configuration
*.node[*].appl.workload_intensity = 1.0

# Run simulation for 120 seconds
sim-time-limit = 120s

# Enable detailed logging
**.cmdenv-log-level = info
```

**Run simulation and check logs:**
```
Expected output:
- LOCAL_OBJECT_DETECTION every 50ms
- COOPERATIVE_PERCEPTION every 100ms (depends on LOCAL_OBJ_DET)
- ROUTE_OPTIMIZATION every 500ms
- VOICE_COMMAND sporadic (Poisson)
- FLEET_TRAFFIC_FORECAST every 60s
- SENSOR_HEALTH every 10s
```

---

## Day 5: Add Task Dependencies & Metrics

**Implement dependency spawning:**
```cpp
void PayloadVehicleApp::handleTaskCompletion(Task* task) {
    // ... existing completion logic ...
    
    // NEW: Spawn child tasks based on dependency definition
    const auto& profile = profileDB->getProfile(task->task_type);
    for (TaskType child_type : profile.dependencies.spawns_children) {
        generateTask(child_type);
        EV_INFO << "Task " << task->task_id << " spawned child: " 
                << taskTypeName(child_type) << endl;
    }
}
```

**Add per-task metrics:**
```cpp
// In PayloadVehicleApp.h:
std::map<TaskType, uint32_t> task_completions_by_type;
std::map<TaskType, uint32_t> task_misses_by_type;
std::map<TaskType, double> task_avg_latency_by_type;

// When task completes:
task_completions_by_type[task->task_type]++;
task_avg_latency_by_type[task->task_type] += (simTime() - task->created_time).dbl();
```

---

## Result After 5 Days:

✅ TaskProfile integrated into code  
✅ All 6 task types generating correctly  
✅ Periodic tasks at fixed intervals  
✅ Poisson task with random arrival  
✅ Batch job running periodically  
✅ Task dependencies spawning children  
✅ Metrics collected per task type  
✅ Ready for offloading decision experiments  

---

## Next: Run Experiments (Week 2-3)

**Scenario 1: Light load (50%)**
```ini
*.node[*].appl.workload_intensity = 0.5
sim-time-limit = 300s
# Measure: Task completion rate, CPU usage
```

**Scenario 2: Normal load (100%)**
```ini
*.node[*].appl.workload_intensity = 1.0
sim-time-limit = 300s
# Measure: Same metrics
```

**Scenario 3: Peak load (200%)**
```ini
*.node[*].appl.workload_intensity = 2.0
sim-time-limit = 300s
# Measure: Deadline misses increase? ← Shows offloading benefit
```

---

## Success Criteria:

✅ Tasks generate at right times  
✅ Dependencies work (parent → child)  
✅ Metrics show task-type breakdown  
✅ Workload intensity multiplier works  
✅ Peak load shows deadline misses → justifies offloading  

You're ready! 🚀
