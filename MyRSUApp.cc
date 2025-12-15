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
        
        // Initialize RSU resource tracking (dynamic state)
        rsu_cpu_total = edgeCPU_GHz;
        rsu_cpu_available = edgeCPU_GHz;  // Initially all available
        rsu_memory_total = edgeMemory_GB;
        rsu_memory_available = edgeMemory_GB;
        rsu_max_concurrent = maxVehicles;  // Use maxVehicles as max concurrent tasks
        
        // Initialize PostgreSQL database connection
        initDatabase();
        
        // Insert RSU metadata (static info) once at initialization
        insertRSUMetadata();
        
        // Start periodic RSU status updates
        rsu_status_update_timer = new cMessage("rsuStatusUpdate");
        scheduleAt(simTime() + rsu_status_update_interval, rsu_status_update_timer);
        
        // Initialize decision checker
        checkDecisionMsg = new cMessage("checkDecision");
        
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
    if (msg == beaconMsg && strcmp(msg->getName(), "sendMessage") == 0) {
        DemoSafetyMessage* wsm = new DemoSafetyMessage();
        populateWSM(wsm);
        wsm->setSenderPos(curPosition);
        wsm->setUserPriority(par("beaconUserPriority").intValue());
        sendDown(wsm);

        EV << "RSU: Sent beacon at time " << simTime() << endl;

        double interval = par("beaconInterval").doubleValue();
        scheduleAt(simTime() + interval, msg);
    } 
    else if (msg == rsu_status_update_timer) {
        // Handle RSU status update
        sendRSUStatusUpdate();
    } 
    else if (msg == checkDecisionMsg) {
        // Poll DB for pending decisions from ML model
        PGconn* conn = getDBConnection();
        if (conn) {
            int decisions_found = 0;
            int decisions_sent = 0;
            
            for (auto& pair : task_records) {
                TaskRecord& record = pair.second;
                
                // Only check for tasks that:
                // 1. Are not completed/failed
                // 2. Haven't had a decision sent yet
                if (!record.completed && !record.failed && !record.decision_sent) {
                    // Query database for decision made by ML model
                    std::string query = "SELECT decision_type, target_service_vehicle_id, confidence_score "
                                      "FROM offloading_decisions WHERE task_id = '" + record.task_id + "' LIMIT 1";
                    PGresult* res = PQexec(conn, query.c_str());
                    
                    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
                        decisions_found++;
                        
                        std::string decision_type = PQgetvalue(res, 0, 0);
                        std::string target_id = PQgetvalue(res, 0, 1);
                        std::string confidence_str = PQgetvalue(res, 0, 2);
                        double confidence = std::stod(confidence_str);
                        
                        EV_INFO << "✓ Found ML decision for task " << record.task_id << ": " << decision_type << endl;
                        std::cout << "ML_DECISION: Task " << record.task_id << " -> " << decision_type 
                                  << " (confidence: " << confidence << ")" << std::endl;
                        
                        // Create decision message
                        OffloadingDecisionMessage* dMsg = new OffloadingDecisionMessage();
                        dMsg->setTask_id(record.task_id.c_str());
                        dMsg->setVehicle_id(record.vehicle_id.c_str());
                        dMsg->setDecision_type(decision_type.c_str());
                        dMsg->setTarget_service_vehicle_id(target_id.c_str());
                        dMsg->setConfidence_score(confidence);
                        dMsg->setDecision_time(simTime().dbl());
                        dMsg->setSenderAddress(myId);  // RSU MAC address
                        
                        // Set target service vehicle MAC if applicable
                        if (decision_type == "SERVICE_VEHICLE" && !target_id.empty()) {
                            if (vehicle_macs.count(target_id)) {
                                dMsg->setTarget_service_vehicle_mac(vehicle_macs[target_id]);
                                EV_INFO << "  → Target SV: " << target_id << endl;
                            } else {
                                EV_WARN << "  ⚠ Service vehicle " << target_id << " MAC not found" << endl;
                            }
                        }
                        
                        // Send decision to requesting vehicle
                        if (vehicle_macs.count(record.vehicle_id)) {
                            dMsg->setRecipientAddress(vehicle_macs[record.vehicle_id]);
                            sendDown(dMsg);
                            
                            record.decision_sent = true;
                            decisions_sent++;
                            
                            EV_INFO << "✓ Sent ML decision to vehicle " << record.vehicle_id << endl;
                            std::cout << "RSU_SEND: Decision sent to " << record.vehicle_id 
                                      << " for task " << record.task_id << std::endl;
                        } else {
                            EV_WARN << "⚠ Vehicle " << record.vehicle_id << " MAC not found, cannot send decision" << endl;
                            std::cout << "RSU_WARN: Cannot send decision - vehicle MAC not found: " 
                                      << record.vehicle_id << std::endl;
                            delete dMsg;
                        }
                    }
                    PQclear(res);
                }
            }
            
            if (decisions_found > 0) {
                EV_INFO << "📊 Decision poll: " << decisions_found << " found, " 
                        << decisions_sent << " sent" << endl;
            }
        } else {
            EV_WARN << "⚠ Cannot poll decisions: No database connection" << endl;
        }
        
        // Reschedule next check
        scheduleAt(simTime() + 0.1, checkDecisionMsg);
    }
    else {
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
        std::cout << "RSU_MSG: Received VehicleResourceStatusMessage from vehicle " 
                  << resourceStatus->getVehicle_id() << std::endl;
        handleVehicleResourceStatus(resourceStatus);
        return;
    }
    
    // Handle TaskResultMessage with completion timing data
    TaskResultMessage* taskResult = dynamic_cast<TaskResultMessage*>(wsm);
    if (taskResult) {
        EV_INFO << "📥 RSU received TaskResultMessage with completion data" << endl;
        handleTaskResultWithCompletion(taskResult);
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
    
    // Cancel and delete RSU status update timer
    if (rsu_status_update_timer) {
        cancelAndDelete(rsu_status_update_timer);
        rsu_status_update_timer = nullptr;
    }
    
    // Cancel and delete decision checker timer
    if (checkDecisionMsg) {
        cancelAndDelete(checkDecisionMsg);
        checkDecisionMsg = nullptr;
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

    // Schedule a check for decision
    if (!checkDecisionMsg) {
        checkDecisionMsg = new cMessage("checkDecision");
    }
    // Schedule if not already scheduled (or schedule a new one for this task specifically?)
    // Better to have a periodic checker or one per task. 
    // For simplicity, let's just ensure the checker is running.
    if (!checkDecisionMsg->isScheduled()) {
        scheduleAt(simTime() + 0.1, checkDecisionMsg);
    }
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
    twin.heading = msg->getHeading();
    
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
    
    // Insert into PostgreSQL database (real-time status)
    insertVehicleStatus(msg);
    
    // Insert/update vehicle metadata (first time or periodic update)
    insertVehicleMetadata(vehicle_id);
    
    // Note: MAC address will be stored when OffloadingRequestMessage arrives
    // (OffloadingRequestMessage has getSenderAddress() inherited from BaseFrame1609_4)
    
    EV_INFO << "  Position: (" << twin.pos_x << ", " << twin.pos_y << ")" << endl;
    EV_INFO << "  CPU Utilization: " << (twin.cpu_utilization * 100.0) << "%" << endl;
    EV_INFO << "  Memory Utilization: " << (twin.mem_utilization * 100.0) << "%" << endl;
    EV_INFO << "  Queue Length: " << twin.current_queue_length << endl;
    EV_INFO << "  Processing Count: " << twin.current_processing_count << endl;
    
    std::cout << "DT_UPDATE: Vehicle " << vehicle_id << " - CPU:" << (twin.cpu_utilization * 100.0) 
              << "% Mem:" << (twin.mem_utilization * 100.0) << "% Queue:" << twin.current_queue_length << std::endl;
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
    // Try to get database connection string from environment variable
    const char* db_env = std::getenv("DATABASE_URL");
    
    if (db_env) {
        db_conninfo = std::string(db_env);
        EV_INFO << "Using DATABASE_URL from environment variable" << endl;
    } else {
        // Try to read from .env file
        EV_INFO << "DATABASE_URL not in environment, attempting to read from .env file..." << endl;
        std::ifstream envFile(".env");
        bool found = false;
        
        if (envFile.is_open()) {
            std::string line;
            while (std::getline(envFile, line)) {
                // Skip comments and empty lines
                if (line.empty() || line[0] == '#') continue;
                
                // Look for DATABASE_URL=
                if (line.find("DATABASE_URL=") == 0) {
                    db_conninfo = line.substr(13); // Skip "DATABASE_URL="
                    found = true;
                    EV_INFO << "Found DATABASE_URL in .env file" << endl;
                    break;
                }
            }
            envFile.close();
        }
        
        if (!found) {
            EV_ERROR << "ERROR: DATABASE_URL not found in environment or .env file!" << endl;
            EV_ERROR << "Please configure DATABASE_URL in your .env file" << endl;
            throw cRuntimeError("Missing DATABASE_URL configuration");
        }
    }
    
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║              INITIALIZING POSTGRESQL CONNECTION                          ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    EV_INFO << "Database: " << db_conninfo << endl;
    
    // Add connection timeout to prevent hanging
    std::string conninfo_with_timeout = db_conninfo + " connect_timeout=5";
    db_conn = PQconnectdb(conninfo_with_timeout.c_str());
    
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
        // Add connection timeout to prevent hanging
        std::string conninfo_with_timeout = db_conninfo + " connect_timeout=5";
        db_conn = PQconnectdb(conninfo_with_timeout.c_str());
        
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
        std::cout << "DB_WARN: No database connection, skipping vehicle_status insert for " 
                  << msg->getVehicle_id() << std::endl;
        return;
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
    std::string acceleration = std::to_string(msg->getAcceleration());
    std::string heading = std::to_string(msg->getHeading());
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
    paramValues[6] = acceleration.c_str();
    paramValues[7] = heading.c_str();
    paramValues[8] = cpu_total_str.c_str();
    paramValues[9] = cpu_allocable.c_str();
    paramValues[10] = cpu_available.c_str();
    paramValues[11] = cpu_util.c_str();
    paramValues[12] = mem_total_str.c_str();
    paramValues[13] = mem_available.c_str();
    paramValues[14] = mem_util.c_str();
    paramValues[15] = queue_len.c_str();
    paramValues[16] = proc_count.c_str();
    paramValues[17] = tasks_gen.c_str();
    paramValues[18] = tasks_ok.c_str();
    paramValues[19] = tasks_late.c_str();
    paramValues[20] = tasks_fail.c_str();
    paramValues[21] = tasks_reject.c_str();
    paramValues[22] = avg_comp_time.c_str();
    paramValues[23] = deadline_miss.c_str();
    paramValues[24] = payload.c_str();
    
    const char* query = "INSERT INTO vehicle_status (vehicle_id, rsu_id, update_time, "
                        "pos_x, pos_y, speed, acceleration, heading, "
                        "cpu_total, cpu_allocable, cpu_available, cpu_utilization, "
                        "mem_total, mem_available, mem_utilization, "
                        "queue_length, processing_count, "
                        "tasks_generated, tasks_completed_on_time, tasks_completed_late, "
                        "tasks_failed, tasks_rejected, avg_completion_time, deadline_miss_ratio, payload) "
                        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, "
                        "$16, $17, $18, $19, $20, $21, $22, $23, $24, $25::jsonb)";
    
    PGresult* res = PQexecParams(conn, query, 25, nullptr, paramValues, nullptr, nullptr, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        EV_WARN << "⚠ Failed to insert vehicle status: " << PQerrorMessage(conn) << endl;
        std::cout << "DB_ERROR: Failed to insert vehicle_status for " << vehicle_id 
                  << ": " << PQerrorMessage(conn) << std::endl;
    } else {
        EV_INFO << "✓ Vehicle status inserted (" << vehicle_id << " @ " << update_time << "s)" << endl;
        std::cout << "DB_INSERT: vehicle_status for " << vehicle_id 
                  << " @ t=" << update_time << "s" << std::endl;
    }
    
    PQclear(res);
}

// ============================================================================
// OFFLOADING DATABASE INSERTIONS
// ============================================================================

void MyRSUApp::insertOffloadingRequest(const OffloadingRequest& request) {
    PGconn* conn = getDBConnection();
    if (!conn) {
        EV_WARN << "⚠ Cannot insert offloading request: No database connection" << endl;
        std::cout << "WARN: ⚠ Cannot insert offloading request: No database connection" << std::endl;
        return;
    }
    
    EV_INFO << "📤 Inserting offloading request into database for task " << request.task_id << endl;
    std::cout << "INFO: 📤 Inserting offloading request into database for task " << request.task_id << std::endl;
    
    // Prepare JSON payload
    std::ostringstream payload_json;
    payload_json << "{"
                 << "\"task_id\":\"" << request.task_id << "\","
                 << "\"vehicle_id\":\"" << request.vehicle_id << "\","
                 << "\"local_decision\":\"" << request.local_decision << "\","
                 << "\"task_size_bytes\":" << request.task_size_bytes << ","
                 << "\"cpu_cycles\":" << request.cpu_cycles << ","
                 << "\"deadline_seconds\":" << request.deadline_seconds << ","
                 << "\"qos_value\":" << request.qos_value
                 << "}";
    
    const char* paramValues[18];
    std::string task_id = request.task_id;
    std::string vehicle_id = request.vehicle_id;
    std::string rsu_id_str = std::to_string(rsu_id);
    std::string request_time = std::to_string(request.request_time);
    std::string task_size = std::to_string(request.task_size_bytes);
    std::string cpu_cycles = std::to_string(request.cpu_cycles);
    std::string deadline = std::to_string(request.deadline_seconds);
    std::string qos = std::to_string(request.qos_value);
    std::string cpu_avail = std::to_string(request.vehicle_cpu_available);
    std::string cpu_util = std::to_string(request.vehicle_cpu_utilization);
    std::string mem_avail = std::to_string(request.vehicle_mem_available);
    std::string queue_len = std::to_string(request.vehicle_queue_length);
    std::string proc_count = std::to_string(request.vehicle_processing_count);
    std::string pos_x = std::to_string(request.pos_x);
    std::string pos_y = std::to_string(request.pos_y);
    std::string spd = std::to_string(request.speed);
    std::string local_dec = request.local_decision;
    std::string payload = payload_json.str();
    
    paramValues[0] = task_id.c_str();
    paramValues[1] = vehicle_id.c_str();
    paramValues[2] = rsu_id_str.c_str();
    paramValues[3] = request_time.c_str();
    paramValues[4] = task_size.c_str();
    paramValues[5] = cpu_cycles.c_str();
    paramValues[6] = deadline.c_str();
    paramValues[7] = qos.c_str();
    paramValues[8] = cpu_avail.c_str();
    paramValues[9] = cpu_util.c_str();
    paramValues[10] = mem_avail.c_str();
    paramValues[11] = queue_len.c_str();
    paramValues[12] = proc_count.c_str();
    paramValues[13] = pos_x.c_str();
    paramValues[14] = pos_y.c_str();
    paramValues[15] = spd.c_str();
    paramValues[16] = local_dec.c_str();
    paramValues[17] = payload.c_str();
    
    const char* query = "INSERT INTO offloading_requests (task_id, vehicle_id, rsu_id, request_time, "
                        "task_size_bytes, cpu_cycles, deadline_seconds, qos_value, "
                        "vehicle_cpu_available, vehicle_cpu_utilization, vehicle_mem_available, "
                        "vehicle_queue_length, vehicle_processing_count, "
                        "pos_x, pos_y, speed, local_decision, payload) "
                        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18::jsonb)";
    
    PGresult* res = PQexecParams(conn, query, 18, nullptr, paramValues, nullptr, nullptr, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        EV_WARN << "✗ Failed to insert offloading request: " << PQerrorMessage(conn) << endl;
        std::cout << "WARN: ✗ Failed to insert offloading request: " << PQerrorMessage(conn) << std::endl;
    } else {
        EV_INFO << "✓ Offloading request inserted successfully (Task: " << request.task_id << ")" << endl;
        std::cout << "INFO: ✓ Offloading request inserted successfully (Task: " << request.task_id << ")" << std::endl;
    }
    
    PQclear(res);
}

