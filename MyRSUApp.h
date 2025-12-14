#ifndef __MYRSUAPP_H_
#define __MYRSUAPP_H_

#include "veins/modules/application/ieee80211p/DemoBaseApplLayer.h"
#include "veins/modules/messages/DemoSafetyMessage_m.h"
#include "rsu_http_poster.h"
#include "TaskMetadataMessage_m.h"
#include <map>
#include <vector>
#include <string>
#include <libpq-fe.h>

using namespace veins;

namespace complex_network {

/**
 * Vehicle Digital Twin structure maintained by RSU
 */
struct VehicleDigitalTwin {
    std::string vehicle_id;
    
    // Latest vehicle state
    double pos_x = 0.0, pos_y = 0.0;
    double speed = 0.0;
    double heading = 0.0;  // Heading in degrees
    
    // Resource Information
    double cpu_total = 0.0;
    double cpu_allocable = 0.0;
    double cpu_available = 0.0;
    double cpu_utilization = 0.0;
    
    double mem_total = 0.0;
    double mem_available = 0.0;
    double mem_utilization = 0.0;
    
    double tx_power = 0.0;
    double reservation_ratio = 0.0;
    
    // Task Statistics
    uint32_t tasks_generated = 0;
    uint32_t tasks_completed_on_time = 0;
    uint32_t tasks_completed_late = 0;
    uint32_t tasks_failed = 0;
    uint32_t tasks_rejected = 0;
    uint32_t current_queue_length = 0;
    uint32_t current_processing_count = 0;
    
    // Performance Metrics
    double avg_completion_time = 0.0;
    double deadline_miss_ratio = 0.0;
    
    // Timing
    double last_update_time = 0.0;
    double first_seen_time = 0.0;
};

/**
 * Task metadata stored by RSU for Digital Twin tracking
 */
struct TaskRecord {
    std::string task_id;
    std::string vehicle_id;
    
    uint64_t task_size_bytes;
    uint64_t cpu_cycles;
    
    double created_time;
    double deadline_seconds;
    double received_time;
    double qos_value;
    
    // Completion/failure info (if received)
    bool completed = false;
    bool failed = false;
    double completion_time = 0.0;
    double processing_time = 0.0;
    bool completed_on_time = false;
    std::string failure_reason;
    
    // Decision tracking (for database polling)
    bool decision_sent = false;  // Flag to avoid re-sending decisions from DB
};

class MyRSUApp : public DemoBaseApplLayer {
public:
    void initialize(int stage) override;
    void finish() override;

protected:
    void onWSM(BaseFrame1609_4* wsm) override;
    void handleSelfMsg(cMessage* msg) override;
    void handleLowerMsg(cMessage* msg) override;
    void handleMessage(cMessage* msg) override;

private:
    RSUHttpPoster poster{"http://127.0.0.1:8000/ingest"};
    // self-message used for periodic beacons; keep as member so we can cancel/delete safely
    omnetpp::cMessage* beaconMsg{nullptr};
    
    // Decision polling timer (for reading ML decisions from database)
    cMessage* checkDecisionMsg = nullptr;
    
    // Vehicle MAC address mapping (for sending decisions)
    std::map<std::string, LAddress::L2Type> vehicle_macs;  // vehicle_id -> MAC address
    
    // Edge server resource parameters (static values)
    double edgeCPU_GHz = 0.0;           // Edge server CPU capacity (GHz)
    double edgeMemory_GB = 0.0;         // Edge server memory capacity (GB)
    int maxVehicles = 0;                // Maximum vehicles this RSU can serve
    double processingDelay_ms = 0.0;    // Base processing delay (ms)
    
    // ============================================================================
    // RSU RESOURCE TRACKING (Dynamic state)
    // ============================================================================
    
    // Dynamic resource state (like vehicles)
    double rsu_cpu_total = 0.0;         // Total CPU capacity (same as edgeCPU_GHz)
    double rsu_cpu_available = 0.0;     // Currently available CPU
    double rsu_memory_total = 0.0;      // Total memory (same as edgeMemory_GB)
    double rsu_memory_available = 0.0;  // Currently available memory
    
    // Task processing state
    int rsu_queue_length = 0;           // Tasks waiting in queue
    int rsu_processing_count = 0;       // Currently processing tasks
    int rsu_max_concurrent = 10;        // Max concurrent tasks
    
    // Statistics
    int rsu_tasks_received = 0;
    int rsu_tasks_processed = 0;
    int rsu_tasks_failed = 0;
    int rsu_tasks_rejected = 0;
    double rsu_total_processing_time = 0.0;
    double rsu_total_queue_time = 0.0;
    
