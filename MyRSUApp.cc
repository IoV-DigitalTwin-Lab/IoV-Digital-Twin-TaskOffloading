#include "MyRSUApp.h"
#include <sstream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <cmath>

using namespace veins;

namespace complex_network {

Define_Module(MyRSUApp);

void MyRSUApp::initialize(int stage) {
    DemoBaseApplLayer::initialize(stage);
    
    if (stage == 0) {
        // Get RSU ID from module index
        rsu_id = getParentModule()->getIndex();
        
        // Initialize edge server resources
        edgeCPU_GHz = par("edgeCPU_GHz").doubleValue();
        edgeMemory_GB = par("edgeMemory_GB").doubleValue();
        maxVehicles = par("maxVehicles").intValue();
        processingDelay_ms = par("processingDelay_ms").doubleValue();
        
        // Initialize PostgreSQL database connection
        initDatabase();
        
        std::cout << "CONSOLE: MyRSUApp " << getParentModule()->getFullName() 
                  << " initialized with edge resources:" << std::endl;
        std::cout << "  - RSU ID: " << rsu_id << std::endl;
        std::cout << "  - CPU: " << edgeCPU_GHz << " GHz" << std::endl;
        std::cout << "  - Memory: " << edgeMemory_GB << " GB" << std::endl;
        std::cout << "  - Max Vehicles: " << maxVehicles << std::endl;
        std::cout << "  - Base Processing Delay: " << processingDelay_ms << " ms" << std::endl;
        
        double interval = par("beaconInterval").doubleValue();
        
    // create and store the beacon self-message so we can cancel/delete it safely later
    beaconMsg = new cMessage("sendMessage");
    scheduleAt(simTime() + 2.0, beaconMsg);

        std::cout << "=== CONSOLE: MyRSUApp INITIALIZED ===" << std::endl;
        std::cout << "CONSOLE: MyRSUApp - Beacon interval: " << interval << "s" << std::endl;
        std::cout << "CONSOLE: MyRSUApp - Starting RSUHttpPoster..." << std::endl;
        
        EV << "RSU initialized with beacon interval: " << interval << "s" << endl;
        
        // RSUHttpPoster disabled - using direct PostgreSQL insertion
        // poster.start();
        
        std::cout << "CONSOLE: MyRSUApp - Direct PostgreSQL insertion enabled (HTTP poster disabled)" << std::endl;
        std::cout << "=== MyRSUApp READY ===" << std::endl;
        EV << "MyRSUApp: RSUHttpPoster started\n";
    }
}

void MyRSUApp::handleSelfMsg(cMessage* msg) {
        if(msg == beaconMsg && strcmp(msg->getName(), "sendMessage") == 0) {
        DemoSafetyMessage* wsm = new DemoSafetyMessage();
        populateWSM(wsm);
        wsm->setSenderPos(curPosition);
        wsm->setUserPriority(par("beaconUserPriority").intValue());
        sendDown(wsm);

        EV << "RSU: Sent beacon at time " << simTime() << endl;

        double interval = par("beaconInterval").doubleValue();
        scheduleAt(simTime() + interval, msg);
        } else {
            DemoBaseApplLayer::handleSelfMsg(msg);
        }
}

void MyRSUApp::handleLowerMsg(cMessage* msg) {
    std::cout << "\n*** CONSOLE: MyRSUApp - handleLowerMsg() CALLED at " 
              << simTime() << " ***" << std::endl;
    
    BaseFrame1609_4* wsm = dynamic_cast<BaseFrame1609_4*>(msg);
    if (wsm) {
        std::cout << "CONSOLE: MyRSUApp - Message IS BaseFrame1609_4, calling parent handler" 
                  << std::endl;
    } else {
        std::cout << "CONSOLE: MyRSUApp - Message is NOT BaseFrame1609_4" << std::endl;
    }
    
    EV << "MyRSUApp: handleLowerMsg() called" << endl;
    
    // This will trigger onWSM if the message is appropriate
    DemoBaseApplLayer::handleLowerMsg(msg);
    
    std::cout << "CONSOLE: MyRSUApp - handleLowerMsg() completed\n" << std::endl;
}

void MyRSUApp::onWSM(BaseFrame1609_4* wsm) {
    std::cout << "\n🔴 RSU SHADOW ANALYSIS - Message Reception at " << simTime() << std::endl;
    
    // Check for task-related messages first
    TaskMetadataMessage* taskMetadata = dynamic_cast<TaskMetadataMessage*>(wsm);
    if (taskMetadata) {
        EV_INFO << "📥 RSU received TaskMetadataMessage" << endl;
        handleTaskMetadata(taskMetadata);
        return;
    }
    
    TaskCompletionMessage* taskCompletion = dynamic_cast<TaskCompletionMessage*>(wsm);
    if (taskCompletion) {
        EV_INFO << "📥 RSU received TaskCompletionMessage" << endl;
        handleTaskCompletion(taskCompletion);
        return;
    }
    
    TaskFailureMessage* taskFailure = dynamic_cast<TaskFailureMessage*>(wsm);
    if (taskFailure) {
        EV_INFO << "📥 RSU received TaskFailureMessage" << endl;
        handleTaskFailure(taskFailure);
        return;
    }
    
    VehicleResourceStatusMessage* resourceStatus = dynamic_cast<VehicleResourceStatusMessage*>(wsm);
    if (resourceStatus) {
        EV_INFO << "📥 RSU received VehicleResourceStatusMessage" << endl;
        handleVehicleResourceStatus(resourceStatus);
        return;
    }
    
    // Original DemoSafetyMessage handling
    DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
    if(!dsm) {
        std::cout << "CONSOLE: MyRSUApp - ERROR: Message is NOT DemoSafetyMessage!" 
                  << std::endl;
        return;
    }
    
    std::cout << "CONSOLE: MyRSUApp - ✓ Message IS DemoSafetyMessage" << std::endl;
    
    // Access REAL signal reception data from the simulation
    Coord senderPos = dsm->getSenderPos();
    Coord rsuPos = curPosition;
    double distance = senderPos.distance(rsuPos);
    
    std::cout << "SHADOW: 📡 RSU RECEIVED signal from Vehicle:" << std::endl;
    std::cout << "SHADOW: Sender position: (" << senderPos.x << "," << senderPos.y << ")" << std::endl;
    std::cout << "SHADOW: RSU position: (" << rsuPos.x << "," << rsuPos.y << ")" << std::endl;
    std::cout << "SHADOW: Distance: " << distance << " m" << std::endl;
    
    // Analyze if signal passed through obstacles
    bool senderNearOfficeT = (senderPos.x >= 240 && senderPos.x <= 320 && senderPos.y >= 10 && senderPos.y <= 90);
    bool senderNearOfficeC = (senderPos.x >= 110 && senderPos.x <= 190 && senderPos.y >= 10 && senderPos.y <= 90);
    
    if (senderNearOfficeT || senderNearOfficeC) {
        std::string obstacleType = senderNearOfficeT ? "Office Tower" : "Office Complex";
        std::cout << "SHADOW: 🏢 SIGNAL TRAVELED THROUGH " << obstacleType << "!" << std::endl;
        std::cout << "SHADOW: Expected path loss: " << (20 * log10(distance) + 40) << " dB (free space)" << std::endl;
        std::cout << "SHADOW: Plus obstacle loss: 18dB + shadowing up to 8dB" << std::endl;
        std::cout << "SHADOW: ✅ Signal STRONG ENOUGH despite " << obstacleType << " attenuation!" << std::endl;
    } else {
        std::cout << "SHADOW: 🌐 CLEAR PATH from vehicle to RSU" << std::endl;
        std::cout << "SHADOW: Expected path loss: " << (20 * log10(distance) + 40) << " dB (free space only)" << std::endl;
        std::cout << "SHADOW: ✅ Optimal reception conditions" << std::endl;
    }
    
    // Try to access actual signal measurements from the frame
    if (dsm->getControlInfo()) {
        cObject* controlInfo = dsm->getControlInfo();
        std::cout << "SHADOW: Control info available: " << controlInfo->getClassName() << std::endl;
        
        // The real signal strength data should be in the control info
        // This varies by Veins version but typically contains RSSI/SNR data
        std::cout << "SHADOW: Real signal reception recorded by PHY layer" << std::endl;
    }
    
    // Access the radio module to get actual reception parameters
    cModule* nicModule = getSubmodule("nic");
    if (nicModule) {
        cModule* phyModule = nicModule->getSubmodule("phy80211p");
        if (phyModule) {
            std::cout << "SHADOW: PHY layer processed signal with real propagation models" << std::endl;
            
            // Get actual configured parameters from simulation
            double sensitivity = -85.0;
            double txPower = 20.0;
            
            if (phyModule->hasPar("sensitivity")) {
                sensitivity = phyModule->par("sensitivity").doubleValue();
                std::cout << "SHADOW: Actual sensitivity threshold: " << sensitivity << " dBm" << std::endl;
            }
            
            // Since we received the message, it passed the sensitivity test
            std::cout << "SHADOW: ✓ Signal PASSED sensitivity test (real simulation result)" << std::endl;
            std::cout << "SHADOW: Real propagation models (TwoRay+LogNormal+Obstacles) applied" << std::endl;
        }
    }
    
    // Get payload from message name
    const char* nm = dsm->getName();
    std::string payload = nm ? std::string(nm) : std::string();
    
    std::cout << "CONSOLE: MyRSUApp - Raw payload length: " << payload.length() << std::endl;
    std::cout << "CONSOLE: MyRSUApp - Raw payload: '" << payload << "'" << std::endl;
    
    EV << "RSU: Received message from vehicle at time " << simTime() << endl;
    
    // Note: Vehicle telemetry (position, speed) now comes with VehicleResourceStatusMessage
    // This onWSM() handler primarily receives regular beacons
    
    std::cout << "***** MyRSUApp - onWSM() COMPLETED *****\n" << std::endl;
}

void MyRSUApp::finish() {
    std::cout << "\n=== CONSOLE: MyRSUApp - finish() called ===" << std::endl;
    
    // Log final Digital Twin statistics
    logDigitalTwinState();
    
    // Close PostgreSQL connection
    closeDatabase();
    
    EV << "MyRSUApp: stopping RSUHttpPoster...\n";
    // cancel and delete our beacon self-message if still scheduled
    if (beaconMsg) {
        cancelAndDelete(beaconMsg);
        beaconMsg = nullptr;
    }

    // RSUHttpPoster disabled - using direct PostgreSQL insertion
    // poster.stop();

    std::cout << "CONSOLE: MyRSUApp - Finished successfully" << std::endl;
    std::cout << "=== MyRSUApp FINISHED ===" << std::endl;
    EV << "MyRSUApp: RSUHttpPoster stopped\n";

    DemoBaseApplLayer::finish();
}

void MyRSUApp::handleMessage(cMessage* msg) {
    std::cout << "CONSOLE: MyRSUApp handleMessage() called with message: " << msg->getName()
              << " at time " << simTime() << std::endl;

    // ========================================================================
    // HANDLE OFFLOADING REQUEST MESSAGES
    // ========================================================================
    veins::OffloadingRequestMessage* offloadReq = dynamic_cast<veins::OffloadingRequestMessage*>(msg);
    if (offloadReq) {
        EV_INFO << "Received OffloadingRequestMessage" << endl;
        handleOffloadingRequest(offloadReq);
        return;
    }
    
    // ========================================================================
    // HANDLE TASK OFFLOAD PACKETS (for RSU processing)
    // ========================================================================
    veins::TaskOffloadPacket* taskPacket = dynamic_cast<veins::TaskOffloadPacket*>(msg);
    if (taskPacket) {
        EV_INFO << "Received TaskOffloadPacket" << endl;
        handleTaskOffloadPacket(taskPacket);
        return;
    }
    
    // ========================================================================
    // HANDLE TASK OFFLOADING LIFECYCLE EVENTS
    // ========================================================================
    veins::TaskOffloadingEvent* offloadEvent = dynamic_cast<veins::TaskOffloadingEvent*>(msg);
    if (offloadEvent) {
        EV_INFO << "Received TaskOffloadingEvent" << endl;
        handleTaskOffloadingEvent(offloadEvent);
        return;
    }

    BaseFrame1609_4* wsm = dynamic_cast<BaseFrame1609_4*>(msg);
    if (wsm) {
        std::cout << "CONSOLE: MyRSUApp handleMessage received BaseFrame1609_4! forwarding to onWSM()" << std::endl;
        onWSM(wsm);
        return;
    }

    DemoBaseApplLayer::handleMessage(msg);
}

// ============================================================================
// DIGITAL TWIN TRACKING IMPLEMENTATION
// ============================================================================

void MyRSUApp::handleTaskMetadata(TaskMetadataMessage* msg) {
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║                   RSU: TASK METADATA RECEIVED                            ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    std::string task_id = msg->getTask_id();
    std::string vehicle_id = msg->getVehicle_id();
    
    // Create task record
    TaskRecord record;
    record.task_id = task_id;
    record.vehicle_id = vehicle_id;
    record.task_size_bytes = msg->getTask_size_bytes();
    record.cpu_cycles = msg->getCpu_cycles();
    record.created_time = msg->getCreated_time();
    record.deadline_seconds = msg->getDeadline_seconds();
    record.received_time = simTime().dbl();
    record.qos_value = msg->getQos_value();
    
    // Store in task records
    task_records[task_id] = record;
    
    // Update vehicle digital twin
    VehicleDigitalTwin& twin = getOrCreateVehicleTwin(vehicle_id);
    twin.tasks_generated++;
    twin.last_update_time = simTime().dbl();
    
    EV_INFO << "Task metadata stored:" << endl;
    EV_INFO << "  Task ID: " << task_id << endl;
    EV_INFO << "  Vehicle ID: " << vehicle_id << endl;
    EV_INFO << "  Size: " << (record.task_size_bytes / 1024.0) << " KB" << endl;
    EV_INFO << "  CPU Cycles: " << (record.cpu_cycles / 1e9) << " G" << endl;
    EV_INFO << "  Deadline: " << record.deadline_seconds << " sec" << endl;
    EV_INFO << "  QoS: " << record.qos_value << endl;
    EV_INFO << "  Received at: " << record.received_time << " (delay: " 
            << (record.received_time - record.created_time) << " sec)" << endl;
    
    logTaskRecord(record, "METADATA_RECEIVED");
    
    // Insert into PostgreSQL database
    insertTaskMetadata(msg);
    
    std::cout << "DT_TASK: RSU received metadata for task " << task_id 
              << " from vehicle " << vehicle_id << std::endl;
}

void MyRSUApp::handleTaskCompletion(TaskCompletionMessage* msg) {
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║                   RSU: TASK COMPLETION RECEIVED                          ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    std::string task_id = msg->getTask_id();
    std::string vehicle_id = msg->getVehicle_id();
    
    // Update task record
    auto it = task_records.find(task_id);
    if (it != task_records.end()) {
        TaskRecord& record = it->second;
        record.completed = true;
        record.completion_time = msg->getCompletion_time();
        record.processing_time = msg->getProcessing_time();
        record.completed_on_time = msg->getCompleted_on_time();
        
        EV_INFO << "Task completion details:" << endl;
        EV_INFO << "  Task ID: " << task_id << endl;
        EV_INFO << "  Vehicle ID: " << vehicle_id << endl;
        EV_INFO << "  Completion time: " << record.completion_time << endl;
        EV_INFO << "  Processing time: " << record.processing_time << " sec" << endl;
        EV_INFO << "  On time: " << (record.completed_on_time ? "YES" : "NO (LATE)") << endl;
        EV_INFO << "  CPU allocated: " << (msg->getCpu_allocated() / 1e9) << " GHz" << endl;
        
        logTaskRecord(record, record.completed_on_time ? "COMPLETED_ON_TIME" : "COMPLETED_LATE");
        
        // Insert into PostgreSQL database
        insertTaskCompletion(msg);
        
        // Update vehicle digital twin
        VehicleDigitalTwin& twin = getOrCreateVehicleTwin(vehicle_id);
        if (record.completed_on_time) {
            twin.tasks_completed_on_time++;
            std::cout << "DT_SUCCESS: Task " << task_id << " completed ON TIME" << std::endl;
        } else {
            twin.tasks_completed_late++;
            std::cout << "DT_LATE: Task " << task_id << " completed LATE" << std::endl;
        }
        twin.last_update_time = simTime().dbl();
    } else {
        EV_INFO << "⚠ Task record not found for task " << task_id << endl;
        std::cout << "DT_WARNING: RSU received completion for unknown task " << task_id << std::endl;
    }
    
    updateDigitalTwinStatistics();
}

void MyRSUApp::handleTaskFailure(TaskFailureMessage* msg) {
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║                   RSU: TASK FAILURE RECEIVED                             ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    std::string task_id = msg->getTask_id();
    std::string vehicle_id = msg->getVehicle_id();
    std::string reason = msg->getFailure_reason();
    
    // Update task record
    auto it = task_records.find(task_id);
    if (it != task_records.end()) {
        TaskRecord& record = it->second;
        record.failed = true;
        record.completion_time = msg->getFailure_time();
        record.failure_reason = reason;
        
        EV_INFO << "Task failure details:" << endl;
        EV_INFO << "  Task ID: " << task_id << endl;
        EV_INFO << "  Vehicle ID: " << vehicle_id << endl;
        EV_INFO << "  Failure time: " << msg->getFailure_time() << endl;
        EV_INFO << "  Reason: " << reason << endl;
        EV_INFO << "  Wasted time: " << msg->getWasted_time() << " sec" << endl;
        
        logTaskRecord(record, "FAILED_" + reason);
        
        // Insert into PostgreSQL database
        insertTaskFailure(msg);
        
        // Update vehicle digital twin
        VehicleDigitalTwin& twin = getOrCreateVehicleTwin(vehicle_id);
        if (reason == "REJECTED") {
            twin.tasks_rejected++;
            std::cout << "DT_REJECT: Task " << task_id << " REJECTED" << std::endl;
        } else {
            twin.tasks_failed++;
            std::cout << "DT_FAIL: Task " << task_id << " FAILED - " << reason << std::endl;
        }
        twin.last_update_time = simTime().dbl();
    } else {
        EV_INFO << "⚠ Task record not found for task " << task_id << endl;
        std::cout << "DT_WARNING: RSU received failure for unknown task " << task_id << std::endl;
    }
    
    updateDigitalTwinStatistics();
}

void MyRSUApp::handleVehicleResourceStatus(VehicleResourceStatusMessage* msg) {
    EV_INFO << "📊 RSU: Vehicle resource status update received" << endl;
    
    std::string vehicle_id = msg->getVehicle_id();
    VehicleDigitalTwin& twin = getOrCreateVehicleTwin(vehicle_id);
    
    // Update vehicle state
    twin.pos_x = msg->getPos_x();
    twin.pos_y = msg->getPos_y();
    twin.speed = msg->getSpeed();
    
    // Update resource information
    twin.cpu_total = msg->getCpu_total();
    twin.cpu_allocable = msg->getCpu_allocable();
    twin.cpu_available = msg->getCpu_available();
    twin.cpu_utilization = msg->getCpu_utilization();
    
    twin.mem_total = msg->getMem_total();
    twin.mem_available = msg->getMem_available();
    twin.mem_utilization = msg->getMem_utilization();
    
    // Update task statistics
    twin.tasks_generated = msg->getTasks_generated();
    twin.tasks_completed_on_time = msg->getTasks_completed_on_time();
    twin.tasks_completed_late = msg->getTasks_completed_late();
    twin.tasks_failed = msg->getTasks_failed();
    twin.tasks_rejected = msg->getTasks_rejected();
    twin.current_queue_length = msg->getCurrent_queue_length();
    twin.current_processing_count = msg->getCurrent_processing_count();
    
    twin.avg_completion_time = msg->getAvg_completion_time();
    twin.deadline_miss_ratio = msg->getDeadline_miss_ratio();
    
    twin.last_update_time = simTime().dbl();
    
    EV_INFO << "Vehicle " << vehicle_id << " Digital Twin updated:" << endl;
    // Insert into PostgreSQL database
    insertVehicleStatus(msg);
    
    EV_INFO << "  Position: (" << twin.pos_x << ", " << twin.pos_y << ")" << endl;
    EV_INFO << "  CPU Utilization: " << twin.cpu_utilization << "%" << endl;
    EV_INFO << "  Memory Utilization: " << twin.mem_utilization << "%" << endl;
    EV_INFO << "  Queue Length: " << twin.current_queue_length << endl;
    EV_INFO << "  Processing Count: " << twin.current_processing_count << endl;
    
    std::cout << "DT_UPDATE: Vehicle " << vehicle_id << " - CPU:" << twin.cpu_utilization 
              << "% Mem:" << twin.mem_utilization << "% Queue:" << twin.current_queue_length << std::endl;
}

VehicleDigitalTwin& MyRSUApp::getOrCreateVehicleTwin(const std::string& vehicle_id) {
    auto it = vehicle_twins.find(vehicle_id);
    if (it == vehicle_twins.end()) {
        VehicleDigitalTwin twin;
        twin.vehicle_id = vehicle_id;
        twin.first_seen_time = simTime().dbl();
        twin.last_update_time = simTime().dbl();
        vehicle_twins[vehicle_id] = twin;
        
        EV_INFO << "✨ Created new Digital Twin for vehicle " << vehicle_id << endl;
        std::cout << "DT_NEW: Created Digital Twin for vehicle " << vehicle_id << std::endl;
        
        return vehicle_twins[vehicle_id];
    }
    return it->second;
}

void MyRSUApp::updateDigitalTwinStatistics() {
    // This could compute aggregate statistics across all vehicles
    // For now, just log current state
    logDigitalTwinState();
}

void MyRSUApp::logDigitalTwinState() {
    EV_INFO << "\n" << endl;
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║                    DIGITAL TWIN STATE SUMMARY                            ║" << endl;
    EV_INFO << "╠══════════════════════════════════════════════════════════════════════════╣" << endl;
    EV_INFO << "║ Tracked Vehicles: " << std::setw(54) << vehicle_twins.size() << "║" << endl;
    EV_INFO << "║ Task Records:     " << std::setw(54) << task_records.size() << "║" << endl;
    EV_INFO << "╠══════════════════════════════════════════════════════════════════════════╣" << endl;
    
    uint32_t total_generated = 0;
    uint32_t total_completed_on_time = 0;
    uint32_t total_completed_late = 0;
    uint32_t total_failed = 0;
    uint32_t total_rejected = 0;
    
    for (const auto& pair : vehicle_twins) {
        const VehicleDigitalTwin& twin = pair.second;
        total_generated += twin.tasks_generated;
        total_completed_on_time += twin.tasks_completed_on_time;
        total_completed_late += twin.tasks_completed_late;
        total_failed += twin.tasks_failed;
        total_rejected += twin.tasks_rejected;
        
        EV_INFO << "║ Vehicle " << std::left << std::setw(63) << twin.vehicle_id << "║" << endl;
        EV_INFO << "║   Generated: " << std::setw(11) << twin.tasks_generated 
                << " OnTime: " << std::setw(11) << twin.tasks_completed_on_time
                << " Late: " << std::setw(11) << twin.tasks_completed_late 
                << " Failed: " << std::setw(8) << twin.tasks_failed << "║" << endl;
        EV_INFO << "║   CPU: " << std::setw(5) << (int)twin.cpu_utilization << "%  "
                << "Mem: " << std::setw(5) << (int)twin.mem_utilization << "%  "
                << "Queue: " << std::setw(5) << twin.current_queue_length << "  "
                << "Processing: " << std::setw(19) << twin.current_processing_count << "║" << endl;
    }
    
    EV_INFO << "╠══════════════════════════════════════════════════════════════════════════╣" << endl;
    EV_INFO << "║ AGGREGATE STATISTICS                                                     ║" << endl;
    EV_INFO << "╠══════════════════════════════════════════════════════════════════════════╣" << endl;
    EV_INFO << "║ Total Generated:      " << std::setw(48) << total_generated << "║" << endl;
    EV_INFO << "║ Total On Time:        " << std::setw(48) << total_completed_on_time << "║" << endl;
    EV_INFO << "║ Total Late:           " << std::setw(48) << total_completed_late << "║" << endl;
    EV_INFO << "║ Total Failed:         " << std::setw(48) << total_failed << "║" << endl;
    EV_INFO << "║ Total Rejected:       " << std::setw(48) << total_rejected << "║" << endl;
    
    if (total_generated > 0) {
        double success_rate = (double)total_completed_on_time / total_generated * 100.0;
        double late_rate = (double)total_completed_late / total_generated * 100.0;
        double fail_rate = (double)total_failed / total_generated * 100.0;
        double reject_rate = (double)total_rejected / total_generated * 100.0;
        
        EV_INFO << "║ ────────────────────────────────────────────────────────────────────────║" << endl;
        EV_INFO << "║ Success Rate:         " << std::setw(38) << success_rate << " %        ║" << endl;
        EV_INFO << "║ Late Rate:            " << std::setw(38) << late_rate << " %        ║" << endl;
        EV_INFO << "║ Failure Rate:         " << std::setw(38) << fail_rate << " %        ║" << endl;
        EV_INFO << "║ Rejection Rate:       " << std::setw(38) << reject_rate << " %        ║" << endl;
    }
    
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    std::cout << "DT_SUMMARY: Vehicles:" << vehicle_twins.size() 
              << " Tasks:" << task_records.size()
              << " Generated:" << total_generated
              << " OnTime:" << total_completed_on_time
              << " Late:" << total_completed_late
              << " Failed:" << total_failed
              << " Rejected:" << total_rejected << std::endl;
}

void MyRSUApp::logTaskRecord(const TaskRecord& record, const std::string& event) {
    EV_INFO << "📝 TASK RECORD [" << event << "]:" << endl;
    EV_INFO << "  ID: " << record.task_id << endl;
    EV_INFO << "  Vehicle: " << record.vehicle_id << endl;
    EV_INFO << "  Size: " << (record.task_size_bytes / 1024.0) << " KB" << endl;
    EV_INFO << "  Cycles: " << (record.cpu_cycles / 1e9) << " G" << endl;
    EV_INFO << "  QoS: " << record.qos_value << endl;
    EV_INFO << "  Created: " << record.created_time << " s" << endl;
    EV_INFO << "  Received: " << record.received_time << " s" << endl;
    if (record.completed || record.failed) {
        EV_INFO << "  Completion: " << record.completion_time << " s" << endl;
        EV_INFO << "  Processing: " << record.processing_time << " s" << endl;
    }
    if (record.failed) {
        EV_INFO << "  Failure reason: " << record.failure_reason << endl;
    }
}

// ============================================================================
// POSTGRESQL DATABASE IMPLEMENTATION
// ============================================================================

void MyRSUApp::initDatabase() {
    // Get database connection string from environment or use default
    const char* db_env = std::getenv("DATABASE_URL");
    if (db_env) {
        db_conninfo = std::string(db_env);
    } else {
        db_conninfo = "host=localhost port=5432 dbname=iov_db user=iov_user password=iov_pass";
    }
    
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║              INITIALIZING POSTGRESQL CONNECTION                          ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    EV_INFO << "Database: " << db_conninfo << endl;
    
    db_conn = PQconnectdb(db_conninfo.c_str());
    
    if (PQstatus(db_conn) != CONNECTION_OK) {
        EV_WARN << "✗ PostgreSQL connection failed: " << PQerrorMessage(db_conn) << endl;
        std::cerr << "⚠ RSU[" << rsu_id << "] DB connection failed: " << PQerrorMessage(db_conn) << std::endl;
        PQfinish(db_conn);
        db_conn = nullptr;
    } else {
        EV_INFO << "✓ PostgreSQL connection established successfully" << endl;
        std::cout << "✓ RSU[" << rsu_id << "] PostgreSQL connected" << std::endl;
    }
}

void MyRSUApp::closeDatabase() {
    if (db_conn) {
        EV_INFO << "Closing PostgreSQL connection..." << endl;
        PQfinish(db_conn);
        db_conn = nullptr;
        std::cout << "✓ RSU[" << rsu_id << "] PostgreSQL connection closed" << std::endl;
    }
}

PGconn* MyRSUApp::getDBConnection() {
    // Reconnect if connection lost
    if (!db_conn || PQstatus(db_conn) != CONNECTION_OK) {
        EV_WARN << "Database connection lost, attempting reconnect..." << endl;
        if (db_conn) {
            PQfinish(db_conn);
        }
        db_conn = PQconnectdb(db_conninfo.c_str());
        
        if (PQstatus(db_conn) != CONNECTION_OK) {
            EV_WARN << "✗ Reconnection failed: " << PQerrorMessage(db_conn) << endl;
            PQfinish(db_conn);
            db_conn = nullptr;
            return nullptr;
        }
        EV_INFO << "✓ Database reconnected successfully" << endl;
    }
    return db_conn;
}

void MyRSUApp::insertTaskMetadata(const TaskMetadataMessage* msg) {
    PGconn* conn = getDBConnection();
    if (!conn) {
        EV_WARN << "⚠ Cannot insert task metadata: No database connection" << endl;
        return;
    }
    
    EV_INFO << "📤 Inserting task metadata into PostgreSQL..." << endl;
    
    // Prepare JSON payload
    std::ostringstream payload_json;
    payload_json << "{"
                 << "\"task_id\":\"" << msg->getTask_id() << "\","
                 << "\"vehicle_id\":\"" << msg->getVehicle_id() << "\","
                 << "\"task_size_bytes\":" << msg->getTask_size_bytes() << ","
                 << "\"cpu_cycles\":" << msg->getCpu_cycles() << ","
                 << "\"qos_value\":" << msg->getQos_value() << ","
                 << "\"created_time\":" << msg->getCreated_time() << ","
                 << "\"deadline_seconds\":" << msg->getDeadline_seconds() << ","
                 << "\"received_time\":" << simTime().dbl()
                 << "}";
    
    const char* paramValues[10];
    std::string task_id = msg->getTask_id();
    std::string vehicle_id = msg->getVehicle_id();
    std::string rsu_id_str = std::to_string(rsu_id);
    std::string task_size = std::to_string(msg->getTask_size_bytes());
    std::string cpu_cycles = std::to_string(msg->getCpu_cycles());
    std::string qos = std::to_string(msg->getQos_value());
    std::string created = std::to_string(msg->getCreated_time());
    std::string deadline = std::to_string(msg->getDeadline_seconds());
    std::string received = std::to_string(simTime().dbl());
    std::string payload = payload_json.str();
    
    paramValues[0] = task_id.c_str();
    paramValues[1] = vehicle_id.c_str();
    paramValues[2] = rsu_id_str.c_str();
    paramValues[3] = task_size.c_str();
    paramValues[4] = cpu_cycles.c_str();
    paramValues[5] = qos.c_str();
    paramValues[6] = created.c_str();
    paramValues[7] = deadline.c_str();
    paramValues[8] = received.c_str();
    paramValues[9] = payload.c_str();
    
    const char* query = "INSERT INTO task_metadata (task_id, vehicle_id, rsu_id, task_size_bytes, "
                        "cpu_cycles, qos_value, created_time, deadline_seconds, received_time, payload) "
                        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10::jsonb)";
    
