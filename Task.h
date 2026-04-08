#ifndef TASK_H_
#define TASK_H_

#include <string>
#include <omnetpp.h>
#include "TaskProfile.h"
#include "TaskProfile.h"

using namespace omnetpp;

namespace complex_network {

/**
 * Task State Machine
 * CREATED → METADATA_SENT → QUEUED → PROCESSING → COMPLETED_ON_TIME/COMPLETED_LATE/FAILED
 *                          → REJECTED
 */
enum TaskState {
    CREATED,              // Task just generated
    METADATA_SENT,        // Metadata transmitted to RSU
    REJECTED,             // Insufficient resources / Queue full
    QUEUED,               // Waiting for resources in priority queue
    PROCESSING,           // Executing locally on vehicle
    COMPLETED_ON_TIME,    // Finished before deadline
    COMPLETED_LATE,       // Finished after deadline (soft miss)
    FAILED                // Deadline expired, task aborted
};

/**
 * Task class representing a computational task with resource requirements
 */
class Task {
public:
    // Task Identification
    std::string task_id;              // Format: "V{vehicle_id}_T{sequence_num}_{timestamp}"
    std::string vehicle_id;           // Originating vehicle identifier
    TaskType type = TaskType::LOCAL_OBJECT_DETECTION; // Task type (from TaskProfile)
    bool is_profile_task = false;     // True if created from TaskProfile
    
    // Task Characteristics
    // mem_footprint_bytes: working-set memory reserved on the processing entity during execution.
    // Equals input_size_bytes (the data that must be resident in memory while the task runs).
    // Separate from output_size_bytes which is post-processing result size.
    uint64_t mem_footprint_bytes;     // Working memory footprint on processing entity (= input_size_bytes)
    uint64_t cpu_cycles;              // Required CPU cycles (C_task)
    uint64_t input_size_bytes;        // Input size (bytes)
    uint64_t output_size_bytes;       // Output size (bytes)
    
    // Timing Information
    simtime_t created_time;           // When task was generated
    simtime_t queue_entry_time;       // When task entered pending_tasks queue (for stable aging)
    double predicted_service_time_sec = 0.0; // Estimated local execution time when queued
    simtime_t deadline;               // Absolute deadline (created_time + relative_deadline)
    double relative_deadline;         // Relative deadline in seconds
    simtime_t received_time;          // When RSU received metadata (0 if not yet received)
    simtime_t processing_start_time;  // When task started processing
    simtime_t completion_time;        // When task completed/failed
    
    // QoS and Priority
    double qos_value;                 // Quality of Service (0.0 - 1.0)
    PriorityLevel priority = PriorityLevel::MEDIUM; // Priority mapping
    bool is_offloadable = true;       // Can this task be offloaded?
    bool is_safety_critical = false;  // Safety-critical tasks should stay local
    bool must_local_tag = false;      // Stage 1 rule tag: force local execution
    bool must_offload_tag = false;    // Stage 1 rule tag: force offload
    bool gpu_required_tag = false;    // Stage 2 rule tag: requires edge acceleration
    bool cooperation_required_tag = false; // Stage 2 rule tag: requires cooperative processing
    
    // Processing State
    TaskState state;                  // Current task state
    uint64_t cpu_cycles_executed;     // CPU cycles already executed
    double cpu_allocated;             // Current CPU allocation (Hz)
    
    // Event Handles
    cMessage* completion_event;       // Scheduled completion event
    cMessage* deadline_event;         // Scheduled deadline check event
    
    // Constructor (generic random-parameter task)
    Task(const std::string& vid, uint64_t seq_num, uint64_t size, uint64_t cycles,
         double deadline_sec, double qos);
        Task(TaskType task_type, const std::string& vid, uint64_t seq_num,
            uint64_t input_size, uint64_t output_size, uint64_t cycles,
            double deadline_sec, double qos, PriorityLevel task_priority,
            bool offloadable, bool safety_critical,
            bool must_local = false, bool must_offload = false,
            bool gpu_required = false, bool cooperation_required = false);
    
    // Destructor
    ~Task();
    