void MyRSUApp::insertOffloadingDecision(const std::string& task_id, const veins::OffloadingDecisionMessage* decision) {
    PGconn* conn = getDBConnection();
    if (!conn) {
        EV_WARN << "⚠ Cannot insert offloading decision: No database connection" << endl;
        return;
    }
    
    EV_DEBUG << "📤 Inserting offloading decision into PostgreSQL..." << endl;
    
    // Prepare JSON payload
    std::ostringstream payload_json;
    payload_json << "{"
                 << "\"task_id\":\"" << decision->getTask_id() << "\","
                 << "\"decision_type\":\"" << decision->getDecision_type() << "\","
                 << "\"confidence_score\":" << decision->getConfidence_score() << ","
                 << "\"estimated_completion_time\":" << decision->getEstimated_completion_time() << ","
                 << "\"decision_reason\":\"" << decision->getDecision_reason() << "\""
                 << "}";
    
    const char* paramValues[10];
    std::string task_id_str = task_id;
    std::string vehicle_id = decision->getVehicle_id();
    std::string rsu_id_str = std::to_string(rsu_id);
    std::string decision_time = std::to_string(decision->getDecision_time());
    std::string decision_type = decision->getDecision_type();
    std::string target_sv = decision->getTarget_service_vehicle_id();
    std::string confidence = std::to_string(decision->getConfidence_score());
    std::string est_time = std::to_string(decision->getEstimated_completion_time());
    std::string reason = decision->getDecision_reason();
    std::string payload = payload_json.str();
    
    paramValues[0] = task_id_str.c_str();
    paramValues[1] = vehicle_id.c_str();
    paramValues[2] = rsu_id_str.c_str();
    paramValues[3] = decision_time.c_str();
    paramValues[4] = decision_type.c_str();
    paramValues[5] = target_sv.c_str();
    paramValues[6] = confidence.c_str();
    paramValues[7] = est_time.c_str();
    paramValues[8] = reason.c_str();
    paramValues[9] = payload.c_str();
    
    const char* query = "INSERT INTO offloading_decisions (task_id, vehicle_id, rsu_id, decision_time, "
                        "decision_type, target_service_vehicle_id, confidence_score, "
                        "estimated_completion_time, decision_reason, payload) "
                        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10::jsonb)";
    
    PGresult* res = PQexecParams(conn, query, 10, nullptr, paramValues, nullptr, nullptr, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        EV_WARN << "✗ Failed to insert offloading decision: " << PQerrorMessage(conn) << endl;
    } else {
        EV_DEBUG << "✓ Offloading decision inserted (Task: " << task_id << ")" << endl;
    }
    
    PQclear(res);
}

