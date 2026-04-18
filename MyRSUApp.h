#ifndef __MYRSUAPP_H_
#define __MYRSUAPP_H_

#include "veins/modules/application/ieee80211p/DemoBaseApplLayer.h"
#include "veins/modules/messages/DemoSafetyMessage_m.h"
// #include "rsu_http_poster.h"  // Disabled - using direct PostgreSQL
#include "TaskMetadataMessage_m.h"
#include "RedisDigitalTwin.h"
#include "FuturePositionPredictor.h"
#include "LinkRateModel.h"
#include <map>
#include <vector>
#include <deque>
#include <string>
#include <set>
#include <unordered_set>
#include <cstdint>
#include <memory>
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

    // Energy state
    double battery_level_pct = 0.0;
    double battery_current_mAh = 0.0;
    double battery_capacity_mAh = 0.0;
    double energy_task_j_total = 0.0;
    double energy_task_j_last = 0.0;

    // Vehicle metadata synced from periodic beacons
    std::string vehicle_type;
    double tx_power_mw = 0.0;
    double storage_capacity_gb = 128.0;
    uint32_t max_queue_size = 50;
    uint32_t max_concurrent_tasks = 4;
    
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
    
    uint64_t mem_footprint_bytes;    // working memory held on the processing entity
    uint64_t cpu_cycles;
    
    double created_time;
    double deadline_seconds;
    double received_time;
    double qos_value;
    
    // Task Profile fields (populated from TaskMetadataMessage when is_profile_task=true)
    bool is_profile_task = false;
    std::string task_type_name;          // e.g. "LOCAL_OBJECT_DETECTION"
    int task_type_id = 0;
    uint64_t input_size_bytes = 0;
    uint64_t output_size_bytes = 0;
    bool is_offloadable = true;
    bool is_safety_critical = false;
    int priority_level = 2;
    
    // Completion/failure info (if received)
    bool completed = false;
    bool failed = false;
    double completion_time = 0.0;
    double processing_time = 0.0;
    bool completed_on_time = false;
    std::string failure_reason;
    
    // Decision tracking (for database polling)
    bool decision_sent = false;  // Flag to avoid re-sending decisions from DB
    bool decision_poll_miss_logged = false;
    int decision_poll_miss_count = 0;
};

/**
 * Vehicle Detail Information in RSU Broadcasts
 * Contains full vehicle state for multi-RSU DDQN candidate selection
 */
struct VehicleDetail {
    std::string vehicle_id;
    double pos_x = 0.0;                  // Vehicle position X (meters)
    double pos_y = 0.0;                  // Vehicle position Y (meters)
    double speed = 0.0;                  // Vehicle speed (m/s)
    double heading = 0.0;                // Vehicle heading (degrees)
    double cpu_available = 0.0;          // Available CPU (MIPS)
    double cpu_utilization = 0.0;        // CPU utilization (0-100%)
    double mem_available = 0.0;          // Available memory (MB)
    double mem_utilization = 0.0;        // Memory utilization (0-100%)
    int queue_length = 0;                // Tasks in queue
};

/**
 * RSU Neighbor State (for RSU-to-RSU communication)
 */
struct RSUNeighborState {
    std::string rsu_id;
    LAddress::L2Type rsu_mac;
    double last_update_time;

    // Resource state
    int queue_length;
    int processing_count;
    int max_concurrent_tasks;
    double cpu_available_ghz;
    double cpu_total_ghz;
    double memory_available_gb;
    double memory_total_gb;

    // Coverage information with full vehicle details
    std::vector<VehicleDetail> vehicle_details_in_coverage;
    std::vector<std::string> vehicle_ids_in_coverage;
    int vehicle_count;

    // Position
    double pos_x;
    double pos_y;

    // Computed metrics
    double load_factor = 0.0;        // CPU utilization factor (0-1)
    bool is_overloaded = false;      // Overload flag
};

/**
 * Vehicle coverage ownership record used by RSU Digital Twin.
 * It identifies whether a vehicle currently belongs to self RSU range
 * or to one of the neighboring RSU ranges.
 */
struct VehicleCoverageRecord {
    std::string vehicle_id;
    std::string best_rsu_id;
    LAddress::L2Type best_rsu_mac = 0;
    bool in_self_range = false;
    double best_rssi = -1e9;
    double self_rssi = -1e9;
    double last_update_time = 0.0;
};

/**
 * RSU Active Task (for task queue and processing)
 */
struct RSUActiveTask {
    std::string task_id;
    std::string vehicle_id;
    LAddress::L2Type vehicle_mac;
    
