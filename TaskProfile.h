#ifndef TASK_PROFILE_H_
#define TASK_PROFILE_H_

#include <string>
#include <map>
#include <vector>

namespace complex_network {

// ============================================================================
// SIMPLIFIED TASK TAXONOMY - 6 Core Task Types
// ============================================================================

/**
 * Task Type Enumeration
 * 6 types covering safety, perception, navigation, analytics, interaction, maintenance
 */
enum class TaskType {
    LOCAL_OBJECT_DETECTION = 0,      // Safety-critical perception (periodic)
    COOPERATIVE_PERCEPTION = 1,      // V2V sensor fusion (periodic)
    ROUTE_OPTIMIZATION = 2,          // Path planning (periodic)
    FLEET_TRAFFIC_FORECAST = 3,      // ML analytics batch job (periodic batch)
    VOICE_COMMAND_PROCESSING = 4,    // User voice commands (Poisson)
    SENSOR_HEALTH_CHECK = 5          // Background diagnostics (periodic)
};

/**
 * Task Generation Pattern
 * Determines how tasks arrive in the system
 */
enum class GenerationPattern {
    PERIODIC,                         // Fixed interval (deterministic)
    POISSON,                          // Random exponential inter-arrival
    BATCH                             // Batch processing (periodic collection)
};

/**
 * Priority Level (maps to QoS value)
 * Higher priority = should execute locally if possible
 */
enum class PriorityLevel {
    SAFETY_CRITICAL = 0,              // QoS 0.90-1.00 (local only)
    HIGH = 1,                         // QoS 0.70-0.89 (prefer local)
    MEDIUM = 2,                       // QoS 0.50-0.69 (hybrid)
    LOW = 3,                          // QoS 0.20-0.49 (offload if beneficial)
    BACKGROUND = 4                    // QoS 0.00-0.19 (defer or drop)
};

/**
 * Task Profile - Static characteristics for each task type
 * Defines the computational and timing properties of each task
 */
struct TaskProfile {
    // Identity
    TaskType type;
    std::string name;                 // Human-readable name
    std::string description;

    // Computational Characteristics
    struct {
        uint64_t input_size_bytes;    // Input data size
        uint64_t output_size_bytes;   // Output data size
        uint64_t cpu_cycles;          // CPU cycles required
        double memory_peak_bytes;     // Peak memory during execution
    } computation;

    // Timing Characteristics
    struct {
        double deadline_seconds;      // Hard/soft deadline
        double qos_value;             // QoS score (0.0-1.0)
        PriorityLevel priority;
    } timing;

    // Generation Pattern
    struct {
        GenerationPattern pattern;    // PERIODIC, POISSON, or BATCH
        // For PERIODIC:
        double period_seconds;        // Fixed interval
        double period_jitter_ratio;   // ±jitter as percentage (0.0-1.0)
        // For POISSON:
        double lambda;                // Rate parameter (tasks/second)
        // For BATCH:
        double batch_interval_seconds; // Batch collection interval
    } generation;

    // Offloading Characteristics
    struct {
        bool is_offloadable;          // Can be sent to RSU?
        bool is_safety_critical;      // Safety functions (cannot offload)
        double offloading_benefit_ratio; // Time saved if offloaded (1.0-20.0x)
    } offloading;

    // Dependency Information
    struct {
        TaskType depends_on;          // Task this depends on (NONE if independent)
        double dependency_ratio;      // % of parent output needed (0.0-1.0)
        std::vector<TaskType> spawns_children; // Tasks to create upon completion
    } dependencies;
};

/**
 * Task Profile Database
 * Maps TaskType to static profile definition
 */
class TaskProfileDatabase {
public:
    // Singleton instance
    static TaskProfileDatabase& getInstance() {
        static TaskProfileDatabase instance;
        return instance;
    }

    // Get profile for a task type
    const TaskProfile& getProfile(TaskType type) const;

    // Get string name for task type
    static std::string getTaskTypeName(TaskType type);

    // Get all task types
    static const std::vector<TaskType>& getAllTaskTypes();

private:
    TaskProfileDatabase();
    std::map<TaskType, TaskProfile> profiles;
};

// ============================================================================
// Constants for Quick Reference
// ============================================================================

// Periodic task periods (seconds)
namespace TaskPeriods {
    constexpr double LOCAL_OBJECT_DETECTION = 0.050;      // 50ms (20 Hz)
    constexpr double COOPERATIVE_PERCEPTION = 0.100;      // 100ms (10 Hz)
    constexpr double ROUTE_OPTIMIZATION = 0.500;          // 500ms (2 Hz)
    constexpr double SENSOR_HEALTH_CHECK = 10.0;          // 10s
    constexpr double FLEET_TRAFFIC_BATCH = 60.0;          // 60s batch
}

// Poisson task rates (tasks/second, lambda parameter)
namespace TaskRates {
    constexpr double VOICE_COMMAND_PROCESSING = 0.2;      // 1 task per 5 seconds
}

// Task priorities (mapped to QoS values)
namespace QoSValues {
    constexpr double LOCAL_OBJECT_DETECTION = 0.95;       // Safety critical
    constexpr double COOPERATIVE_PERCEPTION = 0.85;       // High
    constexpr double ROUTE_OPTIMIZATION = 0.65;           // Medium
    constexpr double FLEET_TRAFFIC_FORECAST = 0.45;       // Low
    constexpr double VOICE_COMMAND_PROCESSING = 0.50;     // Medium
    constexpr double SENSOR_HEALTH_CHECK = 0.30;          // Low
}

} // namespace complex_network

#endif /* TASK_PROFILE_H_ */
