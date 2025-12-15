#ifndef PAYLOADVEHICLEAPP_H
#define PAYLOADVEHICLEAPP_H

#include "veins/modules/application/ieee80211p/DemoBaseApplLayer.h"
#include "veins/modules/messages/DemoSafetyMessage_m.h"
#include "veins/modules/mobility/traci/TraCIMobility.h"
#include "veins/base/utils/FindModule.h"
#include "veins/base/utils/Coord.h"
#include "Task.h"
#include "TaskMetadataMessage_m.h"
#include "TaskOffloadingDecision.h"
#include <queue>
#include <set>
#include <vector>
#include <deque>
#include <map>

namespace complex_network {

class PayloadVehicleApp : public veins::DemoBaseApplLayer {
protected:
    virtual void initialize(int stage) override;
    virtual void onWSM(veins::BaseFrame1609_4* wsm) override;
    virtual void handleMessage(cMessage* msg) override;
    virtual void handleSelfMsg(cMessage* msg) override;
    virtual void receiveSignal(cComponent* src, simsignal_t id, cObject* obj, cObject* details) override;

private:
    veins::LAddress::L2Type myMacAddress;  // Store our own MAC address
    bool messageSent = false;              // To send only once
    
    // Vehicle data members (similar to VehicleApp)
    double flocHz_max = 0.0;         // Maximum CPU capacity (Hz)
    double flocHz_available = 0.0;   // Current available CPU capacity (Hz)
    double txPower_mW = 0.0;         // mW
    double speed = 0.0;              // Vehicle speed
    double prev_speed = 0.0;         // Previous speed for acceleration calculation
    double acceleration = 0.0;       // Current acceleration (m/s^2)
    simtime_t last_speed_update;     // Last time speed was updated
    veins::Coord pos;                // Vehicle position
    veins::TraCIMobility* mobility = nullptr;  // Mobility module
    
    // CPU variation parameters
    double cpuLoadFactor = 0.0;      // Current CPU load (0.0-1.0)
    double lastCpuUpdateTime = 0.0;  // Last time CPU load was updated
    
    // Battery parameters
    double battery_mAh_max = 0.0;        // Maximum battery capacity (mAh)
    double battery_mAh_current = 0.0;    // Current battery level (mAh)
    double battery_voltage = 12.0;       // Battery voltage (V)
    double lastBatteryUpdateTime = 0.0;  // Last battery update time
    
    // Memory parameters
    double memory_MB_max = 0.0;          // Maximum memory capacity (MB)
    double memory_MB_available = 0.0;    // Current available memory (MB)
    double memoryUsageFactor = 0.0;      // Current memory usage (0.0-1.0)
    
    // ============================================================================
    // TASK PROCESSING SYSTEM (New for Digital Twin Design)
    // ============================================================================
    
    // Task Generation Parameters (from omnetpp.ini)
    double task_arrival_mean = 2.0;           // Mean inter-arrival time (seconds)
    double task_size_min = 1e5;               // 100 KB
    double task_size_max = 1.5e6;             // 1.5 MB
    double cpu_per_byte_min = 2e3;            // 2K cycles/byte
    double cpu_per_byte_max = 5e4;            // 50K cycles/byte
    double deadline_min = 0.15;               // Minimum deadline (seconds)
    double deadline_max = 0.35;               // Maximum deadline (seconds)
    
    // Resource Capacity (initialized from omnetpp.ini)
    double cpu_total = 0.0;                   // Total CPU capacity (Hz)
    double cpu_reservation_ratio = 0.0;       // Reserved for safety (20-40%)
    double cpu_allocable = 0.0;               // CPU available for tasks (after reservation)
    double cpu_available = 0.0;               // Currently free CPU
    double memory_total = 0.0;                // Total memory (bytes)
    double memory_available = 0.0;            // Currently free memory
    
    // Task Queue Management
    std::priority_queue<Task*, std::vector<Task*>, TaskComparator> pending_tasks;
    std::set<Task*> processing_tasks;         // Currently executing
    std::deque<Task*> completed_tasks;        // History (limited size)
    std::deque<Task*> failed_tasks;           // History (limited size)
    