void MyRSUApp::insertTaskOffloadingEvent(const veins::TaskOffloadingEvent* event) {
    PGconn* conn = getDBConnection();
    if (!conn) {
        EV_WARN << "⚠ Cannot insert task offloading event: No database connection" << endl;
        return;
    }
    
    EV_DEBUG << "📤 Inserting task offloading event into PostgreSQL..." << endl;
    
    const char* paramValues[7];
    std::string task_id = event->getTask_id();
    std::string event_type = event->getEvent_type();
    std::string event_time = std::to_string(event->getEvent_time());
    std::string source_entity = event->getSource_entity_id();
    std::string target_entity = event->getTarget_entity_id();
    std::string rsu_id_str = std::to_string(rsu_id);
    std::string event_details = event->getEvent_details();
    
    paramValues[0] = task_id.c_str();
    paramValues[1] = event_type.c_str();
    paramValues[2] = event_time.c_str();
    paramValues[3] = source_entity.c_str();
    paramValues[4] = target_entity.c_str();
    paramValues[5] = rsu_id_str.c_str();
    paramValues[6] = event_details.c_str();
    
    const char* query = "INSERT INTO task_offloading_events (task_id, event_type, event_time, "
                        "source_entity_id, target_entity_id, rsu_id, event_details) "
                        "VALUES ($1, $2, $3, $4, $5, $6, $7::jsonb)";
    
    PGresult* res = PQexecParams(conn, query, 7, nullptr, paramValues, nullptr, nullptr, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        EV_WARN << "✗ Failed to insert task offloading event: " << PQerrorMessage(conn) << endl;
    } else {
        EV_DEBUG << "✓ Task offloading event inserted (Task: " << task_id << ", Type: " << event_type << ")" << endl;
    }
    
    PQclear(res);
}

