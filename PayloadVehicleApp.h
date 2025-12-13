#ifndef PAYLOADVEHICLEAPP_H
#define PAYLOADVEHICLEAPP_H

#include "veins/modules/application/ieee80211p/DemoBaseApplLayer.h"
#include "veins/modules/messages/DemoSafetyMessage_m.h"
#include "veins/modules/mobility/traci/TraCIMobility.h"
#include "veins/base/utils/FindModule.h"
#include "veins/base/utils/Coord.h"
#include "Task.h"
#include "TaskMetadataMessage_m.h"
#include <queue>
#include <set>
#include <vector>
#include <deque>

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
    
    // Logging Methods
    void logResourceState(const std::string& context);
    void logQueueState(const std::string& context);
    void logTaskStatistics();
    
    // Helper methods
    veins::LAddress::L2Type findRSUMacAddress();
    std::string createVehicleDataPayload();  // Create payload with actual vehicle data
    void updateVehicleData();                // Update current vehicle parameters
};

} // namespace complex_network

#endif // PAYLOADVEHICLEAPP_H