    size_t max_queue_size = 50;               // Maximum pending tasks
    size_t max_concurrent_tasks = 4;          // Maximum parallel execution
    double min_cpu_guarantee = 0.1;           // 10% of allocable per task
    
    // Task Statistics
    uint64_t task_sequence_number = 0;        // For unique task IDs
    uint32_t tasks_generated = 0;
    uint32_t tasks_completed_on_time = 0;
    uint32_t tasks_completed_late = 0;
    uint32_t tasks_failed = 0;
    uint32_t tasks_rejected = 0;
    
    double total_completion_time = 0.0;       // Sum for average calculation
    
    // Self-message for task generation
    cMessage* taskGenerationEvent = nullptr;
    
    // Task Processing Methods
    void initializeTaskSystem();              // Initialize task processing parameters
    void scheduleNextTaskGeneration();        // Schedule next task arrival (Poisson)
    void generateTask();                      // Generate new task with random characteristics
    bool canAcceptTask(Task* task);           // Check if task can be accepted
    bool canStartProcessing(Task* task);      // Check if task can start immediately
    void allocateResourcesAndStart(Task* task); // Allocate CPU/memory and start processing
    double calculateCPUAllocation(Task* task); // Calculate CPU for a task
    void reallocateCPUResources();            // Redistribute CPU among processing tasks
    void processQueuedTasks();                // Try to start queued tasks
    void handleTaskCompletion(Task* task);    // Task finished successfully
    void handleTaskDeadline(Task* task);      // Task deadline expired
    void dropLowestPriorityTask();            // Drop task when queue is full
    
    // Communication Methods
    void sendTaskMetadataToRSU(Task* task);   // Send metadata to RSU
    void sendTaskCompletionToRSU(Task* task); // Send completion notification
    void sendTaskFailureToRSU(Task* task, const std::string& reason); // Send failure notification
    void sendResourceStatusToRSU();           // Send periodic heartbeat
    void sendVehicleResourceStatus();         // Wrapper for resource status sending
    
    // Service Vehicle Task Completion Handlers
    void handleServiceTaskCompletion(Task* task);  // Service task finished
    void handleServiceTaskDeadline(Task* task);    // Service task deadline expired
    
    // Logging Methods
    void logResourceState(const std::string& context);
    void logQueueState(const std::string& context);
    void logTaskStatistics();
    
    // Helper methods
    veins::LAddress::L2Type findRSUMacAddress();
    std::string createVehicleDataPayload();  // Create payload with actual vehicle data
    void updateVehicleData();                // Update current vehicle parameters
    
    // ============================================================================
    // MODERN RSU SELECTION SYSTEM
    // ============================================================================
    
    // RSU Metrics Structure
    struct RSUMetrics {
        veins::LAddress::L2Type macAddress = 0;
        double lastRSSI = -999.0;                    // Last measured RSSI (dBm)
        double distance = 0.0;                       // Distance to RSU (meters)
        std::deque<bool> receptionHistory;           // Recent packet reception success/fail
        int consecutiveFailures = 0;                 // Count of consecutive failures
        simtime_t lastContactTime = 0;               // Last successful contact
        double score = 0.0;                          // Calculated selection score
        
        // Calculate Packet Reception Ratio (PRR)
        double getPRR() const {
            if (receptionHistory.empty()) return 0.0;
            int successes = 0;
            for (bool success : receptionHistory) {
                if (success) successes++;
            }
            return (double)successes / receptionHistory.size();
        }
    };
    
    // ============================================================================
    // TASK OFFLOADING DECISION FRAMEWORK
    // ============================================================================
    
    // Task timing tracking for latency calculations
    struct TaskTimingInfo {
        double request_time = 0.0;      // When offloading request was sent
        double decision_time = 0.0;     // When RSU decision was received
        double start_time = 0.0;        // When processing started
        std::string decision_type;      // LOCAL, RSU, SERVICE_VEHICLE
        std::string processor_id;       // ID of processor
    };
    
    std::map<std::string, TaskTimingInfo> taskTimings;  // task_id -> timing info
    