    // Task parameters
    uint64_t cpu_cycles;
    uint64_t task_size_bytes;
    double deadline_seconds;
    
    // Timing
    double arrival_time;      // When task arrived at RSU (for queue wait tracking)
    double start_time;         // When task started processing
    double allocated_cpu_ghz;  // CPU allocated to this task
    double estimated_completion_time;  // Expected completion time
    
    // State
    enum State { QUEUED, PROCESSING, COMPLETED, FAILED } state;
    std::string failure_reason;
    
    // Self-message for completion event
    cMessage* completion_msg = nullptr;
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
    // RSUHttpPoster poster{"http://127.0.0.1:8000/ingest"};  // Disabled - using direct PostgreSQL
    // self-message used for periodic beacons; keep as member so we can cancel/delete safely
    omnetpp::cMessage* beaconMsg{nullptr};
    
    // Decision polling timer (for reading ML decisions from database)
    cMessage* checkDecisionMsg = nullptr;

    // Set true at the start of finish() to prevent handleTaskMetadata() from
    // rescheduling checkDecisionMsg while the module is being torn down.
    bool isFinishing = false;

    // Periodic terminal progress printer
    cMessage* progressMsg_ = nullptr;
    static constexpr double kProgressIntervalS = 30.0;
    std::map<std::string, int> agent_ok_;    // agent_name → success count
    std::map<std::string, int> agent_fail_;  // agent_name → fail count
    
    // RSU status broadcast timer (for RSU-to-RSU communication)
    cMessage* rsu_broadcast_timer = nullptr;
    
    // Vehicle MAC address mapping (for sending decisions)
    std::map<std::string, LAddress::L2Type> vehicle_macs;  // vehicle_id -> MAC address
    
    // Edge server resource parameters (static values)
    double edgeCPU_GHz = 0.0;           // Edge server CPU capacity (GHz)
    double edgeMemory_GB = 0.0;         // Edge server memory capacity (GB)
    int maxVehicles = 0;                // Maximum vehicles this RSU can serve
    double processingDelay_ms = 0.0;    // Base processing delay (ms)

    // Direct V2V radio parameters used by the RSSI heuristic.
    double directLinkTxPower_mW = 0.0;
    double directLinkCarrierFrequency_Hz = 0.0;
    double directLinkAntennaHeight_m = 0.0;
    double directLinkRssiThreshold_dBm = 0.0;
    
    // ============================================================================
    // RSU RESOURCE TRACKING (Dynamic state)
    // ============================================================================
    
    // Dynamic resource state (like vehicles)
    double rsu_cpu_total = 0.0;         // Total CPU capacity (same as edgeCPU_GHz)
    double rsu_cpu_available = 0.0;     // Currently available CPU
    double rsu_memory_total = 0.0;      // Total memory (same as edgeMemory_GB)
    double rsu_memory_available = 0.0;  // Currently available memory
    double rsu_background_cpu_util = 0.0; // Baseline infrastructure CPU load (0-1)
    double rsu_background_mem_util = 0.0; // Baseline infrastructure memory load (0-1)
    double rsu_last_background_update = 0.0;
    
    // Task processing state
    int rsu_queue_length = 0;           // Tasks waiting in queue
    int rsu_processing_count = 0;       // Currently processing tasks
    int rsu_max_concurrent = 10;        // Max concurrent tasks
    
    // Statistics
    int rsu_tasks_arrived = 0;      // Count of TaskOffloadPacket arrivals at this RSU
    int rsu_tasks_admitted = 0;     // Count admitted into RSU processing (direct + from waiting queue)
    int rsu_tasks_processed = 0;
    int rsu_tasks_failed = 0;
    int rsu_tasks_rejected = 0;
    double rsu_total_processing_time = 0.0;
    double rsu_total_queue_time = 0.0;
    
    // Status update timing
    double rsu_status_update_interval = 0.5;  // Update every 0.5 second
    cMessage* rsu_status_update_timer = nullptr;
    
    // ============================================================================
    // RSU-TO-RSU COMMUNICATION (I2I)
    // ============================================================================
    
    // Neighbor RSU states (learned via broadcast)
    std::map<std::string, RSUNeighborState> neighbor_rsus;  // rsu_id -> state
    
    // RSU broadcast parameters
    double rsu_broadcast_interval = 0.5;      // Broadcast every 500ms
    double neighbor_state_ttl = 1.5;          // Consider state stale after 1.5s
    int max_vehicles_in_broadcast = 20;       // Limit vehicle list size to avoid huge packets
    
    // Task redirect parameters
    int max_redirect_hops = 2;                // Maximum allowed redirect hops
    
