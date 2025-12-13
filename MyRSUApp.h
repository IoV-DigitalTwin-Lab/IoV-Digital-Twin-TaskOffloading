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
    
    // Edge server resource parameters (static values)
    double edgeCPU_GHz = 0.0;           // Edge server CPU capacity (GHz)
    double edgeMemory_GB = 0.0;         // Edge server memory capacity (GB)
    int maxVehicles = 0;                // Maximum vehicles this RSU can serve
    double processingDelay_ms = 0.0;    // Base processing delay (ms)
    
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
    void handleVehicleResourceStatus(VehicleResourceStatusMessage* msg);
    
    VehicleDigitalTwin& getOrCreateVehicleTwin(const std::string& vehicle_id);
    void updateDigitalTwinStatistics();
    void logDigitalTwinState();
    void logTaskRecord(const TaskRecord& record, const std::string& event);
    
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
};

} // namespace complex_network

#endif