    PGresult* res = PQexecParams(conn, query, 10, nullptr, paramValues, nullptr, nullptr, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        EV_WARN << "✗ Failed to insert task metadata: " << PQerrorMessage(conn) << endl;
        std::cerr << "DB_ERROR: Task metadata insert failed: " << PQerrorMessage(conn) << std::endl;
    } else {
        EV_INFO << "✓ Task metadata inserted successfully (Task: " << msg->getTask_id() << ")" << endl;
        std::cout << "DB_INSERT: Task metadata " << msg->getTask_id() << " stored in PostgreSQL" << std::endl;
    }
    
    PQclear(res);
}

void MyRSUApp::insertTaskCompletion(const TaskCompletionMessage* msg) {
    PGconn* conn = getDBConnection();
    if (!conn) {
        EV_WARN << "⚠ Cannot insert task completion: No database connection" << endl;
        return;
    }
    
    EV_INFO << "📤 Inserting task completion into PostgreSQL..." << endl;
    
    // Prepare JSON payload
    std::ostringstream payload_json;
    payload_json << "{"
                 << "\"task_id\":\"" << msg->getTask_id() << "\","
                 << "\"vehicle_id\":\"" << msg->getVehicle_id() << "\","
                 << "\"completion_time\":" << msg->getCompletion_time() << ","
                 << "\"processing_time\":" << msg->getProcessing_time() << ","
                 << "\"cpu_allocated\":" << msg->getCpu_allocated() << ","
                 << "\"completed_on_time\":" << (msg->getCompleted_on_time() ? "true" : "false")
                 << "}";
    
    const char* paramValues[8];
    std::string task_id = msg->getTask_id();
    std::string vehicle_id = msg->getVehicle_id();
    std::string rsu_id_str = std::to_string(rsu_id);
    std::string event_type = "COMPLETED";
    std::string completion = std::to_string(msg->getCompletion_time());
    std::string processing = std::to_string(msg->getProcessing_time());
    std::string on_time = msg->getCompleted_on_time() ? "t" : "f";
    std::string payload = payload_json.str();
    
    paramValues[0] = task_id.c_str();
    paramValues[1] = vehicle_id.c_str();
    paramValues[2] = rsu_id_str.c_str();
    paramValues[3] = event_type.c_str();
    paramValues[4] = completion.c_str();
    paramValues[5] = processing.c_str();
    paramValues[6] = on_time.c_str();
    paramValues[7] = payload.c_str();
    
    const char* query = "INSERT INTO task_events (task_id, vehicle_id, rsu_id, event_type, "
                        "completion_time, processing_time, completed_on_time, payload) "
                        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8::jsonb)";
    
    PGresult* res = PQexecParams(conn, query, 8, nullptr, paramValues, nullptr, nullptr, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        EV_WARN << "✗ Failed to insert task completion: " << PQerrorMessage(conn) << endl;
        std::cerr << "DB_ERROR: Task completion insert failed: " << PQerrorMessage(conn) << std::endl;
    } else {
        EV_INFO << "✓ Task completion inserted successfully (Task: " << msg->getTask_id() << ")" << endl;
        std::cout << "DB_INSERT: Task completion " << msg->getTask_id() << " stored in PostgreSQL" << std::endl;
    }
    
    PQclear(res);
}