    // Helper methods
    std::string getStateString() const;
    double getRemainingCycles() const;
    double getRemainingDeadline(simtime_t current_time) const;
    bool isDeadlineMissed(simtime_t current_time) const;
    void logTaskInfo(const std::string& prefix) const;

    // Factory for TaskProfile-based tasks
    static Task* createFromProfile(TaskType task_type, const std::string& vid, uint64_t seq_num);
};

/**
 * Task Comparator for priority queue
 * Priority: weighted hybrid score (QoS + deadline urgency)
 *   score = alpha * qos_norm + beta * deadline_urgency
 * Effective score with starvation prevention:
 *   effective = score + aging_factor_per_sec * waiting_time_sec
 * For safety-critical workloads, deadlines are weighted higher.
 */
struct TaskComparator {
    static constexpr double ALPHA_QOS = 0.4;
    static constexpr double BETA_DEADLINE = 0.6;
    static constexpr double QOS_MAX = 1.0;
    static constexpr double DEADLINE_MIN_REF_SEC = 0.08; // 80ms profile minimum
    static constexpr double AGING_FACTOR_PER_SEC = 0.02;
    static constexpr double MAX_AGING_BOOST = 1.0;

    bool operator()(Task* a, Task* b) const {
        double qos_a = std::max(0.0, std::min(a->qos_value / QOS_MAX, 1.0));
        double qos_b = std::max(0.0, std::min(b->qos_value / QOS_MAX, 1.0));

        double deadline_a = std::max((a->deadline - a->created_time).dbl(), 1e-6);
        double deadline_b = std::max((b->deadline - b->created_time).dbl(), 1e-6);

        // Normalized deadline urgency in [0, 1]: tighter deadline => higher urgency.
        double urgency_a = std::min(1.0, DEADLINE_MIN_REF_SEC / deadline_a);
        double urgency_b = std::min(1.0, DEADLINE_MIN_REF_SEC / deadline_b);

        double base_score_a = ALPHA_QOS * qos_a + BETA_DEADLINE * urgency_a;
        double base_score_b = ALPHA_QOS * qos_b + BETA_DEADLINE * urgency_b;

        // Aging term (OS-style starvation prevention): tasks get a bounded boost
        // based on time spent in the pending_tasks queue.
        // Uses queue_entry_time (not created_time) to measure queue wait time only,
        // ensuring stable comparisons for tasks already in the queue.
        double wait_a = std::max(0.0, (simTime() - a->queue_entry_time).dbl());
        double wait_b = std::max(0.0, (simTime() - b->queue_entry_time).dbl());
        double aging_a = std::min(MAX_AGING_BOOST, AGING_FACTOR_PER_SEC * wait_a);
        double aging_b = std::min(MAX_AGING_BOOST, AGING_FACTOR_PER_SEC * wait_b);

        double score_a = base_score_a + aging_a;
        double score_b = base_score_b + aging_b;

        // Reverse comparison for priority_queue max-heap behavior.
        if (std::abs(score_a - score_b) > 1e-12) {
            return score_a < score_b;
        }

        // Deterministic tie-breakers.
        if (std::abs(deadline_a - deadline_b) > 1e-12) {
            return deadline_a > deadline_b; // tighter deadline first
        }
        if (std::abs(qos_a - qos_b) > 1e-12) {
            return qos_a < qos_b; // higher QoS first
        }
        return a->task_id > b->task_id;
    }
};

/**
 * Task Drop Comparator for queue management
 * Drop task with lowest utility score (QoS / remaining_deadline)
 */
struct TaskDropComparator {
    simtime_t current_time;
    
    TaskDropComparator(simtime_t ct) : current_time(ct) {}
    
    bool operator()(Task* a, Task* b) const {
        double remaining_a = (a->deadline - current_time).dbl();
        double remaining_b = (b->deadline - current_time).dbl();
        
        // Guard against negative/zero deadlines
        double score_a = a->qos_value / std::max(remaining_a, 0.001);
        double score_b = b->qos_value / std::max(remaining_b, 0.001);
        
        // Higher score = higher priority (keep), lower score = drop first
        return score_a > score_b;
    }
};

} // namespace complex_network

#endif /* TASK_H_ */