    // Task queue and active tasks
    std::deque<RSUActiveTask> task_queue;           // FIFO queue for pending tasks
    std::map<std::string, RSUActiveTask> active_tasks;  // Currently processing tasks (task_id -> task)
    
    // ============================================================================
    // DIGITAL TWIN TRACKING SYSTEM
    // ============================================================================
    
    // Digital Twin Data Structures
    std::map<std::string, VehicleDigitalTwin> vehicle_twins;  // vehicle_id -> twin
    std::map<std::string, TaskRecord> task_records;           // task_id -> record
    std::map<std::string, VehicleCoverageRecord> vehicle_coverage_records; // vehicle_id -> coverage owner

    // Task IDs awaiting an offloading decision from the DRL agent (written to Redis).
    // The checkDecisionMsg handler iterates THIS set (O(N_pending)) instead of scanning
    // all of task_records (which grows unboundedly and would become O(N_total_tasks)).
    // Entries are removed as soon as a decision is dispatched to the vehicle.
    std::unordered_set<std::string> pending_decision_ids_;

    // Counter used by the periodic task_records cleanup inside checkDecisionMsg.
    int cleanup_tick_ = 0;

    // Pending SV agent subtasks: sub_id (orig::agent) → simtime of dispatch.
    // Written when an agent subtask is sent to a service vehicle; erased when the result
    // arrives.  The checkDecisionMsg timer writes FAILED to Redis for any subtask that
    // has been pending longer than sv_subtask_timeout_s (SV unreachable / message dropped).
    std::map<std::string, simtime_t> sv_subtask_pending_;
    static constexpr double sv_subtask_timeout_s = 5.0;
    
    // Digital Twin Management Methods
    void handleTaskMetadata(TaskMetadataMessage* msg);
    void handleTaskCompletion(TaskCompletionMessage* msg);
    void handleTaskFailure(TaskFailureMessage* msg);
    void handleTaskResultWithCompletion(TaskResultMessage* msg);
    void handleRSUTaskResultRelay(TaskResultMessage* msg);
    void handleServiceVehicleResultRelay(TaskResultMessage* msg, LAddress::L2Type targetRsuMac);
    void handleVehicleResourceStatus(VehicleResourceStatusMessage* msg);
    
    VehicleDigitalTwin& getOrCreateVehicleTwin(const std::string& vehicle_id);
    void refreshVehicleCoverageRecord(const std::string& vehicle_id);
    LAddress::L2Type getBestRsuByRssiForVehicle(const std::string& vehicle_id,
                                                 std::string* bestRsuId = nullptr,
                                                 bool* inSelfRange = nullptr);
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
        std::string initial_gate_classification;
        std::string initial_gate_reason;
        
        // Task characteristics
        uint64_t mem_footprint_bytes;  // working memory on processing entity
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
    std::string selectBestServiceVehicle(const OffloadingRequest& request) const;
    double estimateDirectLinkRssiDbm(double distanceMeters) const;
    
    // RSU task processing (edge server)
    // Tracks tasks currently being processed on the RSU edge server.
    struct PendingRSUTask {
        std::string vehicle_id;
        LAddress::L2Type vehicle_mac = 0;
        LAddress::L2Type ingress_rsu_mac = 0;
        uint64_t mem_footprint_bytes = 0;   // reserved RSU memory for this task
        uint64_t cpu_cycles = 0;            // total cycles required for this task
        double qos_value = 0.5;             // [0,1], used for weighted CPU sharing
        double cycles_remaining = 0.0;      // cycles not yet executed (updated at each reschedule)
        double exec_time_s = 0.0;           // current projected execution time (seconds)
        double scheduled_at = 0.0;          // simTime when task was first accepted
        double last_reschedule_time = 0.0;  // simTime of last CPU reallocation
        double cpu_allocated_hz = 0.0;      // per-task CPU share at last reschedule (Hz)
        cMessage* completion_event = nullptr; // pointer to scheduled completion self-msg
    };

    struct QueuedRSUTask {
        std::string task_id;
        std::string vehicle_id;
        LAddress::L2Type vehicle_mac = 0;
        LAddress::L2Type ingress_rsu_mac = 0;
        uint64_t mem_footprint_bytes = 0;
        uint64_t cpu_cycles = 0;
        double qos_value = 0.5;
        double enqueue_time = 0.0;
        double deadline_seconds = 0.0;  // Absolute deadline (from task metadata)
    };

    std::map<std::string, PendingRSUTask> rsuPendingTasks;  // task_id -> in-flight task
    std::deque<QueuedRSUTask> rsuWaitingQueue;              // bounded wait buffer
    int rsu_waiting_queue_capacity = 5;                     // small buffer to avoid hard drops

