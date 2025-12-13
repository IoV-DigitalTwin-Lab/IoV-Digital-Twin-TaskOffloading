//
// TaskMetadataMessage.msg
// Message definitions for task metadata communication between vehicles and RSUs
//

import veins.modules.messages.BaseFrame1609_4;

namespace veins;

//
// Task Metadata Message - Sent from vehicle to RSU when task is generated
//
packet TaskMetadataMessage extends BaseFrame1609_4 {
    // Task Identification
    string task_id;              // Unique task identifier
    string vehicle_id;           // Originating vehicle ID
    
    // Task Characteristics
    uint64_t task_size_bytes;    // Memory footprint (D_task)
    uint64_t cpu_cycles;         // Required CPU cycles (C_task)
    
    // Timing Information
    double created_time;         // SimTime when task was generated
    double deadline_seconds;     // Relative deadline (e.g., 0.3 seconds)
    double received_time = 0;    // Assigned by RSU upon reception
    
    // QoS and Priority
    double qos_value;            // Quality of Service (0.0 - 1.0)
}

//
// Task Completion Message - Sent from vehicle to RSU when task completes
//
packet TaskCompletionMessage extends BaseFrame1609_4 {
    string task_id;              // Task identifier
    string vehicle_id;           // Originating vehicle ID
    double completion_time;      // When task completed
    double processing_time;      // Actual processing duration
    bool completed_on_time;      // True if completed before deadline
    double cpu_allocated;        // Average CPU allocated during processing
}

//
// Task Failure Message - Sent from vehicle to RSU when task fails/is rejected
//
packet TaskFailureMessage extends BaseFrame1609_4 {
    string task_id;              // Task identifier
    string vehicle_id;           // Originating vehicle ID
    double failure_time;         // When task failed/was rejected
    string failure_reason;       // "DEADLINE_MISSED", "REJECTED_QUEUE_FULL", "REJECTED_INFEASIBLE"
    double wasted_time;          // Time spent before failure
}

//
// Vehicle Resource Status Message - Periodic heartbeat from vehicle to RSU
//
packet VehicleResourceStatusMessage extends BaseFrame1609_4 {
    string vehicle_id;           // Vehicle identifier
    
    // Vehicle State
    double pos_x;                // Position X
    double pos_y;                // Position Y
    double speed;                // Current speed
    
    // Resource Information
    double cpu_total;            // Total CPU capacity (Hz)
    double cpu_allocable;        // CPU available for tasks (after reservation)
    double cpu_available;        // Currently available CPU (Hz)
    double cpu_utilization;      // CPU utilization percentage (0-100)
    
    double mem_total;            // Total memory (bytes)
    double mem_available;        // Available memory (bytes)
    double mem_utilization;      // Memory utilization percentage (0-100)
    
    // Task Queue Status
    uint32_t tasks_generated;    // Total tasks generated
    uint32_t tasks_completed_on_time;  // Tasks completed before deadline
    uint32_t tasks_completed_late;     // Tasks completed after deadline (soft miss)
    uint32_t tasks_failed;       // Tasks that missed deadline (hard miss)
    uint32_t tasks_rejected;     // Tasks rejected (queue full/infeasible)
    uint32_t current_queue_length;     // Current pending tasks
    uint32_t current_processing_count; // Currently processing tasks
    
    // Performance Metrics
    double avg_completion_time;  // Average task completion time
    double deadline_miss_ratio;  // Ratio of missed deadlines
}

//
// Offloading Request Message - Task vehicle → RSU (requesting offloading decision)
//
packet OffloadingRequestMessage extends BaseFrame1609_4 {
    LAddress::L2Type senderAddress = -1; // Sender's MAC address
    string task_id;                      // Task identifier
    string vehicle_id;                   // Originating vehicle ID
    double request_time;                 // When request was sent
    
    // Task Characteristics
    uint64_t task_size_bytes;            // Task size
    uint64_t cpu_cycles;                 // Required CPU cycles
    double deadline_seconds;             // Task deadline
    double qos_value;                    // QoS priority
    
    // Vehicle State at Request Time
    double local_cpu_available_ghz;      // Available CPU (GHz)
    double local_cpu_utilization;        // CPU utilization (0-1)
    double local_mem_available_mb;       // Available memory (MB)
    uint32_t local_queue_length;         // Current queue length
    uint32_t local_processing_count;     // Current processing count
    
    // Vehicle Location
    double pos_x;
    double pos_y;
    double speed;
    
    // Local Decision Recommendation
    string local_decision;               // "LOCAL", "OFFLOAD", or "REJECT"
}

//
// Offloading Decision Message - RSU → Task vehicle (ML model decision)
//
packet OffloadingDecisionMessage extends BaseFrame1609_4 {
    LAddress::L2Type senderAddress = -1; // RSU's MAC address
    string task_id;                      // Task identifier
    string vehicle_id;                   // Target vehicle ID
    double decision_time;                // When decision was made
    
    // Decision
    string decision_type;                // "LOCAL", "RSU", "SERVICE_VEHICLE", "REJECT"
    
    // If SERVICE_VEHICLE: target information
    string target_service_vehicle_id;    // Service vehicle ID (if applicable)
    uint64_t target_service_vehicle_mac; // Service vehicle MAC address (if applicable)
    
    // Decision Metadata
    double confidence_score;             // ML model confidence (0-1)
    double estimated_completion_time;    // Estimated time to complete
    string decision_reason;              // Human-readable reasoning
}

//
// Task Offload Packet - Task vehicle → RSU/Service vehicle (complete task data)
//
packet TaskOffloadPacket extends BaseFrame1609_4 {
    string task_id;                      // Task identifier
    string origin_vehicle_id;            // Originating vehicle ID
    uint64_t origin_vehicle_mac;         // Return address MAC
    double offload_time;                 // When task was offloaded
    
    // Task Characteristics
    uint64_t task_size_bytes;            // Task size
    uint64_t cpu_cycles;                 // Required CPU cycles
    double deadline_seconds;             // Task deadline
    double qos_value;                    // QoS priority
    
    // Task Input Data (placeholder - would contain actual task data)
    string task_input_data;              // Serialized input data
}

//
// Task Result Message - RSU/Service vehicle → Task vehicle (processing results)
//
packet TaskResultMessage extends BaseFrame1609_4 {
    string task_id;                      // Task identifier
    string origin_vehicle_id;            // Original task vehicle ID
    string processor_id;                 // Who processed it ("RSU-X" or vehicle ID)
    double completion_time;              // When processing completed
    double processing_time;              // Actual processing duration
    bool success;                        // True if completed successfully
    
    // Results
    string task_output_data;             // Serialized output data
    string failure_reason;               // If failed, reason why
}

//
// Task Offloading Event - Any entity → RSU (lifecycle tracking for Digital Twin)
//
packet TaskOffloadingEvent extends BaseFrame1609_4 {
    string task_id;                      // Task identifier
    string event_type;                   // "REQUEST_SENT", "DECISION_RECEIVED", "OFFLOAD_SENT", "PROCESSING_STARTED", "RESULT_RECEIVED", "TIMEOUT", etc.
    string source_entity_id;             // Who generated the event
    string target_entity_id;             // Target entity (if applicable)
    double event_time;                   // When event occurred
    string event_details;                // JSON with additional details
}