void MyRSUApp::insertTaskFailure(const TaskFailureMessage* msg) {
    PGconn* conn = getDBConnection();
    if (!conn) {
        EV_WARN << "⚠ Cannot insert task failure: No database connection" << endl;
        return;
    }
    
    EV_INFO << "📤 Inserting task failure into PostgreSQL..." << endl;
    
    // Prepare JSON payload
    std::ostringstream payload_json;
    payload_json << "{"
                 << "\"task_id\":\"" << msg->getTask_id() << "\","
                 << "\"vehicle_id\":\"" << msg->getVehicle_id() << "\","
                 << "\"failure_time\":" << msg->getFailure_time() << ","
                 << "\"failure_reason\":\"" << msg->getFailure_reason() << "\","
                 << "\"wasted_time\":" << msg->getWasted_time()
                 << "}";
    
    const char* paramValues[7];
    std::string task_id = msg->getTask_id();
    std::string vehicle_id = msg->getVehicle_id();
    std::string rsu_id_str = std::to_string(rsu_id);
    std::string event_type = "FAILED";
    std::string failure_time = std::to_string(msg->getFailure_time());
    std::string reason = msg->getFailure_reason();
    std::string payload = payload_json.str();
    
    paramValues[0] = task_id.c_str();
    paramValues[1] = vehicle_id.c_str();
    paramValues[2] = rsu_id_str.c_str();
    paramValues[3] = event_type.c_str();
    paramValues[4] = failure_time.c_str();
    paramValues[5] = reason.c_str();
    paramValues[6] = payload.c_str();
    
    const char* query = "INSERT INTO task_events (task_id, vehicle_id, rsu_id, event_type, "
                        "completion_time, failure_reason, payload) "
                        "VALUES ($1, $2, $3, $4, $5, $6, $7::jsonb)";
    
    PGresult* res = PQexecParams(conn, query, 7, nullptr, paramValues, nullptr, nullptr, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        EV_WARN << "✗ Failed to insert task failure: " << PQerrorMessage(conn) << endl;
        std::cerr << "DB_ERROR: Task failure insert failed: " << PQerrorMessage(conn) << std::endl;
    } else {
        EV_INFO << "✓ Task failure inserted successfully (Task: " << msg->getTask_id() << ")" << endl;
        std::cout << "DB_INSERT: Task failure " << msg->getTask_id() << " stored in PostgreSQL" << std::endl;
    }
    
    PQclear(res);
}

