#ifndef TASK_H_
#define TASK_H_

#include <string>
#include <omnetpp.h>

using namespace omnetpp;

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
    
    // Task Characteristics
    uint64_t task_size_bytes;         // Memory footprint (D_task)
    uint64_t cpu_cycles;              // Required CPU cycles (C_task)
    
    // Timing Information
    simtime_t created_time;           // When task was generated
    simtime_t deadline;               // Absolute deadline (created_time + relative_deadline)
    double relative_deadline;         // Relative deadline in seconds
    simtime_t received_time;          // When RSU received metadata (0 if not yet received)
    simtime_t processing_start_time;  // When task started processing
    simtime_t completion_time;        // When task completed/failed
    
    // QoS and Priority
    double qos_value;                 // Quality of Service (0.0 - 1.0)
    
    // Processing State
    TaskState state;                  // Current task state
    uint64_t cpu_cycles_executed;     // CPU cycles already executed
    double cpu_allocated;             // Current CPU allocation (Hz)
    
    // Event Handles
    cMessage* completion_event;       // Scheduled completion event
    cMessage* deadline_event;         // Scheduled deadline check event
    
    // Constructor
    Task(const std::string& vid, uint64_t seq_num, uint64_t size, uint64_t cycles, 
         double deadline_sec, double qos);
    
    // Destructor
    ~Task();
    
    // Helper methods
    std::string getStateString() const;
    double getRemainingCycles() const;
    double getRemainingDeadline(simtime_t current_time) const;
    bool isDeadlineMissed(simtime_t current_time) const;
    void logTaskInfo(const std::string& prefix) const;
};

/**
 * Task Comparator for priority queue
 * Priority: Higher QoS first, then tighter deadline
 */
struct TaskComparator {
    bool operator()(Task* a, Task* b) const {
        // Higher QoS = higher priority (reverse for priority_queue max-heap)
        if (std::abs(a->qos_value - b->qos_value) > 0.1)
            return a->qos_value < b->qos_value;
        
        // Tighter deadline = higher priority
        double deadline_a = (a->deadline - a->created_time).dbl();
        double deadline_b = (b->deadline - b->created_time).dbl();
        return deadline_a > deadline_b;
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

#endif /* TASK_H_ */