    // Agent sub-task tracking: sub_task_id ("{orig_task_id}::{agent_name}") → metadata
    // Sub-tasks are dispatched by the RSU for each DRL baseline agent so all agents
    // experience true execution rather than analytical estimates.
    struct AgentSubTaskInfo {
        std::string original_task_id;
        std::string agent_name;
        double arrival_time_s;
    };
    std::map<std::string, AgentSubTaskInfo> agent_sub_tasks;

    // Dispatch a sub-task for one DRL baseline agent so it truly executes on its
    // chosen target (this RSU, a neighbor RSU, or a service vehicle).
    // Sub-task IDs are encoded as "{original_task_id}::{agent_name}".
    // On completion the RSU writes per-agent results to task:{id}:results in Redis
    // instead of forwarding a result to the vehicle.
    void dispatchAgentSubTask(const TaskRecord& record,
                              const std::string& agent_name,
                              const std::string& target_type,
                              const std::string& target_id);

    void processTaskOnRSU(const std::string& task_id, veins::TaskOffloadPacket* packet, LAddress::L2Type ingress_rsu_mac);
    void tryStartQueuedRSUTasks();
    void updateRSUBackgroundLoad();
    double getRSUTaskCpuUtilization() const;
    double getEffectiveRSUCpuUtilization() const;
    double getEffectiveRSUMemoryUtilization() const;
    double getEffectiveRSUMemoryAvailable() const;
    bool canReserveRSUMemory(uint64_t bytes) const;
    void reserveRSUMemory(uint64_t bytes);
    void releaseRSUMemory(uint64_t bytes);
    // Recomputes each in-flight task's remaining cycles and reschedules all completion
    // events so each task gets a weighted share of edgeCPU_GHz.
    void reallocateRSUTasks();
    double getTaskPriorityWeight(double qosValue) const;
    void reallocateRSUTasks(double new_cpu_per_task_Hz);
    void sendTaskResultToVehicle(const std::string& task_id, const std::string& vehicle_id,
                                  LAddress::L2Type vehicle_mac, LAddress::L2Type ingress_rsu_mac,
                                  bool success, double processing_time);
    
    // Configuration parameters
    bool mlModelEnabled = false;
    int maxConcurrentOffloadedTasks = 10;
    bool serviceVehicleSelectionEnabled = true;
    double offload_link_bandwidth_hz = 10e6;
    double offload_rate_efficiency = 0.8;
    double offload_rate_cap_bps = -1.0;
    double offload_response_ratio = 0.2;
    
    // ============================================================================
    // POSTGRESQL DATABASE INTEGRATION
    // ============================================================================
    
    PGconn* db_conn = nullptr;
    std::string db_conninfo;
    int rsu_id = 0;
    
    // ============================================================================
    // REDIS DIGITAL TWIN INTEGRATION
    // ============================================================================
    
    RedisDigitalTwin* redis_twin = nullptr;
    bool use_redis = true;  // Config parameter
    std::string redis_host = "127.0.0.1";
    int redis_port = 6379;
    int redis_db = 0;

    // Secondary run export controls (motion + channel context only, no SINR math)
    bool secondary_link_context_export = false;
    std::string secondary_run_id = "dt2_default";
    double secondary_link_sample_interval = 0.1;
    int secondary_redis_max_series_len = 6000;
    std::map<std::string, double> secondary_last_export_time; // vehicle_id -> last export sim_time
    std::string secondary_predictor_mode = "placeholder";
    bool secondary_use_external_predictions = true;
    double secondary_prediction_horizon_s = 0.5;
    double secondary_prediction_step_s = 0.02;
    double secondary_cycle_interval_s = 0.1;
    double secondary_candidate_radius_m = 1500.0;
    bool secondary_q_publish_enabled = true;
    double secondary_sinr_threshold_db = 3.0;
    double secondary_interference_mw = 0.09;
    int secondary_q_ttl_s = 2;
    int secondary_q_stream_maxlen = 200000;
    bool secondary_nakagami_enabled = true;
    double secondary_nakagami_m = 1.0;
    double secondary_nakagami_cell_size_m = 5.0;
    std::unique_ptr<IFuturePositionPredictor> secondary_predictor;
    cMessage* secondary_cycle_timer = nullptr;
    uint64_t secondary_cycle_index = 0;

    struct SecondaryCandidateSet {
        std::vector<std::string> rsu_ids;
        std::vector<std::string> vehicle_ids;
    };
    std::map<std::string, std::vector<PredictedVehicleState>> secondary_cycle_predictions;
    std::map<std::string, SecondaryCandidateSet> secondary_cycle_candidates;