    // Status update timing
    double rsu_status_update_interval = 1.0;  // Update every 1 second
    cMessage* rsu_status_update_timer = nullptr;
    
    // ============================================================================
    // DIGITAL TWIN TRACKING SYSTEM
    // ============================================================================
    
    // Digital Twin Data Structures
    std::map<std::string, VehicleDigitalTwin> vehicle_twins;  // vehicle_id -> twin
    std::map<std::string, TaskRecord> task_records;           // task_id -> record
    
    // Digital Twin Management Methods
    void handleTaskMetadata(TaskMetadataMessage* msg);
    void handleTaskCompletion(TaskCompletionMessage* msg);
    void handleTaskFailure(TaskFailureMessage* msg);
    void handleTaskResultWithCompletion(TaskResultMessage* msg);
    void handleVehicleResourceStatus(VehicleResourceStatusMessage* msg);
    
    VehicleDigitalTwin& getOrCreateVehicleTwin(const std::string& vehicle_id);
    void updateDigitalTwinStatistics();
    void logDigitalTwinState();
    void logTaskRecord(const TaskRecord& record, const std::string& event);
    
    // ============================================================================
    // TASK OFFLOADING ORCHESTRATION
    // ============================================================================
    
    // Offloading request tracking
    struct OffloadingRequest {
        std::string task_id;
        std::string vehicle_id;
        LAddress::L2Type vehicle_mac;
        double request_time;
        std::string local_decision;
        
        // Task characteristics
        uint64_t task_size_bytes;
        uint64_t cpu_cycles;
        double deadline_seconds;
        double qos_value;
        
        // Vehicle state at request
        double vehicle_cpu_available;
        double vehicle_cpu_utilization;
        double vehicle_mem_available;
        uint32_t vehicle_queue_length;  // Changed from double to uint32_t to match database INTEGER type
        uint32_t vehicle_processing_count;
        
        // Vehicle location
        double pos_x;
        double pos_y;
        double speed;
        double heading;
        double acceleration;
    };
    
    std::map<std::string, OffloadingRequest> pending_offloading_requests;  // task_id -> request
    
    // Offloading message handlers
    void handleOffloadingRequest(veins::OffloadingRequestMessage* msg);
    void handleTaskOffloadPacket(veins::TaskOffloadPacket* msg);
    void handleTaskOffloadingEvent(veins::TaskOffloadingEvent* msg);
    
    // ML-based decision engine
    veins::OffloadingDecisionMessage* makeOffloadingDecision(const OffloadingRequest& request);
    std::string selectBestServiceVehicle(const OffloadingRequest& request);
    
    // RSU task processing (edge server)
    void processTaskOnRSU(const std::string& task_id, veins::TaskOffloadPacket* packet);
    void sendTaskResultToVehicle(const std::string& task_id, const std::string& vehicle_id, 
                                  LAddress::L2Type vehicle_mac, bool success);
    
    // Configuration parameters
    bool mlModelEnabled = false;
    int maxConcurrentOffloadedTasks = 10;
    bool serviceVehicleSelectionEnabled = true;
    
    // ============================================================================
    // POSTGRESQL DATABASE INTEGRATION
    // ============================================================================
    
    PGconn* db_conn = nullptr;
    std::string db_conninfo;
    int rsu_id = 0;
    
    void initDatabase();
    void closeDatabase();
    PGconn* getDBConnection();
    
    void insertTaskMetadata(const TaskMetadataMessage* msg);
    void insertTaskCompletion(const TaskCompletionMessage* msg);
    void insertTaskFailure(const TaskFailureMessage* msg);
    void insertVehicleStatus(const VehicleResourceStatusMessage* msg);
    
    // Offloading-specific database insertions
    void insertOffloadingRequest(const OffloadingRequest& request);
    void insertOffloadingDecision(const std::string& task_id, const veins::OffloadingDecisionMessage* decision);
    void insertTaskOffloadingEvent(const veins::TaskOffloadingEvent* event);
    void insertOffloadedTaskCompletion(const std::string& task_id, const std::string& vehicle_id,
                                       const std::string& decision_type, const std::string& processor_id,
                                       double request_time, double decision_time, double start_time, 
                                       double completion_time, bool success, bool completed_on_time,
                                       double deadline_seconds, uint64_t task_size_bytes, uint64_t cpu_cycles,
                                       double qos_value, const std::string& result_data, const std::string& failure_reason);
    
    // RSU status and metadata tracking
    void insertRSUStatus();
    void insertRSUMetadata();
    void insertVehicleMetadata(const std::string& vehicle_id);
    void sendRSUStatusUpdate();
};

} // namespace complex_network

#endif