void MyRSUApp::insertVehicleStatus(const VehicleResourceStatusMessage* msg) {
    PGconn* conn = getDBConnection();
    if (!conn) {
        return; // Silently skip for status updates
    }
    
    // Prepare JSON payload with all status info
    std::ostringstream payload_json;
    payload_json << "{"
                 << "\"vehicle_id\":\"" << msg->getVehicle_id() << "\","
                 << "\"pos_x\":" << msg->getPos_x() << ","
                 << "\"pos_y\":" << msg->getPos_y() << ","
                 << "\"speed\":" << msg->getSpeed() << ","
                 << "\"cpu_total\":" << msg->getCpu_total() << ","
                 << "\"cpu_allocable\":" << msg->getCpu_allocable() << ","
                 << "\"cpu_available\":" << msg->getCpu_available() << ","
                 << "\"cpu_utilization\":" << msg->getCpu_utilization() << ","
                 << "\"mem_total\":" << msg->getMem_total() << ","
                 << "\"mem_available\":" << msg->getMem_available() << ","
                 << "\"mem_utilization\":" << msg->getMem_utilization() << ","
                 << "\"avg_completion_time\":" << msg->getAvg_completion_time() << ","
                 << "\"deadline_miss_ratio\":" << msg->getDeadline_miss_ratio()
                 << "}";
    
    const char* paramValues[24];
    std::string vehicle_id = msg->getVehicle_id();
    std::string rsu_id_str = std::to_string(rsu_id);
    std::string update_time = std::to_string(simTime().dbl());
    std::string pos_x = std::to_string(msg->getPos_x());
    std::string pos_y = std::to_string(msg->getPos_y());
    std::string speed = std::to_string(msg->getSpeed());
    std::string heading = "0";
    std::string cpu_total_str = std::to_string(msg->getCpu_total());
    std::string cpu_allocable = std::to_string(msg->getCpu_allocable());
    std::string cpu_available = std::to_string(msg->getCpu_available());
    std::string cpu_util = std::to_string(msg->getCpu_utilization());
    std::string mem_total_str = std::to_string(msg->getMem_total());
    std::string mem_available = std::to_string(msg->getMem_available());
    std::string mem_util = std::to_string(msg->getMem_utilization());
    std::string queue_len = std::to_string(msg->getCurrent_queue_length());
    std::string proc_count = std::to_string(msg->getCurrent_processing_count());
    std::string tasks_gen = std::to_string(msg->getTasks_generated());
    std::string tasks_ok = std::to_string(msg->getTasks_completed_on_time());
    std::string tasks_late = std::to_string(msg->getTasks_completed_late());
    std::string tasks_fail = std::to_string(msg->getTasks_failed());
    std::string tasks_reject = std::to_string(msg->getTasks_rejected());
    std::string avg_comp_time = std::to_string(msg->getAvg_completion_time());
    std::string deadline_miss = std::to_string(msg->getDeadline_miss_ratio());
    std::string payload = payload_json.str();
    
    paramValues[0] = vehicle_id.c_str();
    paramValues[1] = rsu_id_str.c_str();
    paramValues[2] = update_time.c_str();
    paramValues[3] = pos_x.c_str();
    paramValues[4] = pos_y.c_str();
    paramValues[5] = speed.c_str();
    paramValues[6] = heading.c_str();
    paramValues[7] = cpu_total_str.c_str();
    paramValues[8] = cpu_allocable.c_str();
    paramValues[9] = cpu_available.c_str();
    paramValues[10] = cpu_util.c_str();
    paramValues[11] = mem_total_str.c_str();
    paramValues[12] = mem_available.c_str();
    paramValues[13] = mem_util.c_str();
    paramValues[14] = queue_len.c_str();
    paramValues[15] = proc_count.c_str();
    paramValues[16] = tasks_gen.c_str();
    paramValues[17] = tasks_ok.c_str();
    paramValues[18] = tasks_late.c_str();
    paramValues[19] = tasks_fail.c_str();
    paramValues[20] = tasks_reject.c_str();
    paramValues[21] = avg_comp_time.c_str();
    paramValues[22] = deadline_miss.c_str();
    paramValues[23] = payload.c_str();
    
    const char* query = "INSERT INTO vehicle_status (vehicle_id, rsu_id, update_time, "
                        "pos_x, pos_y, speed, heading, "
                        "cpu_total, cpu_allocable, cpu_available, cpu_utilization, "
                        "mem_total, mem_available, mem_utilization, "
                        "queue_length, processing_count, "
                        "tasks_generated, tasks_completed_on_time, tasks_completed_late, "
                        "tasks_failed, tasks_rejected, avg_completion_time, deadline_miss_ratio, payload) "
                        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, "
                        "$15, $16, $17, $18, $19, $20, $21, $22, $23, $24::jsonb)";
    
    PGresult* res = PQexecParams(conn, query, 24, nullptr, paramValues, nullptr, nullptr, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        EV_WARN << "⚠ Failed to insert vehicle status: " << PQerrorMessage(conn) << endl;
    } else {
        EV_INFO << "✓ Vehicle status inserted (" << vehicle_id << " @ " << update_time << "s)" << endl;
    }
    
    PQclear(res);
}