void MyRSUApp::insertOffloadedTaskCompletion(const std::string& task_id, const std::string& vehicle_id,
                                             const std::string& decision_type, const std::string& processor_id,
                                             double request_time, double decision_time, double start_time,
                                             double completion_time, bool success, bool completed_on_time,
                                             double deadline_seconds, uint64_t task_size_bytes, uint64_t cpu_cycles,
                                             double qos_value, const std::string& result_data, const std::string& failure_reason) {
    PGconn* conn = getDBConnection();
    if (!conn) {
        EV_WARN << "⚠ Cannot insert task completion: No database connection" << endl;
        std::cout << "WARN: ⚠ Cannot insert task completion: No database connection" << std::endl;
        return;
    }
    
    EV_INFO << "📊 Inserting offloaded task completion for task " << task_id << endl;
    std::cout << "INFO: 📊 Inserting offloaded task completion for task " << task_id << std::endl;
    
    // Calculate latencies
    double decision_latency = decision_time - request_time;
    double processing_latency = completion_time - start_time;
    double total_latency = completion_time - request_time;
    
    std::cout << "INFO: 📈 Task Metrics - Decision: " << decision_latency << "s, Processing: " 
              << processing_latency << "s, Total: " << total_latency << "s" << std::endl;
    
    const char* paramValues[19];
    std::string tid = task_id;
    std::string vid = vehicle_id;
    std::string rsu_id_str = std::to_string(rsu_id);
    std::string dec_type = decision_type;
    std::string proc_id = processor_id;
    std::string req_time = std::to_string(request_time);
    std::string dec_time = std::to_string(decision_time);
    std::string strt_time = std::to_string(start_time);
    std::string comp_time = std::to_string(completion_time);
    std::string dec_lat = std::to_string(decision_latency);
    std::string proc_lat = std::to_string(processing_latency);
    std::string tot_lat = std::to_string(total_latency);
    std::string succ = success ? "true" : "false";
    std::string comp_on_time = completed_on_time ? "true" : "false";
    std::string deadline = std::to_string(deadline_seconds);
    std::string tsize = std::to_string(task_size_bytes);
    std::string cycles = std::to_string(cpu_cycles);
    std::string qos = std::to_string(qos_value);
    std::string result = result_data;
    
    paramValues[0] = tid.c_str();
    paramValues[1] = vid.c_str();
    paramValues[2] = rsu_id_str.c_str();
    paramValues[3] = dec_type.c_str();
    paramValues[4] = proc_id.c_str();
    paramValues[5] = req_time.c_str();
    paramValues[6] = dec_time.c_str();
    paramValues[7] = strt_time.c_str();
    paramValues[8] = comp_time.c_str();
    paramValues[9] = dec_lat.c_str();
    paramValues[10] = proc_lat.c_str();
    paramValues[11] = tot_lat.c_str();
    paramValues[12] = succ.c_str();
    paramValues[13] = comp_on_time.c_str();
    paramValues[14] = deadline.c_str();
    paramValues[15] = tsize.c_str();
    paramValues[16] = cycles.c_str();
    paramValues[17] = qos.c_str();
    paramValues[18] = result.c_str();
    
    const char* query = "INSERT INTO offloaded_task_completions (task_id, vehicle_id, rsu_id, "
                        "decision_type, processor_id, request_time, decision_time, start_time, completion_time, "
                        "decision_latency, processing_latency, total_latency, success, completed_on_time, "
                        "deadline_seconds, task_size_bytes, cpu_cycles, qos_value, result_data) "
                        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13::boolean, $14::boolean, "
                        "$15, $16, $17, $18, $19)";
    
    PGresult* res = PQexecParams(conn, query, 19, nullptr, paramValues, nullptr, nullptr, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        EV_WARN << "✗ Failed to insert task completion: " << PQerrorMessage(conn) << endl;
        std::cout << "WARN: ✗ Failed to insert task completion: " << PQerrorMessage(conn) << std::endl;
    } else {
        EV_INFO << "✓ Task completion inserted successfully" << endl;
        std::cout << "INFO: ✓ Task completion inserted - Total Latency: " << total_latency 
                  << "s, Success: " << success << ", On-time: " << completed_on_time << std::endl;
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
    
    std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << "INFO: RSU RECEIVED OFFLOADING REQUEST" << std::endl;
    std::cout << "INFO: Task ID: " << task_id << std::endl;
    std::cout << "INFO: Vehicle ID: " << vehicle_id << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n" << std::endl;
    
    // Store request details
    OffloadingRequest request;
    request.task_id = task_id;
    request.vehicle_id = vehicle_id;
    request.vehicle_mac = msg->getSenderAddress();  // Inherited from BaseFrame1609_4
    request.request_time = simTime().dbl();
    request.local_decision = msg->getLocal_decision();
    
    // Store vehicle MAC address for later decision sending
    vehicle_macs[vehicle_id] = request.vehicle_mac;
    EV_DEBUG << "  → Stored MAC address for vehicle " << vehicle_id << endl;
    
    // Task characteristics
    request.task_size_bytes = msg->getTask_size_bytes();
    request.cpu_cycles = msg->getCpu_cycles();
    request.deadline_seconds = msg->getDeadline_seconds();
    request.qos_value = msg->getQos_value();
    
    // Vehicle state
    request.vehicle_cpu_available = msg->getLocal_cpu_available_ghz();
    request.vehicle_cpu_utilization = msg->getLocal_cpu_utilization();
    request.vehicle_mem_available = msg->getLocal_mem_available_mb();
    request.vehicle_queue_length = msg->getLocal_queue_length();
    request.vehicle_processing_count = msg->getLocal_processing_count();
    
    // Vehicle location
    request.pos_x = msg->getPos_x();
    request.pos_y = msg->getPos_y();
    request.speed = msg->getSpeed();
    
    pending_offloading_requests[task_id] = request;
    
    std::cout << "INFO: 📊 Calling insertOffloadingRequest() for task " << task_id << std::endl;
    
    // Store request in Digital Twin database
    insertOffloadingRequest(request);
    
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
        // Store decision in Digital Twin database
        insertOffloadingDecision(task_id, decision);
        
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
    
    // Store event in Digital Twin database
    insertTaskOffloadingEvent(msg);
    
    std::cout << "RSU_EVENT: " << event_type << " for task " << task_id 
              << " (" << source << " → " << target << ")" << std::endl;
    
    delete msg;
}

void MyRSUApp::handleTaskResultWithCompletion(TaskResultMessage* msg) {
    std::string task_id = msg->getTask_id();
    EV_INFO << "📊 RSU received task completion data for " << task_id << endl;
    
    // Parse timing JSON from task_output_data field
    std::string result_data = msg->getTask_output_data();
    
    try {
        // Manual JSON parsing (simple approach)
        double request_time = 0.0, decision_time = 0.0, start_time = 0.0, completion_time = 0.0;
        double qos_value = 0.0;
        uint64_t task_size_bytes = 0, cpu_cycles = 0;
        bool on_time = false;
        std::string decision_type, processor_id, result;
        
        // Extract values from JSON string (simple parsing)
        size_t pos = 0;
        
        // Helper lambda to extract numeric value
        auto extractDouble = [&result_data](const std::string& key) -> double {
            size_t keyPos = result_data.find("\"" + key + "\":");
            if (keyPos == std::string::npos) return 0.0;
            size_t valueStart = result_data.find(":", keyPos) + 1;
            size_t valueEnd = result_data.find_first_of(",}", valueStart);
            std::string valueStr = result_data.substr(valueStart, valueEnd - valueStart);
            return std::stod(valueStr);
        };
        
        auto extractUint64 = [&result_data](const std::string& key) -> uint64_t {
            size_t keyPos = result_data.find("\"" + key + "\":");
            if (keyPos == std::string::npos) return 0;
            size_t valueStart = result_data.find(":", keyPos) + 1;
            size_t valueEnd = result_data.find_first_of(",}", valueStart);
            std::string valueStr = result_data.substr(valueStart, valueEnd - valueStart);
            return std::stoull(valueStr);
        };
        
        auto extractString = [&result_data](const std::string& key) -> std::string {
            size_t keyPos = result_data.find("\"" + key + "\":");
            if (keyPos == std::string::npos) return "";
            size_t valueStart = result_data.find("\"", keyPos + key.length() + 3) + 1;
            size_t valueEnd = result_data.find("\"", valueStart);
            return result_data.substr(valueStart, valueEnd - valueStart);
        };
        
        auto extractBool = [&result_data](const std::string& key) -> bool {
            size_t keyPos = result_data.find("\"" + key + "\":");
            if (keyPos == std::string::npos) return false;
            size_t valueStart = result_data.find(":", keyPos) + 1;
            std::string valueStr = result_data.substr(valueStart, 4);
            return (valueStr.find("true") != std::string::npos);
        };
        
        // Extract all timing values
        request_time = extractDouble("request_time");
        decision_time = extractDouble("decision_time");
        start_time = extractDouble("start_time");
        completion_time = extractDouble("completion_time");
        qos_value = extractDouble("qos_value");
        task_size_bytes = extractUint64("task_size_bytes");
        cpu_cycles = extractUint64("cpu_cycles");
        on_time = extractBool("on_time");
        decision_type = extractString("decision_type");
        processor_id = extractString("processor_id");
        result = extractString("result");
        
        bool success = msg->getSuccess();
        
        EV_INFO << "Parsed completion data:" << endl;
        EV_INFO << "  Request time: " << request_time << "s" << endl;
        EV_INFO << "  Decision time: " << decision_time << "s" << endl;
        EV_INFO << "  Start time: " << start_time << "s" << endl;
        EV_INFO << "  Completion time: " << completion_time << "s" << endl;
        EV_INFO << "  Decision type: " << decision_type << endl;
        EV_INFO << "  Processor: " << processor_id << endl;
        EV_INFO << "  Success: " << (success ? "Yes" : "No") << endl;
        EV_INFO << "  On-time: " << (on_time ? "Yes" : "No") << endl;
        
        // Get vehicle and RSU info
        std::string origin_vehicle_id = msg->getOrigin_vehicle_id();
        
        // Calculate deadline from timing
        double total_latency = completion_time - request_time;
        double deadline_seconds = total_latency * 1.5;  // Estimate deadline (could be extracted from original request)
        
        // Insert into database
        insertOffloadedTaskCompletion(
            task_id,
            origin_vehicle_id,
            decision_type,
            processor_id,
            request_time,
            decision_time,
            start_time,
            completion_time,
            success,
            on_time,
            deadline_seconds,
            task_size_bytes,
            cpu_cycles,
            qos_value,
            result_data,
            msg->getFailure_reason()
        );
        
        std::cout << "COMPLETION_DB: Task " << task_id << " completion data inserted - "
                  << "Total latency: " << total_latency << "s, Success: " << success 
                  << ", On-time: " << on_time << std::endl;
        
    } catch (const std::exception& e) {
        EV_ERROR << "Failed to parse completion data JSON: " << e.what() << endl;
        std::cout << "ERROR: Failed to parse completion data for task " << task_id 
                  << ": " << e.what() << std::endl;
    }
    
    delete msg;
}

// ============================================================================
// RSU STATUS AND METADATA TRACKING
// ============================================================================

void MyRSUApp::sendRSUStatusUpdate() {
    EV_DEBUG << "📊 RSU: Sending status update to database" << endl;
    
    // Update dynamic metrics
    double cpu_util = (rsu_cpu_total > 0) ? ((rsu_cpu_total - rsu_cpu_available) / rsu_cpu_total) : 0.0;
    double mem_util = (rsu_memory_total > 0) ? ((rsu_memory_total - rsu_memory_available) / rsu_memory_total) : 0.0;
    double avg_proc_time = (rsu_tasks_processed > 0) ? (rsu_total_processing_time / rsu_tasks_processed) : 0.0;
    double avg_queue_time = (rsu_tasks_processed > 0) ? (rsu_total_queue_time / rsu_tasks_processed) : 0.0;
    double success_rate = (rsu_tasks_received > 0) ? ((double)rsu_tasks_processed / rsu_tasks_received) : 0.0;
    
    // Insert into database
    insertRSUStatus();
    
    // Reschedule
    scheduleAt(simTime() + rsu_status_update_interval, rsu_status_update_timer);
}

void MyRSUApp::insertRSUStatus() {
    PGconn* conn = getDBConnection();
    if (!conn) return;
    
    // Calculate metrics
    double cpu_util = (rsu_cpu_total > 0) ? ((rsu_cpu_total - rsu_cpu_available) / rsu_cpu_total) : 0.0;
    double mem_util = (rsu_memory_total > 0) ? ((rsu_memory_total - rsu_memory_available) / rsu_memory_total) : 0.0;
    double avg_proc_time = (rsu_tasks_processed > 0) ? (rsu_total_processing_time / rsu_tasks_processed) : 0.0;
    double avg_queue_time = (rsu_tasks_processed > 0) ? (rsu_total_queue_time / rsu_tasks_processed) : 0.0;
    double success_rate = (rsu_tasks_received > 0) ? ((double)rsu_tasks_processed / rsu_tasks_received) : 0.0;
    int connected_vehicles = vehicle_twins.size();
    
    std::string rsu_id_str = "RSU_" + std::to_string(rsu_id);
    double update_time = simTime().dbl();
    
    // Build INSERT query
    const char* paramValues[20];
    std::string params[20];
    
    params[0] = rsu_id_str;
    params[1] = std::to_string(update_time);
    params[2] = std::to_string(rsu_cpu_total);
    params[3] = std::to_string(rsu_cpu_total);  // cpu_allocable = total for RSU
    params[4] = std::to_string(rsu_cpu_available);
    params[5] = std::to_string(cpu_util);
    params[6] = std::to_string(rsu_memory_total);
    params[7] = std::to_string(rsu_memory_available);
    params[8] = std::to_string(mem_util);
    params[9] = std::to_string(rsu_queue_length);
    params[10] = std::to_string(rsu_processing_count);
    params[11] = std::to_string(rsu_max_concurrent);
    params[12] = std::to_string(rsu_tasks_received);
    params[13] = std::to_string(rsu_tasks_processed);
    params[14] = std::to_string(rsu_tasks_failed);
    params[15] = std::to_string(rsu_tasks_rejected);
    params[16] = std::to_string(avg_proc_time);
    params[17] = std::to_string(avg_queue_time);
    params[18] = std::to_string(success_rate);
    params[19] = std::to_string(connected_vehicles);
    
    for (int i = 0; i < 20; i++) {
        paramValues[i] = params[i].c_str();
    }
    
    const char* query = 
        "INSERT INTO rsu_status ("
        "rsu_id, update_time, cpu_total, cpu_allocable, cpu_available, cpu_utilization, "
        "memory_total, memory_available, memory_utilization, queue_length, processing_count, "
        "max_concurrent_tasks, tasks_received, tasks_processed, tasks_failed, tasks_rejected, "
        "avg_processing_time, avg_queue_time, success_rate, connected_vehicles_count"
        ") VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18, $19, $20)";
    
    PGresult* res = PQexecParams(conn, query, 20, nullptr, paramValues, nullptr, nullptr, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        EV_ERROR << "RSU status insert failed: " << PQerrorMessage(conn) << endl;
    } else {
        EV_DEBUG << "RSU status inserted - CPU: " << (cpu_util*100) << "%, Mem: " << (mem_util*100) 
                 << "%, Tasks: " << rsu_processing_count << "/" << rsu_max_concurrent << endl;
    }
    
    PQclear(res);
}

void MyRSUApp::insertRSUMetadata() {
    PGconn* conn = getDBConnection();
    if (!conn) return;
    
    std::string rsu_id_str = "RSU_" + std::to_string(rsu_id);
    
    // Get RSU position from mobility module
    double pos_x = 0.0, pos_y = 0.0, pos_z = 0.0;
    cModule* mobilityModule = getParentModule()->getSubmodule("mobility");
    if (mobilityModule) {
        pos_x = mobilityModule->par("x").doubleValue();
        pos_y = mobilityModule->par("y").doubleValue();
        pos_z = mobilityModule->par("z").doubleValue();
    }
    
    // Build INSERT query with UPSERT (ON CONFLICT UPDATE)
    const char* paramValues[11];
    std::string params[11];
    
    params[0] = rsu_id_str;
    params[1] = std::to_string(pos_x);
    params[2] = std::to_string(pos_y);
    params[3] = std::to_string(pos_z);
    params[4] = "300.0";  // Default coverage radius 300m
    params[5] = std::to_string(edgeCPU_GHz);
    params[6] = std::to_string(edgeMemory_GB);
    params[7] = "100.0";  // Default storage 100GB
    params[8] = "100.0";  // Default bandwidth 100Mbps
    params[9] = std::to_string(maxVehicles);
    params[10] = std::to_string(processingDelay_ms);
    
    for (int i = 0; i < 11; i++) {
        paramValues[i] = params[i].c_str();
    }
    
    const char* query = 
        "INSERT INTO rsu_metadata ("
        "rsu_id, pos_x, pos_y, pos_z, coverage_radius, cpu_capacity_ghz, memory_capacity_gb, "
        "storage_capacity_gb, bandwidth_mbps, max_vehicles, base_processing_delay_ms, deployment_time"
        ") VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, CURRENT_TIMESTAMP) "
        "ON CONFLICT (rsu_id) DO UPDATE SET "
        "pos_x = EXCLUDED.pos_x, pos_y = EXCLUDED.pos_y, pos_z = EXCLUDED.pos_z, "
        "updated_at = CURRENT_TIMESTAMP";
    
    PGresult* res = PQexecParams(conn, query, 11, nullptr, paramValues, nullptr, nullptr, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        EV_ERROR << "RSU metadata insert failed: " << PQerrorMessage(conn) << endl;
    } else {
        EV_INFO << "✓ RSU metadata inserted/updated for " << rsu_id_str << endl;
        std::cout << "RSU_METADATA: " << rsu_id_str << " @ (" << pos_x << "," << pos_y << ") - " 
                  << edgeCPU_GHz << " GHz, " << edgeMemory_GB << " GB" << std::endl;
    }
    
    PQclear(res);
}

void MyRSUApp::insertVehicleMetadata(const std::string& vehicle_id) {
    PGconn* conn = getDBConnection();
    if (!conn) {
        std::cout << "DB_WARN: No database connection, skipping vehicle_metadata insert for " 
                  << vehicle_id << std::endl;
        return;
    }
    
    // Get vehicle twin data if available
    auto it = vehicle_twins.find(vehicle_id);
    if (it == vehicle_twins.end()) {
        std::cout << "DB_WARN: No twin data available for " << vehicle_id << std::endl;
        return;
    }
    
    const VehicleDigitalTwin& twin = it->second;
    
    // Build INSERT query with UPSERT
    const char* paramValues[6];
    std::string params[6];
    
    params[0] = vehicle_id;
    params[1] = std::to_string(twin.cpu_total / 1e9);  // Convert Hz to GHz
    params[2] = std::to_string(twin.mem_total / 1e6);  // Convert bytes to MB
    params[3] = std::to_string(twin.first_seen_time);
    params[4] = std::to_string(twin.last_update_time);
    params[5] = "false";  // Default: not a service vehicle
    
    for (int i = 0; i < 6; i++) {
        paramValues[i] = params[i].c_str();
    }
    
    const char* query = 
        "INSERT INTO vehicle_metadata ("
        "vehicle_id, cpu_capacity_ghz, memory_capacity_mb, first_seen_time, last_seen_time, service_vehicle"
        ") VALUES ($1, $2, $3, $4, $5, $6::boolean) "
        "ON CONFLICT (vehicle_id) DO UPDATE SET "
        "last_seen_time = EXCLUDED.last_seen_time, updated_at = CURRENT_TIMESTAMP";
    
    PGresult* res = PQexecParams(conn, query, 6, nullptr, paramValues, nullptr, nullptr, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        EV_ERROR << "Vehicle metadata insert failed: " << PQerrorMessage(conn) << endl;
        std::cout << "DB_ERROR: Failed to insert vehicle_metadata for " << vehicle_id 
                  << ": " << PQerrorMessage(conn) << std::endl;
    } else {
        EV_DEBUG << "✓ Vehicle metadata inserted/updated for " << vehicle_id << endl;
        std::cout << "DB_INSERT: vehicle_metadata for " << vehicle_id << std::endl;
    }
    
    PQclear(res);
}


}  // namespace complex_network




