#include "TaskProfile.h"
#include <stdexcept>

namespace complex_network {

// ============================================================================
// Task Profile Database Initialization
// ============================================================================

TaskProfileDatabase::TaskProfileDatabase() {
    // ========================================================================
    // TASK 1: LOCAL_OBJECT_DETECTION
    // ========================================================================
    TaskProfile localObjDet;
    localObjDet.type = TaskType::LOCAL_OBJECT_DETECTION;
    localObjDet.name = "Local Object Detection";
    localObjDet.description = "On-board perception: camera/LiDAR object detection";
    
    localObjDet.computation.input_size_bytes = 2e6;          // 2 MB
    localObjDet.computation.output_size_bytes = 0.1e6;       // 100 KB
    localObjDet.computation.cpu_cycles = 5e8;                // 500M cycles
    localObjDet.computation.memory_peak_bytes = 50e6;        // 50 MB
    
    localObjDet.timing.deadline_seconds = 0.050;             // 50ms hard deadline
    localObjDet.timing.qos_value = QoSValues::LOCAL_OBJECT_DETECTION;
    localObjDet.timing.priority = PriorityLevel::SAFETY_CRITICAL;
    
    localObjDet.generation.pattern = GenerationPattern::PERIODIC;
    localObjDet.generation.period_seconds = TaskPeriods::LOCAL_OBJECT_DETECTION;
    localObjDet.generation.period_jitter_ratio = 0.01;       // ±1% jitter
    
    localObjDet.offloading.is_offloadable = false;           // CANNOT offload
    localObjDet.offloading.is_safety_critical = true;
    localObjDet.offloading.offloading_benefit_ratio = 0.0;   // N/A
    
    localObjDet.dependencies.depends_on = TaskType::LOCAL_OBJECT_DETECTION; // Self (placeholder)
    localObjDet.dependencies.dependency_ratio = 0.0;
    localObjDet.dependencies.spawns_children = {TaskType::COOPERATIVE_PERCEPTION};
    
    profiles[TaskType::LOCAL_OBJECT_DETECTION] = localObjDet;

    // ========================================================================
    // TASK 2: COOPERATIVE_PERCEPTION
    // ========================================================================
    TaskProfile coopPercep;
    coopPercep.type = TaskType::COOPERATIVE_PERCEPTION;
    coopPercep.name = "Cooperative Perception";
    coopPercep.description = "V2V sensor fusion: aggregate multiple vehicle detections";
    
    coopPercep.computation.input_size_bytes = 0.3e6;         // 300 KB
    coopPercep.computation.output_size_bytes = 0.15e6;       // 150 KB
    coopPercep.computation.cpu_cycles = 2.5e9;               // 2.5G cycles
    coopPercep.computation.memory_peak_bytes = 20e6;         // 20 MB
    
    coopPercep.timing.deadline_seconds = 0.150;              // 150ms
    coopPercep.timing.qos_value = QoSValues::COOPERATIVE_PERCEPTION;
    coopPercep.timing.priority = PriorityLevel::HIGH;
    
    coopPercep.generation.pattern = GenerationPattern::PERIODIC;
    coopPercep.generation.period_seconds = TaskPeriods::COOPERATIVE_PERCEPTION;
    coopPercep.generation.period_jitter_ratio = 0.02;        // ±2% jitter
    
    coopPercep.offloading.is_offloadable = true;
    coopPercep.offloading.is_safety_critical = false;
    coopPercep.offloading.offloading_benefit_ratio = 1.5;    // 1.5x faster on RSU
    
    coopPercep.dependencies.depends_on = TaskType::LOCAL_OBJECT_DETECTION;
    coopPercep.dependencies.dependency_ratio = 0.6;           // Needs 60% of detection output
    coopPercep.dependencies.spawns_children = {TaskType::ROUTE_OPTIMIZATION};
    
    profiles[TaskType::COOPERATIVE_PERCEPTION] = coopPercep;

    // ========================================================================
    // TASK 3: ROUTE_OPTIMIZATION
    // ========================================================================
    TaskProfile routeOpt;
    routeOpt.type = TaskType::ROUTE_OPTIMIZATION;
    routeOpt.name = "Route Optimization";
    routeOpt.description = "Path planning: compute optimal route given obstacles & traffic";
    
    routeOpt.computation.input_size_bytes = 1e6;             // 1 MB
    routeOpt.computation.output_size_bytes = 0.2e6;          // 200 KB
    routeOpt.computation.cpu_cycles = 5e9;                   // 5G cycles
    routeOpt.computation.memory_peak_bytes = 30e6;           // 30 MB
    
    routeOpt.timing.deadline_seconds = 1.0;                  // 1 second soft deadline
    routeOpt.timing.qos_value = QoSValues::ROUTE_OPTIMIZATION;
    routeOpt.timing.priority = PriorityLevel::MEDIUM;
    
    routeOpt.generation.pattern = GenerationPattern::PERIODIC;
    routeOpt.generation.period_seconds = TaskPeriods::ROUTE_OPTIMIZATION;
    routeOpt.generation.period_jitter_ratio = 0.05;          // ±5% jitter
    
    routeOpt.offloading.is_offloadable = true;
    routeOpt.offloading.is_safety_critical = false;
    routeOpt.offloading.offloading_benefit_ratio = 2.0;      // 2x faster on RSU (complete map)
    
    routeOpt.dependencies.depends_on = TaskType::COOPERATIVE_PERCEPTION;
    routeOpt.dependencies.dependency_ratio = 0.8;             // Needs 80% of perception output
    routeOpt.dependencies.spawns_children = {};
    
    profiles[TaskType::ROUTE_OPTIMIZATION] = routeOpt;

    // ========================================================================
    // TASK 4: FLEET_TRAFFIC_FORECAST
    // ========================================================================
    TaskProfile fleetTraffic;
    fleetTraffic.type = TaskType::FLEET_TRAFFIC_FORECAST;
    fleetTraffic.name = "Fleet Traffic Forecast";
    fleetTraffic.description = "Batch analytics: predict citywide traffic using fleet data (LSTM)";
    
    fleetTraffic.computation.input_size_bytes = 10e6;        // 10 MB
    fleetTraffic.computation.output_size_bytes = 1e6;        // 1 MB
    fleetTraffic.computation.cpu_cycles = 2e10;              // 20G cycles (LSTM = heavy)
    fleetTraffic.computation.memory_peak_bytes = 100e6;      // 100 MB
    
    fleetTraffic.timing.deadline_seconds = 300.0;            // 5 minutes soft deadline
    fleetTraffic.timing.qos_value = QoSValues::FLEET_TRAFFIC_FORECAST;
    fleetTraffic.timing.priority = PriorityLevel::LOW;
    
    fleetTraffic.generation.pattern = GenerationPattern::BATCH;
    fleetTraffic.generation.batch_interval_seconds = TaskPeriods::FLEET_TRAFFIC_BATCH; // 60s
    
    fleetTraffic.offloading.is_offloadable = true;
    fleetTraffic.offloading.is_safety_critical = false;
    fleetTraffic.offloading.offloading_benefit_ratio = 10.0;  // 10x faster on RSU (GPU, ML optimized)
    fleetTraffic.offloading.is_offloadable = true;            // Note: Must offload (too heavy)
    
    fleetTraffic.dependencies.depends_on = TaskType::FLEET_TRAFFIC_FORECAST; // Self (placeholder)
    fleetTraffic.dependencies.dependency_ratio = 0.0;
    fleetTraffic.dependencies.spawns_children = {};
    
    profiles[TaskType::FLEET_TRAFFIC_FORECAST] = fleetTraffic;

    // ========================================================================
    // TASK 5: VOICE_COMMAND_PROCESSING
    // ========================================================================
    TaskProfile voiceCmd;
    voiceCmd.type = TaskType::VOICE_COMMAND_PROCESSING;
    voiceCmd.name = "Voice Command Processing";
    voiceCmd.description = "User interaction: speech-to-text and intent recognition (Poisson arrival)";
    
    voiceCmd.computation.input_size_bytes = 0.2e6;           // 200 KB (audio)
    voiceCmd.computation.output_size_bytes = 0.05e6;         // 50 KB (recognized command)
    voiceCmd.computation.cpu_cycles = 1e9;                   // 1G cycles
    voiceCmd.computation.memory_peak_bytes = 10e6;           // 10 MB
    
    voiceCmd.timing.deadline_seconds = 0.500;                // 500ms (interactive response)
    voiceCmd.timing.qos_value = QoSValues::VOICE_COMMAND_PROCESSING;
    voiceCmd.timing.priority = PriorityLevel::MEDIUM;
    
    voiceCmd.generation.pattern = GenerationPattern::POISSON;
    voiceCmd.generation.lambda = TaskRates::VOICE_COMMAND_PROCESSING; // λ = 0.2
    
    voiceCmd.offloading.is_offloadable = true;
    voiceCmd.offloading.is_safety_critical = false;
    voiceCmd.offloading.offloading_benefit_ratio = 1.5;      // 1.5x more accurate on cloud
    
    voiceCmd.dependencies.depends_on = TaskType::VOICE_COMMAND_PROCESSING; // Self (placeholder)
    voiceCmd.dependencies.dependency_ratio = 0.0;
    voiceCmd.dependencies.spawns_children = {};
    
    profiles[TaskType::VOICE_COMMAND_PROCESSING] = voiceCmd;

    // ========================================================================
    // TASK 6: SENSOR_HEALTH_CHECK
    // ========================================================================
    TaskProfile sensorHealth;
    sensorHealth.type = TaskType::SENSOR_HEALTH_CHECK;
    sensorHealth.name = "Sensor Health Check";
    sensorHealth.description = "Background maintenance: periodic sensor diagnostics and calibration";
    
    sensorHealth.computation.input_size_bytes = 0.1e6;       // 100 KB
    sensorHealth.computation.output_size_bytes = 0.05e6;     // 50 KB
    sensorHealth.computation.cpu_cycles = 1e8;               // 100M cycles
    sensorHealth.computation.memory_peak_bytes = 5e6;        // 5 MB
    
    sensorHealth.timing.deadline_seconds = 10.0;             // 10 seconds (very relaxed)
    sensorHealth.timing.qos_value = QoSValues::SENSOR_HEALTH_CHECK;
    sensorHealth.timing.priority = PriorityLevel::BACKGROUND;
    
    sensorHealth.generation.pattern = GenerationPattern::PERIODIC;
    sensorHealth.generation.period_seconds = TaskPeriods::SENSOR_HEALTH_CHECK;
    sensorHealth.generation.period_jitter_ratio = 0.1;       // ±10% jitter (very flexible)
    
    sensorHealth.offloading.is_offloadable = true;
    sensorHealth.offloading.is_safety_critical = false;
    sensorHealth.offloading.offloading_benefit_ratio = 1.0;  // No benefit (minimal workload)
    
    sensorHealth.dependencies.depends_on = TaskType::SENSOR_HEALTH_CHECK; // Self (placeholder)
    sensorHealth.dependencies.dependency_ratio = 0.0;
    sensorHealth.dependencies.spawns_children = {};
    
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