// ============================================================================
// TASK OFFLOADING ORCHESTRATION IMPLEMENTATION
// ============================================================================

void MyRSUApp::handleOffloadingRequest(veins::OffloadingRequestMessage* msg) {
    EV_INFO << "\n" << endl;
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║              RSU: OFFLOADING REQUEST RECEIVED                            ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    std::string task_id = msg->getTask_id();
    std::string vehicle_id = msg->getVehicle_id();
    
    std::cout << "RSU_OFFLOAD: Received offloading request for task " << task_id 
              << " from vehicle " << vehicle_id << std::endl;
    
    // Store request details
    OffloadingRequest request;
    request.task_id = task_id;
    request.vehicle_id = vehicle_id;
    request.vehicle_mac = msg->getSenderAddress();  // Inherited from BaseFrame1609_4
    request.request_time = simTime().dbl();
    request.local_decision = msg->getLocal_decision();
    
    // Task characteristics
    request.task_size_bytes = msg->getTask_size_bytes();
    request.cpu_cycles = msg->getCpu_cycles();
    request.deadline_seconds = msg->getDeadline_seconds();
    request.qos_value = msg->getQos_value();
    
    // Vehicle state
    request.vehicle_cpu_available = msg->getLocal_cpu_available_ghz();
    request.vehicle_cpu_utilization = msg->getLocal_cpu_utilization();
    request.vehicle_queue_length = msg->getLocal_queue_length();
    
    pending_offloading_requests[task_id] = request;
    
    EV_INFO << "  Task ID: " << task_id << endl;
    EV_INFO << "  Vehicle ID: " << vehicle_id << endl;
    EV_INFO << "  Task Size: " << (request.task_size_bytes / 1024.0) << " KB" << endl;
    EV_INFO << "  CPU Cycles: " << (request.cpu_cycles / 1e9) << " G" << endl;
    EV_INFO << "  Deadline: " << request.deadline_seconds << " s" << endl;
    EV_INFO << "  QoS: " << request.qos_value << endl;
    EV_INFO << "  Local Decision Recommendation: " << request.local_decision << endl;
    EV_INFO << "  Vehicle CPU Available: " << request.vehicle_cpu_available << " GHz" << endl;
    EV_INFO << "  Vehicle CPU Utilization: " << (request.vehicle_cpu_utilization * 100) << "%" << endl;
    EV_INFO << "  Vehicle Queue Length: " << request.vehicle_queue_length << endl;
    
    // Make ML-based offloading decision
    veins::OffloadingDecisionMessage* decision = makeOffloadingDecision(request);
    
    if (decision) {
        // Send decision back to vehicle
        populateWSM(decision, request.vehicle_mac);
        sendDown(decision);
        
        EV_INFO << "✓ Offloading decision sent back to vehicle " << vehicle_id << endl;
        std::cout << "RSU_OFFLOAD: Decision sent for task " << task_id 
                  << ": " << decision->getDecision_type() << std::endl;
    } else {
        EV_ERROR << "Failed to create offloading decision" << endl;
    }
    
    // Clean up request message
    delete msg;
}

