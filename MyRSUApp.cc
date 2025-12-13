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
        // Initialize edge server resources
        edgeCPU_GHz = par("edgeCPU_GHz").doubleValue();
        edgeMemory_GB = par("edgeMemory_GB").doubleValue();
        maxVehicles = par("maxVehicles").intValue();
        processingDelay_ms = par("processingDelay_ms").doubleValue();
        
        std::cout << "CONSOLE: MyRSUApp " << getParentModule()->getFullName() 
                  << " initialized with edge resources:" << std::endl;
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
        
        poster.start();
        
        std::cout << "CONSOLE: MyRSUApp - RSUHttpPoster STARTED successfully" << std::endl;
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
    
    // Log to file
    try {
        std::ofstream lof("rsu_poster.log", std::ios::app);
        if (lof) {
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            char timebuf[100];
            std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
            lof << timebuf << " ONWSM raw='" << payload << "'\n";
            lof.close();
        }
    } catch(const std::exception& e) {
        std::cout << "CONSOLE: MyRSUApp - Log file error: " << e.what() << std::endl;
    }

    // Diagnostic enqueue: always write a diagnostic log entry and enqueue a small ping JSON
    try {
        std::ostringstream diagss;
        diagss << "{\"diag\":\"ping\",\"simTime\":" << simTime().dbl() << "}";
        std::string diagJson = diagss.str();

        std::ofstream dlf("rsu_poster.log", std::ios::app);
        if (dlf) {
            auto now2 = std::chrono::system_clock::now();
            std::time_t t2 = std::chrono::system_clock::to_time_t(now2);
            char tb2[100];
            std::strftime(tb2, sizeof(tb2), "%Y-%m-%d %H:%M:%S", std::localtime(&t2));
            dlf << tb2 << " DIAGNOSTIC_ENQUEUE payload='" << diagJson << "'\n";
            dlf.close();
        }

        poster.enqueue(diagJson);
        std::cout << "CONSOLE: MyRSUApp - DIAGNOSTIC poster.enqueue() called with: " << diagJson << std::endl;
    } catch (const std::exception &e) {
        std::cout << "CONSOLE: MyRSUApp - Diagnostic enqueue/log error: " << e.what() << std::endl;
    }
    
    EV << "RSU: Received message from vehicle at time " << simTime() << endl;

    std::ostringstream js;
    bool posted = false;
    
    // Check for VEHICLE_DATA prefix
    if (!payload.empty() && payload.rfind("VEHICLE_DATA|", 0) == 0) {
        std::cout << "CONSOLE: MyRSUApp - ✓ Found VEHICLE_DATA prefix, parsing..." << std::endl;
        
        std::string body = payload.substr(13);  // strlen("VEHICLE_DATA|") = 13
        std::istringstream parts(body);
        std::string token;
        std::map<std::string, std::string> kv;
        
        int field_count = 0;
        while (std::getline(parts, token, '|')) {
            auto pos = token.find(':');
            if (pos != std::string::npos) {
                std::string k = token.substr(0, pos);
                std::string v = token.substr(pos + 1);
                kv[k] = v;
                field_count++;
                std::cout << "CONSOLE: MyRSUApp -   Field[" << field_count 
                          << "]: " << k << " = " << v << std::endl;
            }
        }
        
        std::cout << "CONSOLE: MyRSUApp - Parsed " << field_count << " fields" << std::endl;

        // Build JSON
        js << "{";
        bool first = true;
        for (const auto &p : kv) {
            if (!first) js << ", ";
            first = false;
            
            if (p.first == "MAC") {
                js << "\"" << p.first << "\": \"" << p.second << "\"";
            } else {
                js << "\"" << p.first << "\": " << p.second;
            }
        }
        js << "}";

        std::string jsonStr = js.str();
        std::cout << "CONSOLE: MyRSUApp - JSON built: " << jsonStr << std::endl;
        std::cout << "CONSOLE: MyRSUApp - JSON length: " << jsonStr.size() << " bytes" << std::endl;
        std::cout << "CONSOLE: MyRSUApp - Calling poster.enqueue()..." << std::endl;
        
        EV << "MyRSUApp: enqueueing JSON (len=" << jsonStr.size() << ")\n";
        
        poster.enqueue(jsonStr);
        
        std::cout << "CONSOLE: MyRSUApp - ✓ JSON enqueued successfully!" << std::endl;
        EV << "MyRSUApp: enqueued JSON\n";
        posted = true;
        
    } else {
        std::cout << "CONSOLE: MyRSUApp - ⚠ No VEHICLE_DATA prefix found" << std::endl;
    }

    if (!posted) {
        std::cout << "CONSOLE: MyRSUApp - Creating fallback JSON..." << std::endl;
        
        std::ostringstream ss;
        ss << "{\"VehID\":" << std::fixed << std::setprecision(2) << dsm->getSenderPos().x 
           << ", \"Time\":" << simTime().dbl() 
           << ", \"raw\":\"";
        
        for (char c : payload) {
            if (c == '"') ss << "\\\"";
            else if (c == '\\') ss << "\\\\";
            else ss << c;
        }
        ss << "\"}";
        
        std::string jsonStr = ss.str();
        std::cout << "CONSOLE: MyRSUApp - Fallback JSON: " << jsonStr << std::endl;
        
        EV << "MyRSUApp: enqueueing fallback JSON (len=" << jsonStr.size() << ")\n";
        
        poster.enqueue(jsonStr);
        
        std::cout << "CONSOLE: MyRSUApp - ✓ Fallback JSON enqueued" << std::endl;
        EV << "MyRSUApp: enqueued fallback JSON\n";
    }
    
    std::cout << "***** MyRSUApp - onWSM() COMPLETED *****\n" << std::endl;
}

void MyRSUApp::finish() {
    std::cout << "\n=== CONSOLE: MyRSUApp - finish() called ===" << std::endl;
    
    // Log final Digital Twin statistics
    logDigitalTwinState();
    
    EV << "MyRSUApp: stopping RSUHttpPoster...\n";
    // cancel and delete our beacon self-message if still scheduled
    if (beaconMsg) {
        cancelAndDelete(beaconMsg);
        beaconMsg = nullptr;
    }

    // stop poster worker
    poster.stop();

    std::cout << "CONSOLE: MyRSUApp - Poster stopped successfully" << std::endl;
    std::cout << "=== MyRSUApp FINISHED ===" << std::endl;
    EV << "MyRSUApp: RSUHttpPoster stopped\n";

    DemoBaseApplLayer::finish();
}

void MyRSUApp::handleMessage(cMessage* msg) {
    std::cout << "CONSOLE: MyRSUApp handleMessage() called with message: " << msg->getName()
              << " at time " << simTime() << std::endl;

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

}

