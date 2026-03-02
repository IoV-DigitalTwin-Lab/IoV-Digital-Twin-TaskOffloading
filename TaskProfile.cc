#include "TaskProfile.h"
#include <stdexcept>

namespace complex_network {

// ============================================================================
// Task Profile Database Initialization
// ============================================================================

TaskProfileDatabase::TaskProfileDatabase() {
    // ========================================================================
    // TASK 1: LOCAL_OBJECT_DETECTION
    // Physics: 160-240M cycles @ 5-7GHz = 23-48ms — fits 80-120ms deadline locally.
    // NOT offloadable (safety-critical, latency dominated by wireless RTT).
    // ========================================================================
    TaskProfile localObjDet;
    localObjDet.type = TaskType::LOCAL_OBJECT_DETECTION;
    localObjDet.name = "Local Object Detection";
    localObjDet.description = "On-board perception: camera/LiDAR object detection";

    localObjDet.computation.input_size_bytes     = static_cast<uint64_t>(1.5e6); // 1.5 MB min
    localObjDet.computation.input_size_bytes_max = static_cast<uint64_t>(2.5e6); // 2.5 MB max
    localObjDet.computation.output_size_bytes    = static_cast<uint64_t>(0.1e6); // 100 KB
    localObjDet.computation.cpu_cycles           = static_cast<uint64_t>(160e6); // 160M min
    localObjDet.computation.cpu_cycles_max       = static_cast<uint64_t>(240e6); // 240M max
    localObjDet.computation.memory_peak_bytes    = 30e6;                          // 30 MB

    localObjDet.timing.deadline_seconds     = 0.080; // 80ms min deadline
    localObjDet.timing.deadline_seconds_max = 0.120; // 120ms max deadline
    localObjDet.timing.qos_value  = QoSValues::LOCAL_OBJECT_DETECTION;
    localObjDet.timing.priority   = PriorityLevel::SAFETY_CRITICAL;

    localObjDet.generation.pattern            = GenerationPattern::PERIODIC;
    localObjDet.generation.period_seconds     = TaskPeriods::LOCAL_OBJECT_DETECTION;
    localObjDet.generation.period_jitter_ratio = 0.05;  // ±5% jitter

    localObjDet.offloading.is_offloadable          = false;  // local only
    localObjDet.offloading.is_safety_critical      = true;
    localObjDet.offloading.offloading_benefit_ratio = 0.0;

    localObjDet.dependencies.depends_on       = TaskType::LOCAL_OBJECT_DETECTION;
    localObjDet.dependencies.dependency_ratio = 0.0;
    localObjDet.dependencies.spawns_children  = {TaskType::COOPERATIVE_PERCEPTION};

    profiles[TaskType::LOCAL_OBJECT_DETECTION] = localObjDet;

    // ========================================================================
    // TASK 2: COOPERATIVE_PERCEPTION
    // Physics: 1.2-1.8G cycles @ 32GHz/6-task-cap = 5.33GHz/task → 225-338ms + 5ms = 230-343ms ≤ 400ms ✓
    //          @ 5-7GHz vehicle = 171-360ms — often exceeds 250ms ⇒ offloading beneficial.
    // ========================================================================
    TaskProfile coopPercep;
    coopPercep.type = TaskType::COOPERATIVE_PERCEPTION;
    coopPercep.name = "Cooperative Perception";
    coopPercep.description = "V2V sensor fusion: aggregate multiple vehicle detections";

    coopPercep.computation.input_size_bytes     = static_cast<uint64_t>(0.2e6); // 200 KB min
    coopPercep.computation.input_size_bytes_max = static_cast<uint64_t>(0.5e6); // 500 KB max
    coopPercep.computation.output_size_bytes    = static_cast<uint64_t>(0.15e6);// 150 KB
    coopPercep.computation.cpu_cycles           = static_cast<uint64_t>(1.2e9); // 1.2G min
    coopPercep.computation.cpu_cycles_max       = static_cast<uint64_t>(1.8e9); // 1.8G max
    coopPercep.computation.memory_peak_bytes    = 20e6;                          // 20 MB

    coopPercep.timing.deadline_seconds     = 0.250; // 250ms min deadline
    coopPercep.timing.deadline_seconds_max = 0.400; // 400ms max deadline
    coopPercep.timing.qos_value  = QoSValues::COOPERATIVE_PERCEPTION;
    coopPercep.timing.priority   = PriorityLevel::HIGH;

    coopPercep.generation.pattern            = GenerationPattern::PERIODIC;
    coopPercep.generation.period_seconds     = TaskPeriods::COOPERATIVE_PERCEPTION;
    coopPercep.generation.period_jitter_ratio = 0.05;  // ±5% jitter

    coopPercep.offloading.is_offloadable          = true;
    coopPercep.offloading.is_safety_critical      = false;
    coopPercep.offloading.offloading_benefit_ratio = 2.0; // RSU 2x faster

    coopPercep.dependencies.depends_on       = TaskType::LOCAL_OBJECT_DETECTION;
    coopPercep.dependencies.dependency_ratio = 0.6;
    coopPercep.dependencies.spawns_children  = {TaskType::ROUTE_OPTIMIZATION};

    profiles[TaskType::COOPERATIVE_PERCEPTION] = coopPercep;

    // ========================================================================
    // TASK 3: ROUTE_OPTIMIZATION
    // Physics: 2.5-3.5G cycles @ 16GHz RSU = 156-219ms + 5ms base = 161-224ms ≤ 1.5s ✓
    //          @ 5-7GHz vehicle = 357-700ms — feasible locally for fast vehicles.
    // ========================================================================
    TaskProfile routeOpt;
    routeOpt.type = TaskType::ROUTE_OPTIMIZATION;
    routeOpt.name = "Route Optimization";
    routeOpt.description = "Path planning: compute optimal route given obstacles & traffic";

    routeOpt.computation.input_size_bytes     = static_cast<uint64_t>(0.8e6); // 800 KB min
    routeOpt.computation.input_size_bytes_max = static_cast<uint64_t>(1.5e6); // 1.5 MB max
    routeOpt.computation.output_size_bytes    = static_cast<uint64_t>(0.2e6); // 200 KB
    routeOpt.computation.cpu_cycles           = static_cast<uint64_t>(2.5e9); // 2.5G min
    routeOpt.computation.cpu_cycles_max       = static_cast<uint64_t>(3.5e9); // 3.5G max
    routeOpt.computation.memory_peak_bytes    = 30e6;                          // 30 MB

    routeOpt.timing.deadline_seconds     = 1.500; // 1.5s min deadline
    routeOpt.timing.deadline_seconds_max = 2.500; // 2.5s max deadline
    routeOpt.timing.qos_value  = QoSValues::ROUTE_OPTIMIZATION;
    routeOpt.timing.priority   = PriorityLevel::MEDIUM;

    routeOpt.generation.pattern            = GenerationPattern::PERIODIC;
    routeOpt.generation.period_seconds     = TaskPeriods::ROUTE_OPTIMIZATION;
    routeOpt.generation.period_jitter_ratio = 0.10;  // ±10% jitter

    routeOpt.offloading.is_offloadable          = true;
    routeOpt.offloading.is_safety_critical      = false;
    routeOpt.offloading.offloading_benefit_ratio = 2.5; // RSU 2.5x faster (full map available)

    routeOpt.dependencies.depends_on       = TaskType::COOPERATIVE_PERCEPTION;
    routeOpt.dependencies.dependency_ratio = 0.8;
    routeOpt.dependencies.spawns_children  = {};

    profiles[TaskType::ROUTE_OPTIMIZATION] = routeOpt;

    // ========================================================================
    // TASK 4: FLEET_TRAFFIC_FORECAST
    // Physics: 15-25G cycles @ 16GHz RSU = 938ms-1.56s + 5ms = well under 5min deadline ✓
    // ========================================================================
    TaskProfile fleetTraffic;
    fleetTraffic.type = TaskType::FLEET_TRAFFIC_FORECAST;
    fleetTraffic.name = "Fleet Traffic Forecast";
    fleetTraffic.description = "Batch analytics: predict citywide traffic using fleet data (LSTM)";

    fleetTraffic.computation.input_size_bytes     = static_cast<uint64_t>(8e6);  // 8 MB min
    fleetTraffic.computation.input_size_bytes_max = static_cast<uint64_t>(15e6); // 15 MB max
    fleetTraffic.computation.output_size_bytes    = static_cast<uint64_t>(1e6);  // 1 MB
    fleetTraffic.computation.cpu_cycles           = static_cast<uint64_t>(15e9); // 15G min
    fleetTraffic.computation.cpu_cycles_max       = static_cast<uint64_t>(25e9); // 25G max
    fleetTraffic.computation.memory_peak_bytes    = 100e6;                        // 100 MB

    fleetTraffic.timing.deadline_seconds     = 240.0; // 4 min min deadline
    fleetTraffic.timing.deadline_seconds_max = 360.0; // 6 min max deadline
    fleetTraffic.timing.qos_value  = QoSValues::FLEET_TRAFFIC_FORECAST;
    fleetTraffic.timing.priority   = PriorityLevel::LOW;

    fleetTraffic.generation.pattern                = GenerationPattern::BATCH;
    fleetTraffic.generation.batch_interval_seconds = TaskPeriods::FLEET_TRAFFIC_BATCH; // 60s

    fleetTraffic.offloading.is_offloadable          = true;
    fleetTraffic.offloading.is_safety_critical      = false;
    fleetTraffic.offloading.offloading_benefit_ratio = 10.0; // 10x (GPU + ML optimised on RSU)

    fleetTraffic.dependencies.depends_on       = TaskType::FLEET_TRAFFIC_FORECAST;
    fleetTraffic.dependencies.dependency_ratio = 0.0;
    fleetTraffic.dependencies.spawns_children  = {};

    profiles[TaskType::FLEET_TRAFFIC_FORECAST] = fleetTraffic;

    // ========================================================================
    // TASK 5: VOICE_COMMAND_PROCESSING
    // Physics: 350-650M cycles @ 16GHz RSU = 22-41ms + 5ms = 27-46ms ≤ 500ms ✓
    //          @ 5-7GHz vehicle = 50-130ms ≤ 500ms – feasible locally too.
    // ========================================================================
    TaskProfile voiceCmd;
    voiceCmd.type = TaskType::VOICE_COMMAND_PROCESSING;
    voiceCmd.name = "Voice Command Processing";
    voiceCmd.description = "User interaction: speech-to-text and intent recognition (Poisson arrival)";

    voiceCmd.computation.input_size_bytes     = static_cast<uint64_t>(0.15e6); // 150 KB min
    voiceCmd.computation.input_size_bytes_max = static_cast<uint64_t>(0.30e6); // 300 KB max
    voiceCmd.computation.output_size_bytes    = static_cast<uint64_t>(0.05e6); // 50 KB
    voiceCmd.computation.cpu_cycles           = static_cast<uint64_t>(350e6);  // 350M min
    voiceCmd.computation.cpu_cycles_max       = static_cast<uint64_t>(650e6);  // 650M max
    voiceCmd.computation.memory_peak_bytes    = 10e6;                           // 10 MB

    voiceCmd.timing.deadline_seconds     = 0.400; // 400ms min deadline
    voiceCmd.timing.deadline_seconds_max = 0.600; // 600ms max deadline
    voiceCmd.timing.qos_value  = QoSValues::VOICE_COMMAND_PROCESSING;
    voiceCmd.timing.priority   = PriorityLevel::MEDIUM;

    voiceCmd.generation.pattern = GenerationPattern::POISSON;
    voiceCmd.generation.lambda  = TaskRates::VOICE_COMMAND_PROCESSING; // λ = 0.2 (1/5s)

    voiceCmd.offloading.is_offloadable          = true;
    voiceCmd.offloading.is_safety_critical      = false;
    voiceCmd.offloading.offloading_benefit_ratio = 1.5;

    voiceCmd.dependencies.depends_on       = TaskType::VOICE_COMMAND_PROCESSING;
    voiceCmd.dependencies.dependency_ratio = 0.0;
    voiceCmd.dependencies.spawns_children  = {};

    profiles[TaskType::VOICE_COMMAND_PROCESSING] = voiceCmd;

    // ========================================================================
    // TASK 6: SENSOR_HEALTH_CHECK
    // Physics: 80-150M cycles @ 16GHz RSU = 5-9ms + 5ms = 10-14ms ≤ 10s deadline ✓
    // ========================================================================
    TaskProfile sensorHealth;
    sensorHealth.type = TaskType::SENSOR_HEALTH_CHECK;
    sensorHealth.name = "Sensor Health Check";
    sensorHealth.description = "Background maintenance: periodic sensor diagnostics and calibration";

    sensorHealth.computation.input_size_bytes     = static_cast<uint64_t>(0.08e6); // 80 KB min
    sensorHealth.computation.input_size_bytes_max = static_cast<uint64_t>(0.15e6); // 150 KB max
    sensorHealth.computation.output_size_bytes    = static_cast<uint64_t>(0.05e6); // 50 KB
    sensorHealth.computation.cpu_cycles           = static_cast<uint64_t>(80e6);   // 80M min
    sensorHealth.computation.cpu_cycles_max       = static_cast<uint64_t>(150e6);  // 150M max
    sensorHealth.computation.memory_peak_bytes    = 5e6;                            // 5 MB

    sensorHealth.timing.deadline_seconds     = 8.0;  // 8s min deadline
    sensorHealth.timing.deadline_seconds_max = 12.0; // 12s max deadline
    sensorHealth.timing.qos_value  = QoSValues::SENSOR_HEALTH_CHECK;
    sensorHealth.timing.priority   = PriorityLevel::BACKGROUND;

    sensorHealth.generation.pattern            = GenerationPattern::PERIODIC;
    sensorHealth.generation.period_seconds     = TaskPeriods::SENSOR_HEALTH_CHECK;
    sensorHealth.generation.period_jitter_ratio = 0.15;  // ±15% jitter (very flexible)

    sensorHealth.offloading.is_offloadable          = true;
    sensorHealth.offloading.is_safety_critical      = false;
    sensorHealth.offloading.offloading_benefit_ratio = 1.0;

    sensorHealth.dependencies.depends_on       = TaskType::SENSOR_HEALTH_CHECK;
    sensorHealth.dependencies.dependency_ratio = 0.0;
    sensorHealth.dependencies.spawns_children  = {};

    profiles[TaskType::SENSOR_HEALTH_CHECK] = sensorHealth;
}

// ============================================================================
// Database Access Methods
// ============================================================================

const TaskProfile& TaskProfileDatabase::getProfile(TaskType type) const {
    auto it = profiles.find(type);
    if (it == profiles.end()) {
        throw std::runtime_error("Unknown task type requested");
    }
    return it->second;
}

std::string TaskProfileDatabase::getTaskTypeName(TaskType type) {
    switch (type) {
        case TaskType::LOCAL_OBJECT_DETECTION:
            return "LOCAL_OBJECT_DETECTION";
        case TaskType::COOPERATIVE_PERCEPTION:
            return "COOPERATIVE_PERCEPTION";
        case TaskType::ROUTE_OPTIMIZATION:
            return "ROUTE_OPTIMIZATION";
        case TaskType::FLEET_TRAFFIC_FORECAST:
            return "FLEET_TRAFFIC_FORECAST";
        case TaskType::VOICE_COMMAND_PROCESSING:
            return "VOICE_COMMAND_PROCESSING";
        case TaskType::SENSOR_HEALTH_CHECK:
            return "SENSOR_HEALTH_CHECK";
        default:
            return "UNKNOWN";
    }
}

const std::vector<TaskType>& TaskProfileDatabase::getAllTaskTypes() {
    static const std::vector<TaskType> allTypes = {
        TaskType::LOCAL_OBJECT_DETECTION,
        TaskType::COOPERATIVE_PERCEPTION,
        TaskType::ROUTE_OPTIMIZATION,
        TaskType::FLEET_TRAFFIC_FORECAST,
        TaskType::VOICE_COMMAND_PROCESSING,
        TaskType::SENSOR_HEALTH_CHECK
    };
    return allTypes;
}

} // namespace complex_network