    bool offloadingEnabled = false;                        // Enable/disable offloading
    TaskOffloadingDecisionMaker* decisionMaker = nullptr;  // Offloading decision maker
    
    // Pending offloading requests (awaiting RSU decision)
    std::map<std::string, Task*> pendingOffloadingDecisions;  // task_id -> Task*
    
    // Offloaded tasks (awaiting results)
    std::map<std::string, Task*> offloadedTasks;  // task_id -> Task*
    std::map<std::string, std::string> offloadedTaskTargets;  // task_id -> target ("RSU" or vehicle_id)
    
    // Timeout parameters
    simtime_t rsuDecisionTimeout = 1.0;            // Timeout for RSU decision response
    simtime_t offloadedTaskTimeout = 5.0;          // Timeout for task execution result
    
    // Offloading helper methods
    double getRSUDistance();                          // Get distance to selected RSU
    double getEstimatedRSSI();                        // Get estimated RSSI to RSU
    double estimateTransmissionTime(Task* task);     // Estimate network transmission time
    
    // Offloading request/response handlers
    void sendOffloadingRequestToRSU(Task* task, OffloadingDecision localDecision);
    void handleOffloadingDecisionFromRSU(veins::OffloadingDecisionMessage* msg);
    void executeOffloadingDecision(Task* task, veins::OffloadingDecisionMessage* decision);
    
    // Task execution methods
    void sendTaskToRSU(Task* task);
    void sendTaskToServiceVehicle(Task* task, const std::string& serviceVehicleId, veins::LAddress::L2Type serviceMac);
    void handleTaskResult(veins::TaskResultMessage* msg);
    void sendTaskOffloadingEvent(const std::string& taskId, const std::string& eventType, 
                                  const std::string& sourceEntity, const std::string& targetEntity);
    void sendTaskCompletionToRSU(Task* task, bool success, const std::string& failureReason = "");
    void sendTaskCompletionToRSU(const std::string& taskId, double completionTime, 
                                  bool success, bool onTime, 
                                  uint64_t taskSizeBytes, uint64_t cpuCycles, 
                                  double qosValue, const std::string& resultData);
    
    // Service vehicle capability (this vehicle can process tasks for others)
    bool serviceVehicleEnabled = false;
    int maxConcurrentServiceTasks = 3;
    double serviceCpuReservation = 0.3;  // 30% CPU reserved for service
    double serviceMemoryReservation = 512.0;  // 512 MB reserved
    std::queue<Task*> serviceTasks;  // Queue for tasks from other vehicles
    std::set<Task*> processingServiceTasks;  // Currently processing service tasks
    std::map<std::string, std::string> serviceTaskOriginVehicles;  // task_id -> origin_vehicle_id
    std::map<std::string, veins::LAddress::L2Type> serviceTaskOriginMACs;  // task_id -> origin_mac
    
    void handleServiceTaskRequest(veins::TaskOffloadPacket* msg);
    void processServiceTask(Task* task);
    void sendServiceTaskResult(Task* task, const std::string& originalVehicleId);
    
    // RSU Selection Members
    std::map<int, RSUMetrics> rsuMetrics;            // Metrics for each RSU (indexed by RSU number)
    veins::LAddress::L2Type currentRSU = 0;          // Currently selected RSU
    
    // RSU Selection Parameters
    double rssi_threshold = -90.0;                   // Minimum acceptable RSSI (dBm)
    double hysteresis_margin = 3.0;                  // dB margin for switching
    int failure_blacklist_threshold = 5;             // Consecutive failures before blacklist
    simtime_t blacklist_duration = 10.0;             // Seconds to blacklist RSU
    int prr_window_size = 10;                        // Number of packets to track for PRR
    
    // RSU Selection Methods
    veins::LAddress::L2Type selectBestRSU();         // Modern multi-criteria RSU selection
    void updateRSUMetrics(int rsuIndex, bool messageSuccess, double rssi = -999.0);
    double calculateRSUScore(const RSUMetrics& metrics);
    double normalizeValue(double value, double min, double max);
};

} // namespace complex_network

#endif // PAYLOADVEHICLEAPP_H