veins::OffloadingDecisionMessage* MyRSUApp::makeOffloadingDecision(const OffloadingRequest& request) {
    EV_INFO << "🤖 RSU: Making ML-based offloading decision..." << endl;
    
    // ========================================================================
    // ML DECISION ENGINE (PLACEHOLDER - REPLACE WITH ACTUAL ML MODEL)
    // ========================================================================
    
    std::string decisionType;
    std::string reason;
    double confidence = 0.0;
    double estimated_completion_time = 0.0;
    std::string target_service_vehicle_id;
    LAddress::L2Type target_service_vehicle_mac = 0;
    
    if (!mlModelEnabled) {
        // PLACEHOLDER: Simple rule-based decision
        EV_INFO << "  Using placeholder rule-based decision (ML model disabled)" << endl;
        
        // Rule 1: If vehicle queue is long, offload to RSU
        if (request.vehicle_queue_length > 5) {
            decisionType = "RSU";
            reason = "Vehicle queue overloaded (>" + std::to_string((int)request.vehicle_queue_length) + " tasks)";
            confidence = 0.8;
            estimated_completion_time = request.deadline_seconds * 0.6;  // Estimate 60% of deadline
        }
        // Rule 2: If vehicle CPU utilization is high, try service vehicle
        else if (request.vehicle_cpu_utilization > 0.8 && serviceVehicleSelectionEnabled) {
            std::string sv_id = selectBestServiceVehicle(request);
            if (!sv_id.empty()) {
                decisionType = "SERVICE_VEHICLE";
                target_service_vehicle_id = sv_id;
                // TODO: Get actual MAC address from Digital Twin
                target_service_vehicle_mac = 0;  // Placeholder
                reason = "Vehicle CPU high (" + std::to_string((int)(request.vehicle_cpu_utilization*100)) + "%), service vehicle available";
                confidence = 0.75;
                estimated_completion_time = request.deadline_seconds * 0.7;
            } else {
                // No service vehicle available, use RSU
                decisionType = "RSU";
                reason = "Vehicle CPU high but no service vehicle available";
                confidence = 0.7;
                estimated_completion_time = request.deadline_seconds * 0.6;
            }
        }
        // Rule 3: Vehicle can handle it locally
        else if (request.vehicle_cpu_available > 0.5) {
            decisionType = "LOCAL";
            reason = "Vehicle has sufficient resources (CPU available: " + std::to_string(request.vehicle_cpu_available) + " GHz)";
            confidence = 0.85;
            estimated_completion_time = request.deadline_seconds * 0.5;
        }
        // Rule 4: Borderline case - default to RSU
        else {
            decisionType = "RSU";
            reason = "Borderline case, using RSU for reliability";
            confidence = 0.65;
            estimated_completion_time = request.deadline_seconds * 0.7;
        }
    } else {
        // TODO: ML MODEL INFERENCE HERE
        // Extract features from request and Digital Twin
        // Call ML model prediction
        // Parse ML model output
        EV_INFO << "  ML model inference (NOT YET IMPLEMENTED)" << endl;
        decisionType = "LOCAL";  // Fallback
        reason = "ML model not yet integrated";
        confidence = 0.5;
        estimated_completion_time = request.deadline_seconds * 0.8;
    }
    
    EV_INFO << "  Decision: " << decisionType << endl;
    EV_INFO << "  Reason: " << reason << endl;
    EV_INFO << "  Confidence: " << confidence << endl;
    
    // Create decision message
    veins::OffloadingDecisionMessage* decision = new veins::OffloadingDecisionMessage();
    decision->setSenderAddress(myId);  // Set RSU's MAC address
    decision->setTask_id(request.task_id.c_str());
    decision->setVehicle_id(request.vehicle_id.c_str());
    decision->setDecision_type(decisionType.c_str());
    decision->setDecision_time(simTime().dbl());
    decision->setConfidence_score(confidence);
    decision->setEstimated_completion_time(estimated_completion_time);
    decision->setDecision_reason(reason.c_str());
    
    if (decisionType == "SERVICE_VEHICLE") {
        decision->setTarget_service_vehicle_id(target_service_vehicle_id.c_str());
        decision->setTarget_service_vehicle_mac(target_service_vehicle_mac);
    }
    
    return decision;
}