    struct RsuStaticContext {
        std::string rsu_id;
        double pos_x = 0.0;
        double pos_y = 0.0;
    };
    std::vector<RsuStaticContext> rsu_static_contexts;

    // Obstacle polygon for secondary SINR shadow loss
    struct ObstaclePolygon {
        double db_per_cut;
        double db_per_meter;
        double aabb_min_x, aabb_min_y, aabb_max_x, aabb_max_y;
        std::vector<std::pair<double, double>> vertices;
    };
    std::vector<ObstaclePolygon> secondary_obstacle_polygons;
    
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
    void insertLifecycleEvent(const std::string& task_id, const std::string& event_type,
                              const std::string& source, const std::string& target,
                              const std::string& details = "{}");
    void updateTaskStaticTraceFromEvent(const std::string& task_id,
                                        const std::string& event_type,
                                        double event_time,
                                        const std::string& details);
    void insertOffloadedTaskCompletion(const std::string& task_id, const std::string& vehicle_id,
                                       const std::string& decision_type, const std::string& processor_id,
                                       double request_time, double decision_time, double start_time, 
                                       double completion_time, bool success, bool completed_on_time,
                                       double deadline_seconds, uint64_t mem_footprint_bytes, uint64_t cpu_cycles,
                                       double qos_value, const std::string& result_data, const std::string& failure_reason);
    
    // RSU status and metadata tracking
    void insertRSUStatus();
    void insertRSUMetadata();
    void insertVehicleMetadata(const std::string& vehicle_id);
    void sendRSUStatusUpdate();

    // Secondary DT storage exports (DB + Redis)
    void initializeRsuStaticContexts();
    void initializeSecondaryPredictor();
    void initializeSecondaryCycleWorker();
    void loadObstaclePolygons();
    double computeObstacleLossDb(double tx_x, double tx_y, double rx_x, double rx_y) const;
    double estimateSinrDbWithObstacles(double tx_x, double tx_y, double rx_x, double rx_y, double dist,
                                       double vehicle_shadow_loss_db = 0.0,
                                       double lognormal_shadow_db = 0.0,
                                       double nakagami_fading_db = 0.0,
                                       double interference_mw = -1.0) const;
    void runSecondaryCycle();
    void exportSecondaryContextSamples(const VehicleResourceStatusMessage* msg,
                                       const VehicleDigitalTwin& sourceTwin);
    void insertSecondaryProgress(double sim_time);
    void insertSecondaryVehicleSample(const VehicleResourceStatusMessage* msg);
    void insertSecondaryLinkSample(const std::string& link_type,
                                   const std::string& tx_entity_id,
                                   const std::string& rx_entity_id,
                                   double sim_time,
                                   double tx_pos_x,
                                   double tx_pos_y,
                                   double rx_pos_x,
                                   double rx_pos_y,
                                   double distance_m,
                                   double relative_speed,
                                   double tx_heading,
                                   double rx_heading);
    void insertSecondaryQCycle(uint64_t cycle_id,
                               double sim_time,
                               int trajectory_count,
                               int candidate_count,
                               int entry_count);
    void insertSecondaryQEntry(uint64_t cycle_id,
                               const std::string& link_type,
                               const std::string& tx_entity_id,
                               const std::string& rx_entity_id,
                               int step_index,
                               double predicted_time,
                               double tx_pos_x,
                               double tx_pos_y,
                               double rx_pos_x,
                               double rx_pos_y,
                               double distance_m,
                               double sinr_db);
    
    // ============================================================================
    // RSU-TO-RSU COMMUNICATION METHODS
    // ============================================================================
    void broadcastRSUStatus();
    void handleRSUStatusBroadcast(veins::RSUStatusBroadcastMessage* msg);
    void updateNeighborState(const RSUNeighborState& state);
    bool isNeighborStateFresh(const RSUNeighborState& state);
    void cleanupStaleNeighborStates();
    
    void queueTaskForProcessing(const RSUActiveTask& active_task);
    void startNextQueuedTask();
    double calculateProcessingTime(const RSUActiveTask& task);
    void deallocateTaskResources(const RSUActiveTask& task);
    void checkDeadlineAndNotify(const RSUActiveTask& task);

    // Track agent result for progress printer
    void trackAgentResult(const std::string& agent_name, const std::string& status) {
        if (status == "COMPLETED_ON_TIME") agent_ok_[agent_name]++;
        else agent_fail_[agent_name]++;
    }
};

} // namespace complex_network

#endif