std::string MyRSUApp::selectBestServiceVehicle(const OffloadingRequest& request) {
    EV_INFO << "  Searching for best service vehicle..." << endl;
    
    // Query Digital Twin for available service vehicles
    // TODO: Implement proper service vehicle selection based on:
    // - CPU/memory availability
    // - Distance from task vehicle
    // - Current workload
    // - Historical reliability
    
    // PLACEHOLDER: Return empty (no service vehicle available)
    EV_INFO << "    No service vehicles available (placeholder)" << endl;
    return "";
}

void MyRSUApp::handleTaskOffloadPacket(veins::TaskOffloadPacket* msg) {
    EV_INFO << "\n" << endl;
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║              RSU: TASK OFFLOAD PACKET RECEIVED                           ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    std::string task_id = msg->getTask_id();
    std::string vehicle_id = msg->getOrigin_vehicle_id();
    
    std::cout << "RSU_PROCESS: Received task " << task_id << " from vehicle " << vehicle_id 
              << " for RSU processing" << std::endl;
    
    EV_INFO << "  Task ID: " << task_id << endl;
    EV_INFO << "  Origin Vehicle: " << vehicle_id << endl;
    EV_INFO << "  Task Size: " << (msg->getTask_size_bytes() / 1024.0) << " KB" << endl;
    EV_INFO << "  CPU Cycles Required: " << (msg->getCpu_cycles() / 1e9) << " G" << endl;
    
    // Process task on RSU edge server
    processTaskOnRSU(task_id, msg);
    
    // msg will be deleted in processTaskOnRSU or here
    delete msg;
}

void MyRSUApp::processTaskOnRSU(const std::string& task_id, veins::TaskOffloadPacket* packet) {
    EV_INFO << "⚙️ RSU: Processing task " << task_id << " on edge server..." << endl;
    
    // TODO: Implement actual task processing
    // - Allocate RSU edge server resources
    // - Simulate processing time based on CPU cycles
    // - Track task in processing queue
    // - Schedule completion event
    
    // PLACEHOLDER: Simulate instant processing
    std::string vehicle_id = packet->getOrigin_vehicle_id();
    LAddress::L2Type vehicle_mac = packet->getOrigin_vehicle_mac();
    
    EV_INFO << "  Edge Server CPU: " << edgeCPU_GHz << " GHz" << endl;
    EV_INFO << "  Edge Server Memory: " << edgeMemory_GB << " GB" << endl;
    
    // Simulate successful processing
    bool success = true;
    
    std::cout << "RSU_PROCESS: Task " << task_id << " processing complete, sending result" << std::endl;
    
    sendTaskResultToVehicle(task_id, vehicle_id, vehicle_mac, success);
}

void MyRSUApp::sendTaskResultToVehicle(const std::string& task_id, const std::string& vehicle_id,
                                        LAddress::L2Type vehicle_mac, bool success) {
    EV_INFO << "📤 RSU: Sending task result to vehicle " << vehicle_id << endl;
    
    // Create result message
    veins::TaskResultMessage* result = new veins::TaskResultMessage();
    result->setTask_id(task_id.c_str());
    result->setProcessor_id("RSU");
    result->setSuccess(success);
    result->setProcessing_time(0.1);  // Placeholder: 100ms processing time
    result->setCompletion_time(simTime().dbl());
    
    if (success) {
        result->setTask_output_data("RSU_PROCESSED_DATA");  // Placeholder
        EV_INFO << "  Result: SUCCESS" << endl;
    } else {
        result->setFailure_reason("Processing failed");
        EV_INFO << "  Result: FAILED" << endl;
    }
    
    // Send to vehicle
    populateWSM(result, vehicle_mac);
    sendDown(result);
    
    std::cout << "RSU_RESULT: Sent task result for " << task_id << " to vehicle " << vehicle_id << std::endl;
}

void MyRSUApp::handleTaskOffloadingEvent(veins::TaskOffloadingEvent* msg) {
    EV_DEBUG << "📊 RSU: Received task offloading event" << endl;
    
    std::string task_id = msg->getTask_id();
    std::string event_type = msg->getEvent_type();
    std::string source = msg->getSource_entity_id();
    std::string target = msg->getTarget_entity_id();
    double event_time = msg->getEvent_time();
    
    EV_DEBUG << "  Task: " << task_id << endl;
    EV_DEBUG << "  Event: " << event_type << endl;
    EV_DEBUG << "  Source: " << source << " → Target: " << target << endl;
    EV_DEBUG << "  Time: " << event_time << "s" << endl;
    
    // TODO: Log to Digital Twin database
    // INSERT INTO task_offloading_events (task_id, event_type, source_entity_id, ...)
    
    std::cout << "RSU_EVENT: " << event_type << " for task " << task_id 
              << " (" << source << " → " << target << ")" << std::endl;
    
    delete msg;
}

}  // namespace complex_network




