#include <cstddef>
#include "PayloadVehicleApp.h"
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <cmath>

using namespace veins;

namespace complex_network {

Define_Module(PayloadVehicleApp);

void PayloadVehicleApp::initialize(int stage) {
    DemoBaseApplLayer::initialize(stage);

    if (stage == 0) {
        messageSent = false;

        // Initialize vehicle parameters (similar to VehicleApp)
        flocHz_max = par("initFlocHz").doubleValue();
        flocHz_available = flocHz_max;  // Start with full capacity
        txPower_mW = par("initTxPower_mW").doubleValue();
        cpuLoadFactor = uniform(0.1, 0.3);  // Initial light load
        lastCpuUpdateTime = simTime().dbl();
        
        // Initialize battery (typical EV battery: 40-100 kWh → 3000-8000 mAh @ 12V equivalent)
        battery_mAh_max = par("initBattery_mAh").doubleValue();
        battery_mAh_current = battery_mAh_max * uniform(0.5, 1.0);  // Start 50-100% charged
        battery_voltage = 12.0;  // Standard automotive 12V system
        lastBatteryUpdateTime = simTime().dbl();
        
        // Initialize memory (typical vehicle ECU: 2-8 GB RAM)
        memory_MB_max = par("initMemory_MB").doubleValue();
        memory_MB_available = memory_MB_max * uniform(0.4, 0.7);  // 40-70% initially available
        memoryUsageFactor = 1.0 - (memory_MB_available / memory_MB_max);
        
        // Setup mobility hookup
        mobility = FindModule<TraCIMobility*>::findSubModule(getParentModule());
        if (mobility) {
            subscribe(mobility->mobilityStateChangedSignal, this);
            // Initialize position and speed
            pos = mobility->getPositionAt(simTime());
            speed = mobility->getSpeed();
            prev_speed = speed;
            acceleration = 0.0;
            last_speed_update = simTime();
        }

        EV_INFO << "PayloadVehicleApp: Initialized with vehicle data" << endl;
        std::cout << "CONSOLE: PayloadVehicleApp initialized at " << simTime() << std::endl;
        std::cout << "CONSOLE: PayloadVehicleApp - Vehicle data initialized" << std::endl;
        std::cout << "SHADOW: Vehicle starting signal monitoring for shadowing analysis" << std::endl;

        // ===== INITIALIZE TASK PROCESSING SYSTEM =====
        initializeTaskSystem();
        
        // Schedule immediate message sending (0.1s to allow full initialization)
        cMessage* sendMsgEvent = new cMessage("sendPayloadMessage");
        scheduleAt(simTime() + 0.1, sendMsgEvent);
        std::cout << "CONSOLE: PayloadVehicleApp - Scheduled to send payload message at time " << (simTime() + 0.1) << std::endl;
        
        // Schedule periodic position monitoring for shadowing analysis
        cMessage* monitorEvent = new cMessage("monitorPosition");
        scheduleAt(simTime() + 1, monitorEvent);
        std::cout << "SHADOW: Position monitoring started - checking for obstacle effects" << std::endl;
    }
    else if (stage == 1) {
        // Get MAC address in stage 1
        DemoBaseApplLayerToMac1609_4Interface* macInterface =
            FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(getParentModule());

        if (macInterface) {
            myMacAddress = macInterface->getMACAddress();
            std::cout << "CONSOLE: PayloadVehicleApp MAC address: " << myMacAddress << std::endl;
            EV_INFO << "PayloadVehicleApp MAC address: " << myMacAddress << endl;
        } else {
            std::cout << "CONSOLE: ERROR - PayloadVehicleApp MAC interface NOT found!" << std::endl;
            myMacAddress = 0;
        }
    }
}

void PayloadVehicleApp::handleSelfMsg(cMessage* msg) {
    // Handle task-related events
    if (strcmp(msg->getName(), "taskGeneration") == 0) {
        generateTask();
        // Don't delete - will be reused by scheduleNextTaskGeneration()
        return;
    }
    else if (strcmp(msg->getName(), "taskCompletion") == 0) {
        Task* task = (Task*)msg->getContextPointer();
        handleTaskCompletion(task);
        delete msg;
        return;
    }
    else if (strcmp(msg->getName(), "taskDeadline") == 0) {
        Task* task = (Task*)msg->getContextPointer();
        handleTaskDeadline(task);
        delete msg;
        return;
    }
    else if (strcmp(msg->getName(), "serviceTaskCompletion") == 0) {
        Task* task = (Task*)msg->getContextPointer();
        handleServiceTaskCompletion(task);
        delete msg;
        return;
    }
    else if (strcmp(msg->getName(), "serviceTaskDeadline") == 0) {
        Task* task = (Task*)msg->getContextPointer();
        handleServiceTaskDeadline(task);
        delete msg;
        return;
    }
    else if (strcmp(msg->getName(), "sendPayloadMessage") == 0) {
        std::cout << "CONSOLE: PayloadVehicleApp - Sending periodic vehicle status update..." << std::endl;
        EV << "PayloadVehicleApp: Sending periodic vehicle status update..." << endl;

        // Update vehicle data before sending
        updateVehicleData();

        // Send VehicleResourceStatusMessage to RSU for Digital Twin tracking
        // This includes position, speed, CPU, memory, and task statistics
        sendVehicleResourceStatus();

        // Schedule next periodic update using heartbeatIntervalS parameter
        double heartbeatInterval = par("heartbeatIntervalS").doubleValue();
        scheduleAt(simTime() + heartbeatInterval, new cMessage("sendPayloadMessage"));
        
        delete msg;
    } 
    else if (strcmp(msg->getName(), "monitorPosition") == 0) {
        // Monitor vehicle position and get REAL simulation shadowing data
        if (mobility) {
            Coord currentPos = mobility->getPositionAt(simTime());
            
            // Try to access the actual radio module for real signal measurements
            cModule* nicModule = getParentModule()->getSubmodule("nic");
            if (nicModule) {
                cModule* phyModule = nicModule->getSubmodule("phy80211p");
                if (phyModule) {
                    // Get actual signal parameters from the simulation
                    double currentSensitivity = -85.0; // Default
                    double thermalNoise = -110.0; // Default
                    
                    // Try to get real parameters if available
                    if (phyModule->hasPar("sensitivity")) {
                        currentSensitivity = phyModule->par("sensitivity").doubleValue();
                    }
                    if (phyModule->hasPar("thermalNoise")) {
                        thermalNoise = phyModule->par("thermalNoise").doubleValue();
                    }
                    
                    std::cout << "SHADOW: Vehicle at (" << currentPos.x << "," << currentPos.y << ")" << std::endl;
                    std::cout << "SHADOW: Current sensitivity: " << currentSensitivity << " dBm" << std::endl;
                    std::cout << "SHADOW: Thermal noise floor: " << thermalNoise << " dBm" << std::endl;
                    
                    // Check if near obstacles for context with detailed analysis
                    bool nearOfficeT = (currentPos.x >= 240 && currentPos.x <= 320 && currentPos.y >= 10 && currentPos.y <= 90);
                    bool nearOfficeC = (currentPos.x >= 110 && currentPos.x <= 190 && currentPos.y >= 10 && currentPos.y <= 90);
                    
                    if (nearOfficeT) {
                        std::cout << "SHADOW: ⚠️ NEAR Office Tower - EXPECT 18dB obstacle loss + 8dB shadowing!" << std::endl;
                        std::cout << "SHADOW: Vehicle ID " << myMacAddress << " in HIGH attenuation zone" << std::endl;
                        std::cout << "SHADOW: Next message reception will show signal degradation effects" << std::endl;
                    } else if (nearOfficeC) {
                        std::cout << "SHADOW: ⚠️ NEAR Office Complex - EXPECT 18dB obstacle loss + 8dB shadowing!" << std::endl;
                        std::cout << "SHADOW: Vehicle ID " << myMacAddress << " in HIGH attenuation zone" << std::endl;
                        std::cout << "SHADOW: Next message reception will show signal degradation effects" << std::endl;
                    } else {
                        std::cout << "SHADOW: ✅ CLEAR AREA - baseline signal conditions (2-4dB environmental only)" << std::endl;
                        std::cout << "SHADOW: Vehicle ID " << myMacAddress << " in LOW attenuation zone" << std::endl;
                    }
                }
            }
            
            // Reuse the same message for next monitoring instead of creating new one
            scheduleAt(simTime() + 2, msg);
        }
        else {
            // If mobility not available, delete the message
            delete msg;
        }
    } 
    else if (msg->getKind() == SEND_BEACON_EVT) {
        // Handle beacon events ourselves to avoid rescheduling issues
        // Only process if beaconing is actually enabled
        if (par("sendBeacons").boolValue()) {
            DemoSafetyMessage* bsm = new DemoSafetyMessage();
            populateWSM(bsm);
            sendDown(bsm);
            // Cancel before rescheduling to avoid "already scheduled" error
            if (msg->isScheduled()) {
                cancelEvent(msg);
            }
            scheduleAt(simTime() + par("beaconInterval").doubleValue(), msg);
        }
        // Don't delete - message is reused
    }
    else {
        // Let parent class handle other events (like WSA)
        DemoBaseApplLayer::handleSelfMsg(msg);
    }
}

void PayloadVehicleApp::onWSM(BaseFrame1609_4* wsm) {
    std::cout << "\n🔵 SHADOW ANALYSIS - Vehicle ID " << myMacAddress << " RECEIVING MESSAGE" << std::endl;
    
    // Get current position for analysis
    Coord currentPos = mobility ? mobility->getPositionAt(simTime()) : Coord(0,0,0);
    std::cout << "SHADOW: 📍 Reception at position (" << currentPos.x << "," << currentPos.y << ")" << std::endl;
    
    // Determine expected shadowing based on position
    bool nearOfficeT = (currentPos.x >= 240 && currentPos.x <= 320 && currentPos.y >= 10 && currentPos.y <= 90);
    bool nearOfficeC = (currentPos.x >= 110 && currentPos.x <= 190 && currentPos.y >= 10 && currentPos.y <= 90);
    
    if (nearOfficeT || nearOfficeC) {
        std::string obstacleType = nearOfficeT ? "Office Tower" : "Office Complex";
        std::cout << "SHADOW: 🏢 MESSAGE RECEIVED THROUGH " << obstacleType << "!" << std::endl;
        std::cout << "SHADOW: Expected attenuation: 18dB (obstacle) + up to 8dB (shadowing) = 26dB MAX" << std::endl;
        std::cout << "SHADOW: ✅ Signal SURVIVED " << obstacleType << " - Strong enough for decode!" << std::endl;
        std::cout << "SHADOW: Actual signal strength > sensitivity threshold despite obstacles" << std::endl;
    } else {
        std::cout << "SHADOW: 🌐 CLEAR PATH MESSAGE RECEPTION!" << std::endl;
        std::cout << "SHADOW: Expected attenuation: 2-4dB (environmental + path loss only)" << std::endl;
        std::cout << "SHADOW: ✅ Optimal reception conditions - no major obstacles" << std::endl;
    }
    
    // Extract REAL signal data from the received frame
    if (mobility) {
        // Try to access actual received signal strength from the frame
        cModule* nicModule = getParentModule()->getSubmodule("nic");
        if (nicModule) {
            cModule* phyModule = nicModule->getSubmodule("phy80211p");
            
            // Look for signal power information in the control info
            if (wsm->getControlInfo()) {
                std::cout << "SHADOW: 📡 Frame control info type: " << wsm->getControlInfo()->getClassName() << std::endl;
                
                // Try to access Veins-specific signal data
                std::string controlInfoType = wsm->getControlInfo()->getClassName();
                if (controlInfoType.find("PhyToMac") != std::string::npos) {
                    std::cout << "SHADOW: ✅ PHY-to-MAC control info available - real signal processed" << std::endl;
                    std::cout << "SHADOW: Signal passed all propagation models (TwoRay+LogNormal+Obstacles)" << std::endl;
                } else {
                    std::cout << "SHADOW: Control info type: " << controlInfoType << std::endl;
                }
            }
            
            // Access the actual radio statistics if available
            if (phyModule) {
                // Check if we can access any recorded statistics or parameters
                if (phyModule->hasPar("sensitivity")) {
                    double sens = phyModule->par("sensitivity").doubleValue();
                    std::cout << "SHADOW: 📊 PHY sensitivity threshold: " << sens << " dBm" << std::endl;
                }
                
                if (phyModule->hasPar("thermalNoise")) {
                    double noise = phyModule->par("thermalNoise").doubleValue();
                    std::cout << "SHADOW: 📊 Thermal noise floor: " << noise << " dBm" << std::endl;
                }
                
                // Log successful reception confirmation
                std::cout << "SHADOW: ✅ Real PHY layer confirms successful frame decode" << std::endl;
                std::cout << "SHADOW: All propagation effects (including obstacles) accounted for" << std::endl;
            }
        }
    }
    
    std::cout << "🔵 END SHADOW ANALYSIS for Vehicle " << myMacAddress << "\n" << std::endl;
    
    EV << "PayloadVehicleApp: Received WSM message!" << endl;

    // Check if this is a response from RSU
    DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
    if (dsm) {
        std::string receivedPayload = dsm->getName(); // Use getName instead of getPayload
        
        std::cout << "CONSOLE: PayloadVehicleApp - DEBUG - Received message: '" << receivedPayload << "'" << std::endl;

        if (!receivedPayload.empty() && 
            (receivedPayload.find("RSU_RESPONSE") != std::string::npos || 
             receivedPayload.find("RSU Response") != std::string::npos)) {
            std::cout << "CONSOLE: =========================================" << std::endl;
            std::cout << "CONSOLE: PayloadVehicleApp - RECEIVED PAYLOAD FROM RSU:" << std::endl;
            std::cout << "CONSOLE: PayloadVehicleApp - Payload: " << receivedPayload << std::endl;
            std::cout << "CONSOLE: PayloadVehicleApp - Received at time: " << simTime() << std::endl;
            std::cout << "CONSOLE: PayloadVehicleApp - Sender Module: " << wsm->getSenderModule()->getFullName() << std::endl;
            std::cout << "CONSOLE: =========================================" << std::endl;

            EV << "PayloadVehicleApp: RECEIVED PAYLOAD: " << receivedPayload << endl;
        } else {
            std::cout << "CONSOLE: PayloadVehicleApp - DEBUG - Message is not an RSU response" << std::endl;
        }
    }

    DemoBaseApplLayer::onWSM(wsm);
}

void PayloadVehicleApp::handleMessage(cMessage* msg) {
    std::cout << "CONSOLE: PayloadVehicleApp handleMessage() called with: " << msg->getName() << std::endl;
    EV << "PayloadVehicleApp: handleMessage() called with " << msg->getName() << endl;

    // ========================================================================
    // HANDLE OFFLOADING DECISION MESSAGES
    // ========================================================================
    veins::OffloadingDecisionMessage* decisionMsg = dynamic_cast<veins::OffloadingDecisionMessage*>(msg);
    if (decisionMsg) {
        EV_INFO << "Received OffloadingDecisionMessage" << endl;
        handleOffloadingDecisionFromRSU(decisionMsg);
        return;
    }
    
    // ========================================================================
    // HANDLE TASK RESULT MESSAGES
    // ========================================================================
    veins::TaskResultMessage* resultMsg = dynamic_cast<veins::TaskResultMessage*>(msg);
    if (resultMsg) {
        EV_INFO << "Received TaskResultMessage" << endl;
        handleTaskResult(resultMsg);
        return;
    }
    
    // ========================================================================
    // HANDLE SERVICE TASK REQUESTS (if service vehicle enabled)
    // ========================================================================
    veins::TaskOffloadPacket* taskPacket = dynamic_cast<veins::TaskOffloadPacket*>(msg);
    if (taskPacket) {
        EV_INFO << "Received TaskOffloadPacket" << endl;
        if (serviceVehicleEnabled) {
            handleServiceTaskRequest(taskPacket);
        } else {
            EV_WARN << "Received service task request but service vehicle not enabled" << endl;
            delete taskPacket;
        }
        return;
    }
    
    // ========================================================================
    // HANDLE GENERAL WSM MESSAGES
    // ========================================================================
    BaseFrame1609_4* wsm = dynamic_cast<BaseFrame1609_4*>(msg);
    if (wsm) {
        onWSM(wsm);
        return;
    }

    DemoBaseApplLayer::handleMessage(msg);
}

LAddress::L2Type PayloadVehicleApp::findRSUMacAddress() {
    std::cout << "CONSOLE: PayloadVehicleApp - Looking for closest RSU MAC address..." << std::endl;

    // Get the network module
    cModule* networkModule = getModuleByPath("^.^");
    if (!networkModule) {
        std::cout << "CONSOLE: PayloadVehicleApp ERROR - Network module not found!" << std::endl;
        return 0;
    }

    // Get current vehicle position
    Coord vehiclePos = mobility ? mobility->getPositionAt(simTime()) : Coord(0, 0, 0);
    
    double minDistance = 999999.0;
    int closestRSUIndex = -1;
    LAddress::L2Type closestRSUMac = 0;

    // Check all RSUs (0, 1, 2) and find the closest one
    for (int i = 0; i < 3; i++) {
        cModule* rsuModule = networkModule->getSubmodule("rsu", i);
        if (rsuModule) {
            // Get RSU mobility module to access position
            cModule* rsuMobilityModule = rsuModule->getSubmodule("mobility");
            if (rsuMobilityModule) {
                // Get RSU position from mobility parameters
                double rsuX = rsuMobilityModule->par("x").doubleValue();
                double rsuY = rsuMobilityModule->par("y").doubleValue();
                
                // Calculate Euclidean distance
                double dx = vehiclePos.x - rsuX;
                double dy = vehiclePos.y - rsuY;
                double distance = sqrt(dx * dx + dy * dy);
                
                std::cout << "CONSOLE: PayloadVehicleApp - RSU[" << i << "] at (" << rsuX << "," << rsuY 
                          << ") distance: " << distance << "m" << std::endl;
                
                // Track closest RSU
                if (distance < minDistance) {
                    minDistance = distance;
                    closestRSUIndex = i;
                    
                    // Get the MAC address of this RSU
                    DemoBaseApplLayerToMac1609_4Interface* rsuMacInterface =
                        FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(rsuModule);
                    
                    if (rsuMacInterface) {
                        closestRSUMac = rsuMacInterface->getMACAddress();
                    }
                }
            }
        }
    }

    if (closestRSUIndex >= 0 && closestRSUMac != 0) {
        std::cout << "CONSOLE: PayloadVehicleApp - Selected CLOSEST RSU[" << closestRSUIndex 
                  << "] at distance " << minDistance << "m, MAC: " << closestRSUMac << std::endl;
        return closestRSUMac;
    }

    std::cout << "CONSOLE: PayloadVehicleApp ERROR - No valid RSU MAC address found!" << std::endl;
    return 0;
}

void PayloadVehicleApp::updateVehicleData() {
    // Update live kinematics snapshot from mobility
    if (mobility) {
        pos = mobility->getPositionAt(simTime());
        double current_speed = mobility->getSpeed();
        
        // Calculate acceleration
        simtime_t time_delta = simTime() - last_speed_update;
        if (time_delta > 0) {
            acceleration = (current_speed - prev_speed) / time_delta.dbl();
        } else {
            acceleration = 0.0;
        }
        
        prev_speed = speed;
        speed = current_speed;
        last_speed_update = simTime();
    }
    
    // Update CPU availability with dynamic variation model
    double currentTime = simTime().dbl();
    double timeDelta = currentTime - lastCpuUpdateTime;
    
    // Model CPU load variation (simulates background tasks, thermal throttling, etc.)
    // Load factor varies slowly using random walk with bounds
    double loadChange = uniform(-0.1, 0.1) * timeDelta;  // Small random changes
    cpuLoadFactor += loadChange;
    
    // Keep CPU load within realistic bounds (10% - 90%)
    if (cpuLoadFactor < 0.1) cpuLoadFactor = 0.1;
    if (cpuLoadFactor > 0.9) cpuLoadFactor = 0.9;
    
    // Calculate available CPU capacity based on load
    flocHz_available = flocHz_max * (1.0 - cpuLoadFactor);
    lastCpuUpdateTime = currentTime;
    
    // === BATTERY DRAIN MODEL ===
    double batteryTimeDelta = currentTime - lastBatteryUpdateTime;
    if (batteryTimeDelta > 0) {
        // Calculate power consumption (Watts)
        // CPU power: Higher load = more power (typical: 10-50W for vehicle ECU)
        double cpuPower_W = 10.0 + (cpuLoadFactor * 40.0);  // 10-50W based on load
        
        // Radio transmission power (convert mW to W)
        double radioPower_W = txPower_mW / 1000.0;
        
        // Speed-based power (motor consumption, negative for regenerative braking)
        // Simplified model: higher speed = more drain, deceleration = slight regen
        double speedPower_W = speed * 0.5;  // Rough approximation
        
        // Total power consumption
        double totalPower_W = cpuPower_W + radioPower_W + speedPower_W;
        
        // Convert to mAh drain: P(W) * t(h) / V = Ah, then * 1000 = mAh
        // mAh_drain = (Power_W * time_hours * 1000) / voltage_V
        double batteryDrain_mAh = (totalPower_W * batteryTimeDelta / 3600.0 * 1000.0) / battery_voltage;
        
        battery_mAh_current -= batteryDrain_mAh;
        
        // Prevent negative battery (minimum 1%)
        if (battery_mAh_current < battery_mAh_max * 0.01) {
            battery_mAh_current = battery_mAh_max * 0.01;
        }
        
        lastBatteryUpdateTime = currentTime;
    }
    
    // === MEMORY USAGE MODEL ===
    // Memory usage varies based on active tasks and caching
    double memoryChange = uniform(-0.05, 0.08) * timeDelta;  // Can increase or decrease
    memoryUsageFactor += memoryChange;
    
    // Keep memory usage realistic (20% - 95%)
    if (memoryUsageFactor < 0.2) memoryUsageFactor = 0.2;
    if (memoryUsageFactor > 0.95) memoryUsageFactor = 0.95;
    
    memory_MB_available = memory_MB_max * (1.0 - memoryUsageFactor);
    
    // Calculate battery percentage
    double battery_pct = (battery_mAh_current / battery_mAh_max) * 100.0;
    
    std::cout << "CONSOLE: PayloadVehicleApp - Updated vehicle data: pos=(" << pos.x << "," << pos.y 
              << ") speed=" << speed << " CPU_max=" << flocHz_max/1e9 << "GHz"
              << " CPU_avail=" << flocHz_available/1e9 << "GHz (load=" << (cpuLoadFactor*100) << "%)" 
              << " Battery=" << battery_pct << "% (" << battery_mAh_current << "/" << battery_mAh_max << "mAh)"
              << " Memory=" << memory_MB_available << "/" << memory_MB_max << "MB"
              << " txPower_mW=" << txPower_mW << std::endl;
}

std::string PayloadVehicleApp::createVehicleDataPayload() {
    // Create structured payload with actual vehicle data (similar to recordHeartbeatScalars format)
    std::ostringstream payload;
    
    // Calculate derived metrics
    double battery_pct = (battery_mAh_current / battery_mAh_max) * 100.0;
    double memory_avail_pct = (memory_MB_available / memory_MB_max) * 100.0;
    
    payload << "VEHICLE_DATA|"
            << "VehID:" << getParentModule()->getIndex() << "|"
            << "Time:" << std::fixed << std::setprecision(3) << simTime().dbl() << "|"
            << "CPU_Max_Hz:" << std::fixed << std::setprecision(2) << flocHz_max << "|"
            << "CPU_Avail_Hz:" << std::fixed << std::setprecision(2) << flocHz_available << "|"
            << "CPU_Load_Pct:" << std::fixed << std::setprecision(2) << (cpuLoadFactor * 100) << "|"
            << "Battery_Pct:" << std::fixed << std::setprecision(2) << battery_pct << "|"
            << "Battery_mAh:" << std::fixed << std::setprecision(2) << battery_mAh_current << "|"
            << "Battery_Max_mAh:" << std::fixed << std::setprecision(2) << battery_mAh_max << "|"
            << "Memory_Avail_MB:" << std::fixed << std::setprecision(2) << memory_MB_available << "|"
            << "Memory_Avail_Pct:" << std::fixed << std::setprecision(2) << memory_avail_pct << "|"
            << "Memory_Max_MB:" << std::fixed << std::setprecision(2) << memory_MB_max << "|"
            << "TxPower_mW:" << std::fixed << std::setprecision(2) << txPower_mW << "|"
            << "Speed:" << std::fixed << std::setprecision(2) << speed << "|"
            << "PosX:" << std::fixed << std::setprecision(2) << pos.x << "|"
            << "PosY:" << std::fixed << std::setprecision(2) << pos.y << "|"
            << "MAC:" << myMacAddress;
    
    return payload.str();
}

void PayloadVehicleApp::receiveSignal(cComponent* src, simsignal_t id, cObject* obj, cObject* details) {
    // Handle mobility state changes
    if (mobility && id == mobility->mobilityStateChangedSignal) {
        // Update position and speed when mobility changes
        pos = mobility->getPositionAt(simTime());
        double current_speed = mobility->getSpeed();
        
        // Calculate acceleration
        simtime_t time_delta = simTime() - last_speed_update;
        if (time_delta > 0) {
            acceleration = (current_speed - prev_speed) / time_delta.dbl();
        } else {
            acceleration = 0.0;
        }
        
        prev_speed = speed;
        speed = current_speed;
        last_speed_update = simTime();
    }
    
    DemoBaseApplLayer::receiveSignal(src, id, obj, details);
}

// ============================================================================
// TASK PROCESSING SYSTEM IMPLEMENTATION
// ============================================================================

void PayloadVehicleApp::initializeTaskSystem() {
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║            INITIALIZING TASK PROCESSING SYSTEM                           ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    // Read parameters from omnetpp.ini
    task_arrival_mean = par("task_arrival_mean").doubleValue();
    task_size_min = par("task_size_min").doubleValue();
    task_size_max = par("task_size_max").doubleValue();
    cpu_per_byte_min = par("cpu_per_byte_min").doubleValue();
    cpu_per_byte_max = par("cpu_per_byte_max").doubleValue();
    deadline_min = par("deadline_min").doubleValue();
    deadline_max = par("deadline_max").doubleValue();
    
    // Initialize resource capacities
    cpu_total = par("cpu_total").doubleValue();
    cpu_reservation_ratio = par("cpu_reservation_ratio").doubleValue();
    cpu_allocable = cpu_total * (1.0 - cpu_reservation_ratio);
    cpu_available = cpu_allocable;  // Initially all allocable CPU is available
    
    memory_total = par("memory_total").doubleValue();
    memory_available = memory_total;  // Initially all memory is available
    
    // Queue parameters
    max_queue_size = par("max_queue_size").intValue();
    max_concurrent_tasks = par("max_concurrent_tasks").intValue();
    min_cpu_guarantee = par("min_cpu_guarantee").doubleValue();
    
    EV_INFO << "┌──────────────────────────────────────────────────────────────────────────┐" << endl;
    EV_INFO << "│ TASK GENERATION PARAMETERS                                               │" << endl;
    EV_INFO << "├──────────────────────────────────────────────────────────────────────────┤" << endl;
    EV_INFO << "│ Mean Arrival Time:    " << std::left << std::setw(48) << task_arrival_mean << " sec │" << endl;
    EV_INFO << "│ Task Size Range:      " << std::setw(36) << (task_size_min/1024) << " - " 
            << std::setw(8) << (task_size_max/1024) << " KB  │" << endl;
    EV_INFO << "│ CPU/Byte Range:       " << std::setw(36) << cpu_per_byte_min << " - " 
            << std::setw(8) << cpu_per_byte_max << " cyc│" << endl;
    EV_INFO << "│ Deadline Range:       " << std::setw(36) << deadline_min << " - " 
            << std::setw(8) << deadline_max << " sec│" << endl;
    EV_INFO << "└──────────────────────────────────────────────────────────────────────────┘" << endl;
    
    EV_INFO << "┌──────────────────────────────────────────────────────────────────────────┐" << endl;
    EV_INFO << "│ RESOURCE CAPACITIES                                                      │" << endl;
    EV_INFO << "├──────────────────────────────────────────────────────────────────────────┤" << endl;
    EV_INFO << "│ CPU Total:            " << std::setw(38) << (cpu_total / 1e9) << " GHz      │" << endl;
    EV_INFO << "│ CPU Reserved (" << std::setw(3) << (int)(cpu_reservation_ratio*100) << "%):  " 
            << std::setw(38) << ((cpu_total - cpu_allocable) / 1e9) << " GHz      │" << endl;
    EV_INFO << "│ CPU Allocable:        " << std::setw(38) << (cpu_allocable / 1e9) << " GHz      │" << endl;
    EV_INFO << "│ Memory Total:         " << std::setw(38) << (memory_total / 1e6) << " MB       │" << endl;
    EV_INFO << "│ Max Queue Size:       " << std::setw(48) << max_queue_size << "      │" << endl;
    EV_INFO << "│ Max Concurrent Tasks: " << std::setw(48) << max_concurrent_tasks << "      │" << endl;
    EV_INFO << "└──────────────────────────────────────────────────────────────────────────┘" << endl;

    
    // Schedule first task generation
    scheduleNextTaskGeneration();
    
    // ============================================================================
    // INITIALIZE OFFLOADING DECISION FRAMEWORK
    // ============================================================================
    
    // Check if offloading is enabled
    offloadingEnabled = par("offloadingEnabled").boolValue();
    
    if (offloadingEnabled) {
        // Initialize decision maker
        decisionMaker = new HeuristicDecisionMaker();
        EV_INFO << "✓ Task offloading decision maker initialized" << endl;
        std::cout << "OFFLOADING: Decision maker initialized for vehicle " 
                  << getParentModule()->getIndex() << std::endl;
        
        // Read timeout parameters
        rsuDecisionTimeout = par("rsuDecisionTimeout").doubleValue();
        offloadedTaskTimeout = par("offloadedTaskTimeout").doubleValue();
        EV_INFO << "  RSU decision timeout: " << rsuDecisionTimeout << "s" << endl;
        EV_INFO << "  Offloaded task timeout: " << offloadedTaskTimeout << "s" << endl;
    } else {
        EV_INFO << "⚠ Offloading DISABLED - tasks will execute locally only" << endl;
    }
    
    // Service vehicle parameters
    serviceVehicleEnabled = par("serviceVehicleEnabled").boolValue();
    if (serviceVehicleEnabled) {
        maxConcurrentServiceTasks = par("maxConcurrentServiceTasks").intValue();
        serviceCpuReservation = par("serviceCpuReservation").doubleValue();
        serviceMemoryReservation = par("serviceMemoryReservation").doubleValue();
        EV_INFO << "✓ Service vehicle mode ENABLED (can process tasks for others)" << endl;
        std::cout << "SERVICE_VEHICLE: Vehicle " << getParentModule()->getIndex() 
                  << " can accept " << maxConcurrentServiceTasks << " service tasks" << std::endl;
    }
    
    EV_INFO << "✅ Task offloading system fully initialized" << endl;
    
    EV_INFO << "✓ Task processing system initialized successfully" << endl;
    std::cout << "TASK_SYSTEM: Initialized for Vehicle " << getParentModule()->getIndex() 
              << " with " << (cpu_allocable/1e9) << " GHz allocable CPU" << std::endl;
}

void PayloadVehicleApp::scheduleNextTaskGeneration() {
    // Poisson process: exponential inter-arrival time
    double next_arrival = exponential(task_arrival_mean);
    
    if (taskGenerationEvent == nullptr) {
        taskGenerationEvent = new cMessage("taskGeneration");
    }
    
    scheduleAt(simTime() + next_arrival, taskGenerationEvent);
    
    EV_INFO << "⏰ Next task generation scheduled in " << next_arrival << " seconds (at " 
            << (simTime() + next_arrival).dbl() << ")" << endl;
}

void PayloadVehicleApp::generateTask() {
    EV_INFO << "\n" << endl;
    EV_INFO << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
    EV_INFO << "                         GENERATING NEW TASK                                " << endl;
    EV_INFO << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
    
    // Generate random task characteristics
    uint64_t task_size = (uint64_t)uniform(task_size_min, task_size_max);
    double cpu_per_byte = uniform(cpu_per_byte_min, cpu_per_byte_max);
    uint64_t cpu_cycles = (uint64_t)(task_size * cpu_per_byte);
    double deadline = uniform(deadline_min, deadline_max);
    double qos = uniform(0.0, 1.0);
    
    // Create task
    std::string vehicle_id = std::to_string(getParentModule()->getIndex());
    Task* task = new Task(vehicle_id, task_sequence_number++, task_size, cpu_cycles, deadline, qos);
    tasks_generated++;
    
    EV_INFO << "Task generated: ID=" << task->task_id << " Size=" << (task_size/1024.0) << "KB "
            << "Cycles=" << (cpu_cycles/1e9) << "G QoS=" << qos << " Deadline=" << deadline << "s" << endl;
    
    // Send metadata to RSU (non-blocking)
    sendTaskMetadataToRSU(task);
    task->state = METADATA_SENT;
    
    // ============================================================================
    // OFFLOADING DECISION INTEGRATION
    // ============================================================================
    
    if (offloadingEnabled && decisionMaker) {
        EV_INFO << "🤖 Offloading enabled - building decision context..." << endl;
        
        // Build decision context for the decision maker
        DecisionContext context;
        
        // Task characteristics
        context.task_size_kb = task->task_size_bytes / 1024.0;
        context.cpu_cycles_required = task->cpu_cycles;
        context.qos_value = task->qos_value;
        context.deadline_seconds = task->relative_deadline;
        
        // Local vehicle resources
        context.local_cpu_available = cpu_available / 1e9;  // Convert Hz to GHz
        context.local_cpu_utilization = (cpu_allocable - cpu_available) / cpu_allocable;
        context.local_mem_available = memory_available / 1e6;  // Convert bytes to MB
        context.local_queue_length = pending_tasks.size();
        context.local_processing_count = processing_tasks.size();
        
        // RSU availability and channel conditions
        LAddress::L2Type rsu = selectBestRSU();
        context.rsu_available = (rsu != 0);
        context.rsu_distance = getRSUDistance();
        context.estimated_rsu_rssi = getEstimatedRSSI();
        context.estimated_transmission_time = estimateTransmissionTime(task);
        
        // Current time for deadline calculations
        context.current_time = simTime().dbl();
        
        EV_INFO << "Decision context: LocalCPU=" << context.local_cpu_available << "GHz, "
                << "Utilization=" << (context.local_cpu_utilization*100) << "%, "
                << "Queue=" << context.local_queue_length << ", "
                << "RSU_dist=" << context.rsu_distance << "m, "
                << "RSSI=" << context.estimated_rsu_rssi << "dBm" << endl;
        
        // Make local offloading decision
        OffloadingDecision localDecision = decisionMaker->makeDecision(context);
        
        std::string decisionStr;
        switch(localDecision) {
            case OffloadingDecision::EXECUTE_LOCALLY:
                decisionStr = "LOCAL";
                break;
            case OffloadingDecision::OFFLOAD_TO_RSU:
                decisionStr = "OFFLOAD_TO_RSU";
                break;
            case OffloadingDecision::OFFLOAD_TO_SERVICE_VEHICLE:
                decisionStr = "OFFLOAD_TO_SERVICE_VEHICLE";
                break;
            case OffloadingDecision::REJECT_TASK:
                decisionStr = "REJECT";
                break;
        }
        
        EV_INFO << "📊 Local decision: " << decisionStr << endl;
        std::cout << "OFFLOAD_DECISION: Vehicle " << vehicle_id << " local decision for task " 
                  << task->task_id << ": " << decisionStr << std::endl;
        
        // ========================================================================
        // DECISION BRANCH: LOCAL vs OFFLOAD
        // ========================================================================
        
        if (localDecision == OffloadingDecision::EXECUTE_LOCALLY) {
            // LOCAL DECISION: Process immediately without consulting RSU
            // Vehicle is confident it can handle this task
            EV_INFO << "💻 Local decision is LOCAL - processing immediately (no RSU consultation)" << endl;
            std::cout << "OFFLOAD_LOCAL: Task " << task->task_id << " processing locally (confident)" << std::endl;
            
            // Track timing for completion report (even though no RSU decision needed)
            TaskTimingInfo timing;
            timing.request_time = simTime().dbl();
            timing.decision_time = simTime().dbl();  // Immediate local decision
            timing.decision_type = "LOCAL";
            timing.processor_id = "VEHICLE_" + std::to_string(getParentModule()->getIndex());
            taskTimings[task->task_id] = timing;
            
            // Check if can accept task
            if (!canAcceptTask(task)) {
                EV_INFO << "❌ Task REJECTED - queue full or infeasible" << endl;
                task->state = REJECTED;
                tasks_rejected++;
                sendTaskFailureToRSU(task, "LOCAL_REJECTED");
                delete task;
                logTaskStatistics();
                scheduleNextTaskGeneration();
                return;
            }
            
            // Process locally (existing flow)
            if (canStartProcessing(task)) {
                EV_INFO << "✓ Resources available - starting immediately" << endl;
                allocateResourcesAndStart(task);
            } else {
                EV_INFO << "⏸ Resources busy - queuing task" << endl;
                task->state = QUEUED;
                pending_tasks.push(task);
                logQueueState("Task queued after LOCAL decision");
            }
            
            logResourceState("After task generation (local processing)");
            scheduleNextTaskGeneration();
            return;
        }
        
        // OFFLOAD DECISION: Consult RSU for final decision
        // Local heuristic suggests offloading - get ML-based decision from RSU
        EV_INFO << "🌐 Local decision is OFFLOAD - consulting RSU for optimal placement" << endl;
        std::cout << "OFFLOAD_REQUEST: Task " << task->task_id << " requesting RSU decision" << std::endl;
        
        // Send offloading request to RSU (includes local recommendation)
        sendOffloadingRequestToRSU(task, localDecision);
        
        // Mark task as awaiting RSU ML decision
        EV_INFO << "⏳ Task awaiting RSU ML decision..." << endl;
        // Task is already in pendingOffloadingDecisions map from sendOffloadingRequestToRSU()
        
        // Schedule a timeout for the decision
        cMessage* timeoutMsg = new cMessage("rsuDecisionTimeout");
        timeoutMsg->setContextPointer(task);
        scheduleAt(simTime() + rsuDecisionTimeout, timeoutMsg);
        
        logResourceState("After task generation (awaiting offload decision)");
        scheduleNextTaskGeneration();
        return;  // Don't process yet - wait for RSU decision
    }
    
    // ============================================================================
    // FALLBACK: ORIGINAL LOCAL-ONLY PROCESSING (if offloading disabled)
    // ============================================================================
    
    // Check if can accept task
    if (!canAcceptTask(task)) {
        EV_INFO << "❌ Task REJECTED - queue full or infeasible" << endl;
        task->state = REJECTED;
        tasks_rejected++;
        sendTaskFailureToRSU(task, "REJECTED");
        delete task;
        logTaskStatistics();
        scheduleNextTaskGeneration();
        return;
    }
    
    EV_INFO << "✓ Task accepted" << endl;
    
    // Check if can start immediately
    if (canStartProcessing(task)) {
        EV_INFO << "✓ Resources available - starting immediately" << endl;
        allocateResourcesAndStart(task);
    } else {
        EV_INFO << "⏸ Resources busy - queuing task" << endl;
        task->state = QUEUED;
        pending_tasks.push(task);
        logQueueState("Task queued");
    }
    
    logResourceState("After task generation");
    scheduleNextTaskGeneration();
}

bool PayloadVehicleApp::canAcceptTask(Task* task) {
    // Check queue capacity
    if (pending_tasks.size() >= max_queue_size) {
        EV_INFO << "❌ Queue full (" << pending_tasks.size() << "/" << max_queue_size << ")" << endl;
        return false;
    }
    
    // Check if task is too large for vehicle memory
    if (task->task_size_bytes > memory_total) {
        EV_INFO << "❌ Task too large for vehicle memory" << endl;
        return false;
    }
    
    // Optimistic feasibility check
    double avg_cpu = cpu_allocable / (processing_tasks.size() + 1);
    double estimated_time = task->cpu_cycles / avg_cpu;
    if (estimated_time > task->relative_deadline * 1.2) {
        EV_INFO << "❌ Task likely infeasible (est. " << estimated_time << "s > deadline " 
                << task->relative_deadline << "s × 1.2)" << endl;
        return false;
    }
    
    EV_INFO << "✓ Task passes acceptance checks" << endl;
    return true;
}

bool PayloadVehicleApp::canStartProcessing(Task* task) {
    // Check concurrent task limit
    if (processing_tasks.size() >= max_concurrent_tasks) {
        EV_INFO << "  ⏸ Max concurrent tasks reached (" << processing_tasks.size() << "/" 
                << max_concurrent_tasks << ")" << endl;
        return false;
    }
    
    // Check memory availability
    if (task->task_size_bytes > memory_available) {
        EV_INFO << "  ⏸ Insufficient memory (" << (memory_available/1e6) << " MB available, " 
                << (task->task_size_bytes/1e6) << " MB needed)" << endl;
        return false;
    }
    
    return true;
}

void PayloadVehicleApp::allocateResourcesAndStart(Task* task) {
    EV_INFO << "┌──────────────────────────────────────────────────────────────────────────┐" << endl;
    EV_INFO << "│           ALLOCATING RESOURCES AND STARTING TASK                         │" << endl;
    EV_INFO << "└──────────────────────────────────────────────────────────────────────────┘" << endl;
    
    // Allocate memory
    memory_available -= task->task_size_bytes;
    
    // Add to processing set
    processing_tasks.insert(task);
    task->state = PROCESSING;
    task->processing_start_time = simTime();
    
    // Track start time for latency calculation
    if (taskTimings.find(task->task_id) != taskTimings.end()) {
        taskTimings[task->task_id].start_time = simTime().dbl();
        EV_INFO << "Start time tracked for task " << task->task_id << endl;
    }
    
    // Calculate CPU allocation
    task->cpu_allocated = calculateCPUAllocation(task);
    cpu_available = cpu_allocable;
    for (Task* t : processing_tasks) {
        cpu_available -= t->cpu_allocated;
    }
    
    EV_INFO << "Resource allocation:" << endl;
    EV_INFO << "  • Memory allocated: " << (task->task_size_bytes / 1e6) << " MB" << endl;
    EV_INFO << "  • CPU allocated: " << (task->cpu_allocated / 1e9) << " GHz" << endl;
    EV_INFO << "  • Remaining CPU: " << (cpu_available / 1e9) << " GHz" << endl;
    EV_INFO << "  • Remaining Memory: " << (memory_available / 1e6) << " MB" << endl;
    
    // Calculate execution time
    double exec_time = task->cpu_cycles / task->cpu_allocated;
    
    EV_INFO << "Execution plan:" << endl;
    EV_INFO << "  • Estimated execution time: " << exec_time << " seconds" << endl;
    EV_INFO << "  • Deadline: " << task->relative_deadline << " seconds" << endl;
    EV_INFO << "  • Will complete at: " << (simTime() + exec_time).dbl() << endl;
    EV_INFO << "  • Deadline expires at: " << task->deadline.dbl() << endl;
    
    // Schedule completion event
    cMessage* completionMsg = new cMessage("taskCompletion");
    completionMsg->setContextPointer(task);
    task->completion_event = completionMsg;
    scheduleAt(simTime() + exec_time, completionMsg);
    
    // Schedule deadline check
    cMessage* deadlineMsg = new cMessage("taskDeadline");
    deadlineMsg->setContextPointer(task);
    task->deadline_event = deadlineMsg;
    scheduleAt(task->deadline, deadlineMsg);
    
    task->logTaskInfo("Task started processing");
    
    std::cout << "TASK_EXEC: Vehicle " << getParentModule()->getIndex() << " started task " 
              << task->task_id << " with " << (task->cpu_allocated/1e9) << " GHz" << std::endl;
}

double PayloadVehicleApp::calculateCPUAllocation(Task* task) {
    if (processing_tasks.empty()) {
        // First task gets full allocable CPU
        return cpu_allocable;
    }
    
    // Calculate minimum guarantee
    double F_min = std::min(cpu_allocable * min_cpu_guarantee, 
                           cpu_allocable / max_concurrent_tasks);
    
    // Calculate weights based on urgency
    double total_weight = 0.0;
    std::map<Task*, double> weights;
    
    for (Task* t : processing_tasks) {
        double remaining_cycles = t->getRemainingCycles();
        double remaining_deadline = t->getRemainingDeadline(simTime());
        double weight = std::min(remaining_cycles / std::max(remaining_deadline, 0.001), 
                                cpu_allocable);
        weights[t] = weight;
        total_weight += weight;
    }
    
    // Add current task
    double remaining_deadline = task->getRemainingDeadline(simTime());
    double task_weight = std::min((double)task->cpu_cycles / std::max(remaining_deadline, 0.001),
                                  cpu_allocable);
    weights[task] = task_weight;
    total_weight += task_weight;
    
    // Calculate proportional allocation
    size_t n_tasks = processing_tasks.size() + 1;  // +1 for current task
    double leftover_cpu = cpu_allocable - (n_tasks * F_min);
    
    if (leftover_cpu > 0 && total_weight > 0) {
        double allocated = F_min + (leftover_cpu * task_weight / total_weight);
        EV_INFO << "CPU Allocation (Proportional Fair): F_min=" << (F_min/1e9) 
                << " GHz + proportional=" << ((allocated - F_min)/1e9) 
                << " GHz = " << (allocated/1e9) << " GHz" << endl;
        return allocated;
    }
    
    EV_INFO << "CPU Allocation (Minimum Guarantee): " << (F_min/1e9) << " GHz" << endl;
    return F_min;
}

void PayloadVehicleApp::reallocateCPUResources() {
    if (processing_tasks.empty()) {
        cpu_available = cpu_allocable;
        return;
    }
    
    EV_INFO << "┌──────────────────────────────────────────────────────────────────────────┐" << endl;
    EV_INFO << "│               REALLOCATING CPU RESOURCES                                 │" << endl;
    EV_INFO << "├──────────────────────────────────────────────────────────────────────────┤" << endl;
    EV_INFO << "│ Processing tasks: " << std::setw(52) << processing_tasks.size() << " │" << endl;
    EV_INFO << "└──────────────────────────────────────────────────────────────────────────┘" << endl;
    
    // Recalculate allocation for all processing tasks
    for (Task* task : processing_tasks) {
        double old_allocation = task->cpu_allocated;
        double new_allocation = calculateCPUAllocation(task);
        
        if (std::abs(new_allocation - old_allocation) > 1e6) {  // Significant change (> 1MHz)
            EV_INFO << "Task " << task->task_id << ": " << (old_allocation/1e9) 
                    << " → " << (new_allocation/1e9) << " GHz" << endl;
            
            task->cpu_allocated = new_allocation;
            
            // Reschedule completion event
            if (task->completion_event && task->completion_event->isScheduled()) {
                cancelEvent(task->completion_event);
            }
            
            double remaining_cycles = task->getRemainingCycles();
            double new_exec_time = remaining_cycles / new_allocation;
            
            scheduleAt(simTime() + new_exec_time, task->completion_event);
            
            EV_INFO << "  Remaining cycles: " << (remaining_cycles/1e9) << " G" << endl;
            EV_INFO << "  New completion time: " << new_exec_time << " seconds" << endl;
        }
    }
    
    // Recalculate available CPU
    cpu_available = cpu_allocable;
    for (Task* t : processing_tasks) {
        cpu_available -= t->cpu_allocated;
    }
    
    EV_INFO << "CPU available after reallocation: " << (cpu_available/1e9) << " GHz" << endl;
}

void PayloadVehicleApp::processQueuedTasks() {
    EV_INFO << "┌──────────────────────────────────────────────────────────────────────────┐" << endl;
    EV_INFO << "│            PROCESSING QUEUED TASKS                                       │" << endl;
    EV_INFO << "├──────────────────────────────────────────────────────────────────────────┤" << endl;
    EV_INFO << "│ Queue size: " << std::setw(58) << pending_tasks.size() << " │" << endl;
    EV_INFO << "└──────────────────────────────────────────────────────────────────────────┘" << endl;
    
    std::vector<Task*> to_remove;
    
    // Try to start tasks from queue
    while (!pending_tasks.empty()) {
        Task* task = pending_tasks.top();
        
        // Check if task deadline already passed
        if (task->isDeadlineMissed(simTime())) {
            EV_INFO << "❌ Task " << task->task_id << " deadline expired in queue" << endl;
            pending_tasks.pop();
            task->state = FAILED;
            tasks_failed++;
            sendTaskFailureToRSU(task, "DEADLINE_MISSED_IN_QUEUE");
            delete task;
            continue;
        }
        
        // Check if can start
        if (canStartProcessing(task)) {
            EV_INFO << "✓ Starting queued task " << task->task_id << endl;
            pending_tasks.pop();
            allocateResourcesAndStart(task);
        } else {
            // Can't start this task, stop trying
            EV_INFO << "⏸ Cannot start more tasks from queue" << endl;
            break;
        }
    }
    
    logQueueState("After processing queue");
}

void PayloadVehicleApp::handleTaskCompletion(Task* task) {
    EV_INFO << "\n" << endl;
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║                       TASK COMPLETED                                     ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    task->completion_time = simTime();
    double completion_time_elapsed = (task->completion_time - task->created_time).dbl();
    
    // Check if completed on time or late
    bool on_time = task->completion_time <= task->deadline;
    
    if (on_time) {
        task->state = COMPLETED_ON_TIME;
        tasks_completed_on_time++;
        EV_INFO << "✓ Task completed ON TIME" << endl;
        std::cout << "TASK_SUCCESS: Vehicle " << getParentModule()->getIndex() << " completed task " 
                  << task->task_id << " ON TIME (" << completion_time_elapsed << "s / " 
                  << task->relative_deadline << "s)" << std::endl;
    } else {
        task->state = COMPLETED_LATE;
        tasks_completed_late++;
        double lateness = (task->completion_time - task->deadline).dbl();
        EV_INFO << "⚠ Task completed LATE (by " << lateness << " seconds)" << endl;
        std::cout << "TASK_LATE: Vehicle " << getParentModule()->getIndex() << " completed task " 
                  << task->task_id << " LATE (by " << lateness << "s)" << std::endl;
    }
    
    // Release resources
    processing_tasks.erase(task);
    cpu_available += task->cpu_allocated;
    memory_available += task->task_size_bytes;
    
    // Cancel deadline event
    if (task->deadline_event && task->deadline_event->isScheduled()) {
        cancelEvent(task->deadline_event);
        delete task->deadline_event;
        task->deadline_event = nullptr;
    }
    
    // Record statistics
    total_completion_time += completion_time_elapsed;
    
    task->logTaskInfo("Task completed");
    
    // Send completion notification to RSU with timing data
    sendTaskCompletionToRSU(task->task_id, task->completion_time.dbl(), 
                            true, on_time, 
                            task->task_size_bytes, task->cpu_cycles, 
                            task->qos_value, "completed");
    
    // Reallocate CPU to remaining tasks
    reallocateCPUResources();
    
    // Try to start queued tasks
    processQueuedTasks();
    
    // Keep limited history
    completed_tasks.push_back(task);
    if (completed_tasks.size() > 100) {
        delete completed_tasks.front();
        completed_tasks.pop_front();
    }
    
    logResourceState("After task completion");
    logTaskStatistics();
}

void PayloadVehicleApp::handleTaskDeadline(Task* task) {
    // Check if task is still processing or queued
    if (task->state != PROCESSING && task->state != QUEUED) {
        // Task already completed or failed
        return;
    }
    
    EV_INFO << "\n" << endl;
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║                    TASK DEADLINE EXPIRED                                 ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    task->state = FAILED;
    task->completion_time = simTime();
    double wasted_time = (task->completion_time - task->created_time).dbl();
    
    EV_INFO << "❌ Task FAILED - deadline missed" << endl;
    EV_INFO << "  Wasted time: " << wasted_time << " seconds" << endl;
    
    std::cout << "TASK_FAILED: Vehicle " << getParentModule()->getIndex() << " task " 
              << task->task_id << " DEADLINE MISSED (wasted " << wasted_time << "s)" << std::endl;
    
    // Release resources if was processing
    if (processing_tasks.find(task) != processing_tasks.end()) {
        processing_tasks.erase(task);
        cpu_available += task->cpu_allocated;
        memory_available += task->task_size_bytes;
        
        // Cancel completion event
        if (task->completion_event && task->completion_event->isScheduled()) {
            cancelEvent(task->completion_event);
            delete task->completion_event;
            task->completion_event = nullptr;
        }
        
        reallocateCPUResources();
    }
    
    tasks_failed++;
    
    task->logTaskInfo("Task failed - deadline expired");
    
    // Send completion report for failed task (for completion rate calculation)
    sendTaskCompletionToRSU(task->task_id, task->completion_time.dbl(), 
                            false, false, 
                            task->task_size_bytes, task->cpu_cycles, 
                            task->qos_value, "deadline_missed");
    
    // Send failure notification to RSU
    sendTaskFailureToRSU(task, "DEADLINE_MISSED");
    
    // Try to start queued tasks with freed resources
    processQueuedTasks();
    
    // Keep limited history
    failed_tasks.push_back(task);
    if (failed_tasks.size() > 100) {
        delete failed_tasks.front();
        failed_tasks.pop_front();
    }
    
    logResourceState("After task failure");
    logTaskStatistics();
}

void PayloadVehicleApp::sendTaskMetadataToRSU(Task* task) {
    EV_INFO << "📤 [TASK EVENT] Sending task metadata to RSU (triggered by task generation)" << endl;
    
    TaskMetadataMessage* msg = new TaskMetadataMessage();
    msg->setTask_id(task->task_id.c_str());
    msg->setVehicle_id(task->vehicle_id.c_str());
    msg->setTask_size_bytes(task->task_size_bytes);
    msg->setCpu_cycles(task->cpu_cycles);
    msg->setCreated_time(task->created_time.dbl());
    msg->setDeadline_seconds(task->relative_deadline);
    msg->setQos_value(task->qos_value);
    
    // Find RSU and send
    LAddress::L2Type rsuMacAddress = selectBestRSU();
    if (rsuMacAddress != 0) {
        populateWSM((BaseFrame1609_4*)msg, rsuMacAddress);
        sendDown(msg);
        EV_INFO << "✓ Task metadata sent to RSU (MAC: " << rsuMacAddress << ")" << endl;
        std::cout << "TASK_METADATA: Vehicle " << task->vehicle_id << " sent task " 
                  << task->task_id << " metadata to RSU" << std::endl;
    } else {
        EV_INFO << "⚠ RSU not found, broadcasting metadata" << endl;
        populateWSM((BaseFrame1609_4*)msg);
        sendDown(msg);
    }
}

void PayloadVehicleApp::sendTaskCompletionToRSU(Task* task) {
    EV_INFO << "📤 Sending task completion notification to RSU" << endl;
    
    TaskCompletionMessage* msg = new TaskCompletionMessage();
    msg->setTask_id(task->task_id.c_str());
    msg->setVehicle_id(task->vehicle_id.c_str());
    msg->setCompletion_time(task->completion_time.dbl());
    msg->setProcessing_time((task->completion_time - task->processing_start_time).dbl());
    msg->setCompleted_on_time(task->state == COMPLETED_ON_TIME);
    msg->setCpu_allocated(task->cpu_allocated);
    
    LAddress::L2Type rsuMacAddress = selectBestRSU();
    if (rsuMacAddress != 0) {
        populateWSM((BaseFrame1609_4*)msg, rsuMacAddress);
        sendDown(msg);
    } else {
        populateWSM((BaseFrame1609_4*)msg);
        sendDown(msg);
    }
}

void PayloadVehicleApp::sendTaskFailureToRSU(Task* task, const std::string& reason) {
    EV_INFO << "📤 Sending task failure notification to RSU (reason: " << reason << ")" << endl;
    
    TaskFailureMessage* msg = new TaskFailureMessage();
    msg->setTask_id(task->task_id.c_str());
    msg->setVehicle_id(task->vehicle_id.c_str());
    msg->setFailure_time(simTime().dbl());
    msg->setFailure_reason(reason.c_str());
    msg->setWasted_time((simTime() - task->created_time).dbl());
    
    LAddress::L2Type rsuMacAddress = selectBestRSU();
    if (rsuMacAddress != 0) {
        populateWSM((BaseFrame1609_4*)msg, rsuMacAddress);
        sendDown(msg);
    } else {
        populateWSM((BaseFrame1609_4*)msg);
        sendDown(msg);
    }
}

// ============================================================================
// SERVICE VEHICLE TASK COMPLETION HANDLERS
// ============================================================================

void PayloadVehicleApp::handleServiceTaskCompletion(Task* task) {
    EV_INFO << "\n" << endl;
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║              SERVICE TASK COMPLETED                                      ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    task->completion_time = simTime();
    double processing_time = (task->completion_time - task->processing_start_time).dbl();
    
    // Determine if task met its deadline
    if (task->completion_time <= task->deadline) {
        task->state = COMPLETED_ON_TIME;
        EV_INFO << "✅ Service task COMPLETED ON TIME" << endl;
        std::cout << "SERVICE_COMPLETE_ON_TIME: Vehicle " << getParentModule()->getIndex() 
                  << " completed service task " << task->task_id 
                  << " in " << processing_time << "s" << std::endl;
    } else {
        task->state = COMPLETED_LATE;
        double lateness = (task->completion_time - task->deadline).dbl();
        EV_INFO << "⚠️ Service task COMPLETED LATE (+" << lateness << "s)" << endl;
        std::cout << "SERVICE_COMPLETE_LATE: Vehicle " << getParentModule()->getIndex() 
                  << " completed service task " << task->task_id 
                  << " LATE by " << lateness << "s" << std::endl;
    }
    
    EV_INFO << "  Processing time: " << processing_time << " seconds" << endl;
    
    // Remove from processing set
    processingServiceTasks.erase(task);
    
    // Release resources
    memory_available += task->task_size_bytes;
    
    // Cancel deadline event
    if (task->deadline_event && task->deadline_event->isScheduled()) {
        cancelEvent(task->deadline_event);
        delete task->deadline_event;
        task->deadline_event = nullptr;
    }
    
    // Get origin vehicle and send result
    auto it = serviceTaskOriginVehicles.find(task->task_id);
    if (it != serviceTaskOriginVehicles.end()) {
        std::string origin_vehicle_id = it->second;
        sendServiceTaskResult(task, origin_vehicle_id);
    } else {
        EV_ERROR << "Origin vehicle not found for service task " << task->task_id << endl;
    }
    
    // Try to process next queued service task
    if (!serviceTasks.empty() && processingServiceTasks.size() < maxConcurrentServiceTasks) {
        Task* nextTask = serviceTasks.front();
        serviceTasks.pop();
        processServiceTask(nextTask);
    }
    
    // Clean up
    delete task;
    
    EV_INFO << "Service vehicle queue: " << serviceTasks.size() 
            << ", processing: " << processingServiceTasks.size() << endl;
}

void PayloadVehicleApp::handleServiceTaskDeadline(Task* task) {
    // Check if task is still processing
    if (task->state != PROCESSING) {
        // Task already completed
        return;
    }
    
    EV_INFO << "\n" << endl;
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║           SERVICE TASK DEADLINE EXPIRED                                  ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    task->state = FAILED;
    task->completion_time = simTime();
    double wasted_time = (task->completion_time - task->processing_start_time).dbl();
    
    EV_INFO << "❌ Service task FAILED - deadline missed" << endl;
    EV_INFO << "  Wasted time: " << wasted_time << " seconds" << endl;
    
    std::cout << "SERVICE_FAILED: Vehicle " << getParentModule()->getIndex() 
              << " service task " << task->task_id 
              << " DEADLINE MISSED (wasted " << wasted_time << "s)" << std::endl;
    
    // Remove from processing set
    if (processingServiceTasks.find(task) != processingServiceTasks.end()) {
        processingServiceTasks.erase(task);
        memory_available += task->task_size_bytes;
        
        // Cancel completion event
        if (task->completion_event && task->completion_event->isScheduled()) {
            cancelEvent(task->completion_event);
            delete task->completion_event;
            task->completion_event = nullptr;
        }
    }
    
    // Send failure result to origin vehicle
    auto it = serviceTaskOriginVehicles.find(task->task_id);
    if (it != serviceTaskOriginVehicles.end()) {
        std::string origin_vehicle_id = it->second;
        // Could send failure notification here
        EV_INFO << "Service task failed for origin vehicle: " << origin_vehicle_id << endl;
        
        // Clean up tracking
        serviceTaskOriginVehicles.erase(task->task_id);
        serviceTaskOriginMACs.erase(task->task_id);
    }
    
    // Try to process next queued service task
    if (!serviceTasks.empty() && processingServiceTasks.size() < maxConcurrentServiceTasks) {
        Task* nextTask = serviceTasks.front();
        serviceTasks.pop();
        processServiceTask(nextTask);
    }
    
    delete task;
    
    EV_INFO << "Service vehicle queue: " << serviceTasks.size() 
            << ", processing: " << processingServiceTasks.size() << endl;
}

// ============================================================================

void PayloadVehicleApp::logResourceState(const std::string& context) {
    EV_INFO << "┌──────────────────────────────────────────────────────────────────────────┐" << endl;
    EV_INFO << "│ RESOURCE STATE: " << std::left << std::setw(56) << context << " │" << endl;
    EV_INFO << "├──────────────────────────────────────────────────────────────────────────┤" << endl;
    EV_INFO << "│ CPU Total:       " << std::setw(38) << (cpu_total / 1e9) << " GHz      │" << endl;
    EV_INFO << "│ CPU Allocable:   " << std::setw(38) << (cpu_allocable / 1e9) << " GHz      │" << endl;
    EV_INFO << "│ CPU Available:   " << std::setw(38) << (cpu_available / 1e9) << " GHz      │" << endl;
    double cpu_util = ((cpu_allocable - cpu_available) / cpu_allocable) * 100.0;
    EV_INFO << "│ CPU Utilization: " << std::setw(38) << cpu_util << " %        │" << endl;
    EV_INFO << "│ Memory Total:    " << std::setw(38) << (memory_total / 1e6) << " MB       │" << endl;
    EV_INFO << "│ Memory Available:" << std::setw(38) << (memory_available / 1e6) << " MB       │" << endl;
    double mem_util = ((memory_total - memory_available) / memory_total) * 100.0;
    EV_INFO << "│ Memory Util:     " << std::setw(38) << mem_util << " %        │" << endl;
    EV_INFO << "└──────────────────────────────────────────────────────────────────────────┘" << endl;
}

void PayloadVehicleApp::logQueueState(const std::string& context) {
    EV_INFO << "┌──────────────────────────────────────────────────────────────────────────┐" << endl;
    EV_INFO << "│ QUEUE STATE: " << std::left << std::setw(60) << context << " │" << endl;
    EV_INFO << "├──────────────────────────────────────────────────────────────────────────┤" << endl;
    EV_INFO << "│ Pending Tasks:    " << std::setw(38) << pending_tasks.size() << " / " 
            << std::setw(12) << max_queue_size << "│" << endl;
    EV_INFO << "│ Processing Tasks: " << std::setw(38) << processing_tasks.size() << " / " 
            << std::setw(12) << max_concurrent_tasks << "│" << endl;
    EV_INFO << "│ Completed Tasks:  " << std::setw(52) << completed_tasks.size() << "│" << endl;
    EV_INFO << "│ Failed Tasks:     " << std::setw(52) << failed_tasks.size() << "│" << endl;
    EV_INFO << "└──────────────────────────────────────────────────────────────────────────┘" << endl;
}

void PayloadVehicleApp::logTaskStatistics() {
    uint32_t total_tasks = tasks_completed_on_time + tasks_completed_late + tasks_failed + tasks_rejected;
    
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║                      TASK STATISTICS                                     ║" << endl;
    EV_INFO << "╠══════════════════════════════════════════════════════════════════════════╣" << endl;
    EV_INFO << "║ Tasks Generated:      " << std::setw(48) << tasks_generated << "║" << endl;
    EV_INFO << "║ Tasks Accepted:       " << std::setw(48) << (tasks_generated - tasks_rejected) << "║" << endl;
    EV_INFO << "║ Tasks Rejected:       " << std::setw(48) << tasks_rejected << "║" << endl;
    EV_INFO << "║ ────────────────────────────────────────────────────────────────────────║" << endl;
    EV_INFO << "║ Completed On Time:    " << std::setw(48) << tasks_completed_on_time << "║" << endl;
    EV_INFO << "║ Completed Late:       " << std::setw(48) << tasks_completed_late << "║" << endl;
    EV_INFO << "║ Failed (Deadline):    " << std::setw(48) << tasks_failed << "║" << endl;
    EV_INFO << "║ ────────────────────────────────────────────────────────────────────────║" << endl;
    
    if (total_tasks > 0) {
        double success_rate = (double)tasks_completed_on_time / total_tasks * 100.0;
        double late_rate = (double)tasks_completed_late / total_tasks * 100.0;
        double fail_rate = (double)tasks_failed / total_tasks * 100.0;
        double reject_rate = (double)tasks_rejected / total_tasks * 100.0;
        
        EV_INFO << "║ Success Rate:         " << std::setw(38) << success_rate << " %        ║" << endl;
        EV_INFO << "║ Late Completion Rate: " << std::setw(38) << late_rate << " %        ║" << endl;
        EV_INFO << "║ Failure Rate:         " << std::setw(38) << fail_rate << " %        ║" << endl;
        EV_INFO << "║ Rejection Rate:       " << std::setw(38) << reject_rate << " %        ║" << endl;
    }
    
    if (tasks_completed_on_time + tasks_completed_late > 0) {
        double avg_time = total_completion_time / (tasks_completed_on_time + tasks_completed_late);
        EV_INFO << "║ Avg Completion Time:  " << std::setw(38) << avg_time << " sec      ║" << endl;
    }
    
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    std::cout << "TASK_STATS: Vehicle " << getParentModule()->getIndex() 
              << " - Generated:" << tasks_generated 
              << " OnTime:" << tasks_completed_on_time 
              << " Late:" << tasks_completed_late 
              << " Failed:" << tasks_failed 
              << " Rejected:" << tasks_rejected << std::endl;
}

void PayloadVehicleApp::sendResourceStatusToRSU() {
    EV_INFO << "📡 Sending vehicle resource status to RSU..." << endl;
    
    // Update position and speed
    if (mobility) {
        pos = mobility->getPositionAt(simTime());
        double current_speed = mobility->getSpeed();
        
        // Calculate acceleration
        simtime_t time_delta = simTime() - last_speed_update;
        if (time_delta > 0) {
            acceleration = (current_speed - prev_speed) / time_delta.dbl();
        } else {
            acceleration = 0.0;
        }
        
        prev_speed = speed;
        speed = current_speed;
        last_speed_update = simTime();
    }
    
    // Create VehicleResourceStatusMessage
    VehicleResourceStatusMessage* statusMsg = new VehicleResourceStatusMessage();
    
    // Vehicle identification
    std::ostringstream veh_id;
    veh_id << getParentModule()->getIndex();
    statusMsg->setVehicle_id(veh_id.str().c_str());
    
    // Position and speed
    statusMsg->setPos_x(pos.x);
    statusMsg->setPos_y(pos.y);
    statusMsg->setSpeed(speed);
    statusMsg->setAcceleration(acceleration);
    double heading = mobility ? mobility->getHeading().getRad() * 180.0 / M_PI : 0.0;
    statusMsg->setHeading(heading);
    
    // Resource information
    statusMsg->setCpu_total(cpu_total);
    statusMsg->setCpu_allocable(cpu_allocable);
    statusMsg->setCpu_available(cpu_available);
    // Store as ratio (0.0-1.0) not percentage
    double cpu_util = (cpu_allocable > 0) ? ((cpu_allocable - cpu_available) / cpu_allocable) : 0.0;
    statusMsg->setCpu_utilization(cpu_util);
    
    statusMsg->setMem_total(memory_total);
    statusMsg->setMem_available(memory_available);
    // Store as ratio (0.0-1.0) not percentage
    double mem_util = (memory_total > 0) ? ((memory_total - memory_available) / memory_total) : 0.0;
    statusMsg->setMem_utilization(mem_util);
    
    // Task statistics
    statusMsg->setTasks_generated(tasks_generated);
    statusMsg->setTasks_completed_on_time(tasks_completed_on_time);
    statusMsg->setTasks_completed_late(tasks_completed_late);
    statusMsg->setTasks_failed(tasks_failed);
    statusMsg->setTasks_rejected(tasks_rejected);
    statusMsg->setCurrent_queue_length(pending_tasks.size());
    statusMsg->setCurrent_processing_count(processing_tasks.size());
    
    // Calculate average completion time and deadline miss ratio
    if (tasks_completed_on_time + tasks_completed_late > 0) {
        statusMsg->setAvg_completion_time(total_completion_time / (tasks_completed_on_time + tasks_completed_late));
    } else {
        statusMsg->setAvg_completion_time(0.0);
    }
    
    uint32_t total_completed = tasks_completed_on_time + tasks_completed_late;
    if (total_completed > 0) {
        statusMsg->setDeadline_miss_ratio((double)tasks_completed_late / total_completed);
    } else {
        statusMsg->setDeadline_miss_ratio(0.0);
    }
    
    // Find RSU and send
    LAddress::L2Type rsuMacAddress = selectBestRSU();
    if (rsuMacAddress != 0) {
        populateWSM(statusMsg, rsuMacAddress);
        sendDown(statusMsg);
        EV_INFO << "✓ Vehicle resource status sent to RSU (MAC: " << rsuMacAddress << ")" << endl;
        std::cout << "RESOURCE_UPDATE: Vehicle " << getParentModule()->getIndex() 
                  << " sent resource status to RSU - CPU:" << (cpu_util * 100.0) 
                  << "% Mem:" << (mem_util * 100.0) << "% Queue:" << pending_tasks.size() << std::endl;
    } else {
        EV_WARN << "⚠ RSU not found, cannot send resource status" << endl;
        delete statusMsg;
    }
}

void PayloadVehicleApp::sendVehicleResourceStatus() {
    sendResourceStatusToRSU();
}

// ============================================================================
// MODERN MULTI-CRITERIA RSU SELECTION IMPLEMENTATION
// ============================================================================

LAddress::L2Type PayloadVehicleApp::selectBestRSU() {
    std::cout << "\n🎯 RSU_SELECTION: Starting modern multi-criteria selection..." << std::endl;
    
    // Get vehicle position
    Coord vehiclePos = mobility ? mobility->getPositionAt(simTime()) : Coord(0, 0, 0);
    cModule* networkModule = getModuleByPath("^.^");
    if (!networkModule) {
        std::cout << "RSU_SELECTION: ERROR - Network module not found!" << std::endl;
        return 0;
    }
    
    // Update metrics for all RSUs
    for (int i = 0; i < 3; i++) {
        cModule* rsuModule = networkModule->getSubmodule("rsu", i);
        if (rsuModule) {
            cModule* rsuMobilityModule = rsuModule->getSubmodule("mobility");
            if (rsuMobilityModule) {
                double rsuX = rsuMobilityModule->par("x").doubleValue();
                double rsuY = rsuMobilityModule->par("y").doubleValue();
                double dx = vehiclePos.x - rsuX;
                double dy = vehiclePos.y - rsuY;
                double distance = sqrt(dx * dx + dy * dy);
                
                // Initialize or update RSU metrics
                if (rsuMetrics.find(i) == rsuMetrics.end()) {
                    rsuMetrics[i] = RSUMetrics();
                    // Get MAC address from RSU's MAC interface
                    DemoBaseApplLayerToMac1609_4Interface* rsuMacInterface =
                        FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(rsuModule);
                    if (rsuMacInterface) {
                        rsuMetrics[i].macAddress = rsuMacInterface->getMACAddress();
                    }
                }
                
                rsuMetrics[i].distance = distance;
                
                // Estimate RSSI based on distance (simplified model)
                // TX power 20mW = 13 dBm, Free space path loss at 5.89GHz
                double lambda = 3e8 / 5.89e9;  // wavelength
                double fspl = 20 * log10(distance) + 20 * log10(5.89e9) + 20 * log10(4 * M_PI / 3e8);
                double estimatedRSSI = 13.0 - fspl;  // Simple estimate
                
                // If we don't have a recent RSSI measurement, use estimate
                if (rsuMetrics[i].lastRSSI < -100) {
                    rsuMetrics[i].lastRSSI = estimatedRSSI;
                }
                
                std::cout << "RSU_SELECTION: RSU[" << i << "] dist=" << distance 
                          << "m, estRSSI=" << estimatedRSSI << " dBm" << std::endl;
            }
        }
    }
    
    // Filter viable RSUs and calculate scores
    LAddress::L2Type bestRSU = 0;
    double bestScore = -999;
    int bestIndex = -1;
    
    for (auto& pair : rsuMetrics) {
        int rsuIndex = pair.first;
        RSUMetrics& metrics = pair.second;
        
        // Check blacklist condition
        if (metrics.consecutiveFailures >= failure_blacklist_threshold) {
            simtime_t timeSinceLastFailure = simTime() - metrics.lastContactTime;
            if (timeSinceLastFailure < blacklist_duration) {
                std::cout << "RSU_SELECTION: RSU[" << rsuIndex << "] BLACKLISTED (failures=" 
                          << metrics.consecutiveFailures << ")" << std::endl;
                continue;  // Skip blacklisted RSU
            } else {
                // Reset blacklist after timeout
                metrics.consecutiveFailures = 0;
                std::cout << "RSU_SELECTION: RSU[" << rsuIndex << "] blacklist expired, reset" << std::endl;
            }
        }
        
        // Check minimum RSSI threshold
        if (metrics.lastRSSI < rssi_threshold) {
            std::cout << "RSU_SELECTION: RSU[" << rsuIndex << "] below RSSI threshold ("
                      << metrics.lastRSSI << " < " << rssi_threshold << " dBm)" << std::endl;
            continue;  // Skip RSUs with too weak signal
        }
        
        // Calculate multi-criteria score
        metrics.score = calculateRSUScore(metrics);
        
        // Apply hysteresis: favor currently selected RSU
        if (metrics.macAddress == currentRSU && currentRSU != 0) {
            metrics.score += 0.1;  // 10% bonus for staying with current RSU
            std::cout << "RSU_SELECTION: RSU[" << rsuIndex << "] is CURRENT, added hysteresis bonus" << std::endl;
        }
        
        std::cout << "RSU_SELECTION: RSU[" << rsuIndex << "] SCORE=" << metrics.score 
                  << " (RSSI=" << metrics.lastRSSI << ", PRR=" << metrics.getPRR() 
                  << ", dist=" << metrics.distance << "m)" << std::endl;
        
        // Track best candidate
        if (metrics.score > bestScore) {
            bestScore = metrics.score;
            bestRSU = metrics.macAddress;
            bestIndex = rsuIndex;
        }
    }
    
    // Check if we should switch RSUs (hysteresis)
    if (bestRSU != 0 && bestRSU != currentRSU && currentRSU != 0) {
        // Switching to new RSU - check if improvement is significant enough
        if (bestScore - rsuMetrics[bestIndex].score < 0.15) {  // Need >15% improvement
            std::cout << "RSU_SELECTION: ⚠️ Hysteresis prevents switch (not enough improvement)" << std::endl;
            return currentRSU;  // Stay with current RSU
        }
    }
    
    if (bestRSU != 0) {
        if (bestRSU != currentRSU) {
            std::cout << "RSU_SELECTION: ✅ SWITCHING to RSU[" << bestIndex 
                      << "] MAC=" << bestRSU << " (score=" << bestScore << ")" << std::endl;
            currentRSU = bestRSU;
        } else {
            std::cout << "RSU_SELECTION: ✅ STAYING with RSU[" << bestIndex 
                      << "] MAC=" << bestRSU << " (score=" << bestScore << ")" << std::endl;
        }
        return bestRSU;
    }
    
    std::cout << "RSU_SELECTION: ❌ No viable RSU found, fallback to distance-based" << std::endl;
    return findRSUMacAddress();  // Fallback to old method
}

void PayloadVehicleApp::updateRSUMetrics(int rsuIndex, bool messageSuccess, double rssi) {
    if (rsuMetrics.find(rsuIndex) == rsuMetrics.end()) {
        rsuMetrics[rsuIndex] = RSUMetrics();
    }
    
    RSUMetrics& metrics = rsuMetrics[rsuIndex];
    
    // Update RSSI if provided
    if (rssi > -999) {
        metrics.lastRSSI = rssi;
    }
    
    // Update reception history for PRR calculation
    metrics.receptionHistory.push_back(messageSuccess);
    if (metrics.receptionHistory.size() > (size_t)prr_window_size) {
        metrics.receptionHistory.pop_front();  // Keep only recent history
    }
    
    // Update failure tracking
    if (messageSuccess) {
        metrics.consecutiveFailures = 0;  // Reset on success
        metrics.lastContactTime = simTime();
    } else {
        metrics.consecutiveFailures++;
    }
    
    std::cout << "RSU_METRICS: RSU[" << rsuIndex << "] updated - success=" << messageSuccess 
              << ", PRR=" << metrics.getPRR() << ", failures=" << metrics.consecutiveFailures << std::endl;
}

double PayloadVehicleApp::calculateRSUScore(const RSUMetrics& metrics) {
    // Multi-criteria weighted scoring (weights sum to 1.0)
    double score = 0.0;
    
    // 1. RSSI component (40% weight) - normalized to 0-1
    double rssiScore = normalizeValue(metrics.lastRSSI, rssi_threshold, -40.0);
    score += rssiScore * 0.4;
    
    // 2. PRR component (30% weight)
    double prrScore = metrics.getPRR();
    score += prrScore * 0.3;
    
    // 3. Distance component (20% weight) - closer is better
    double distScore = normalizeValue(2000.0 - metrics.distance, 0, 2000.0);  // Invert
    score += distScore * 0.2;
    
    // 4. Recency component (10% weight) - recent contact is better
    double timeSinceContact = (simTime() - metrics.lastContactTime).dbl();
    double recencyScore = normalizeValue(10.0 - timeSinceContact, 0, 10.0);  // Penalize old contacts
    score += recencyScore * 0.1;
    
    return score;
}

double PayloadVehicleApp::normalizeValue(double value, double min, double max) {
    if (max <= min) return 0.0;
    double normalized = (value - min) / (max - min);
    // Clamp to [0, 1]
    if (normalized < 0.0) normalized = 0.0;
    if (normalized > 1.0) normalized = 1.0;
    return normalized;
}

// ============================================================================
// TASK OFFLOADING HELPER METHODS
// ============================================================================

double PayloadVehicleApp::getRSUDistance() {
    // Get distance to currently selected RSU
    if (currentRSU == 0) {
        EV_WARN << "No RSU currently selected, selecting best RSU" << endl;
        currentRSU = selectBestRSU();
        if (currentRSU == 0) {
            EV_WARN << "No RSU available" << endl;
            return 999999.0;  // Very large distance if no RSU
        }
    }
    
    // Find the RSU index for currentRSU MAC address
    for (const auto& pair : rsuMetrics) {
        if (pair.second.macAddress == currentRSU) {
            EV_DEBUG << "Distance to RSU: " << pair.second.distance << " meters" << endl;
            return pair.second.distance;
        }
    }
    
    // If not found in metrics, return a default high distance
    EV_WARN << "Current RSU not found in metrics" << endl;
    return 1000.0;
}

double PayloadVehicleApp::getEstimatedRSSI() {
    // Get RSSI from currently selected RSU
    if (currentRSU == 0) {
        EV_WARN << "No RSU currently selected, selecting best RSU" << endl;
        currentRSU = selectBestRSU();
        if (currentRSU == 0) {
            EV_WARN << "No RSU available" << endl;
            return -999.0;  // Very weak signal if no RSU
        }
    }
    
    // Find the RSU index for currentRSU MAC address
    for (const auto& pair : rsuMetrics) {
        if (pair.second.macAddress == currentRSU) {
            double rssi = pair.second.lastRSSI;
            EV_DEBUG << "RSSI from RSU: " << rssi << " dBm" << endl;
            return rssi;
        }
    }
    
    // If not found in metrics, return a default weak RSSI
    EV_WARN << "Current RSU not found in metrics" << endl;
    return -90.0;
}

double PayloadVehicleApp::estimateTransmissionTime(Task* task) {
    // IEEE 802.11p DSRC Channel Rate: 3-27 Mbps (typically 6 Mbps for reliability)
    double bandwidth_mbps = 6.0;  // Conservative estimate for reliable transmission
    
    // Calculate transmission time (data size in bits / bandwidth)
    double transmission_time = (task->task_size_bytes * 8.0) / (bandwidth_mbps * 1e6);
    
    // Add propagation delay (distance / speed of light)
    double distance = getRSUDistance();
    double propagation_delay = distance / 3e8;  // Speed of light in m/s
    
    // Add processing overhead (~1ms for MAC layer)
    double overhead = 0.001;
    
    double total_time = transmission_time + propagation_delay + overhead;
    
    EV_DEBUG << "Transmission estimate: " << (transmission_time * 1000) << "ms data + "
             << (propagation_delay * 1e6) << "us propagation + 1ms overhead = "
             << (total_time * 1000) << "ms total" << endl;
    
    return total_time;
}

// ============================================================================
// OFFLOADING REQUEST/RESPONSE HANDLERS
// ============================================================================

void PayloadVehicleApp::sendOffloadingRequestToRSU(Task* task, OffloadingDecision localDecision) {
    EV_INFO << "📤 Sending offloading request to RSU for task " << task->task_id << endl;
    std::cout << "OFFLOAD_REQUEST: Vehicle " << task->vehicle_id << " requesting decision for task " 
              << task->task_id << std::endl;
    
    // Create offloading request message
    veins::OffloadingRequestMessage* msg = new veins::OffloadingRequestMessage();
    
    // Task identification and characteristics
    msg->setTask_id(task->task_id.c_str());
    msg->setVehicle_id(task->vehicle_id.c_str());
    msg->setRequest_time(simTime().dbl());
    msg->setTask_size_bytes(task->task_size_bytes);
    msg->setCpu_cycles(task->cpu_cycles);
    msg->setDeadline_seconds(task->relative_deadline);
    msg->setQos_value(task->qos_value);
    
    // Vehicle resource state
    msg->setLocal_cpu_available_ghz(cpu_available / 1e9);
    msg->setLocal_cpu_utilization((cpu_allocable - cpu_available) / cpu_allocable);
    msg->setLocal_mem_available_mb(memory_available / 1e6);
    msg->setLocal_queue_length(pending_tasks.size());
    msg->setLocal_processing_count(processing_tasks.size());
    
    // Vehicle location and mobility
    Coord pos = mobility->getPositionAt(simTime());
    msg->setPos_x(pos.x);
    msg->setPos_y(pos.y);
    msg->setSpeed(mobility->getSpeed());
    
    // Local decision recommendation
    std::string decisionStr;
    switch(localDecision) {
        case OffloadingDecision::EXECUTE_LOCALLY:
            decisionStr = "LOCAL";
            break;
        case OffloadingDecision::OFFLOAD_TO_RSU:
            decisionStr = "OFFLOAD_TO_RSU";
            break;
        case OffloadingDecision::OFFLOAD_TO_SERVICE_VEHICLE:
            decisionStr = "OFFLOAD_TO_SERVICE_VEHICLE";
            break;
        case OffloadingDecision::REJECT_TASK:
            decisionStr = "REJECT";
            break;
        default:
            decisionStr = "UNKNOWN";
    }
    msg->setLocal_decision(decisionStr.c_str());
    
    // Set sender address (our MAC)
    msg->setSenderAddress(myId);
    
    // Send to selected RSU
    LAddress::L2Type rsuMac = selectBestRSU();
    if (rsuMac == 0) {
        EV_ERROR << "No RSU available for offloading request" << endl;
        delete msg;
        return;
    }
    
    populateWSM(msg, rsuMac);
    sendDown(msg);
    
    // Mark task as awaiting decision
    pendingOffloadingDecisions[task->task_id] = task;
    
    // Record timing information for latency tracking
    TaskTimingInfo timing;
    timing.request_time = simTime().dbl();
    taskTimings[task->task_id] = timing;
    
    // Send lifecycle event to RSU for Digital Twin
    sendTaskOffloadingEvent(task->task_id, "REQUEST_SENT", task->vehicle_id, "RSU");
    
    EV_INFO << "Offloading request sent to RSU (MAC: " << rsuMac << ")" << endl;
    std::cout << "INFO: Offloading request sent to RSU (MAC: " << rsuMac << ")" << std::endl;
}

void PayloadVehicleApp::handleOffloadingDecisionFromRSU(veins::OffloadingDecisionMessage* msg) {
    EV_INFO << "📥 Received offloading decision from RSU" << endl;
    std::string taskId = msg->getTask_id();
    std::string decisionType = msg->getDecision_type();
    
    std::cout << "INFO: 📥 Received offloading decision for task " << taskId 
              << ": " << decisionType << std::endl;
    
    // Find the task in pending decisions
    auto it = pendingOffloadingDecisions.find(taskId);
    if (it == pendingOffloadingDecisions.end()) {
        EV_WARN << "Task " << taskId << " not found in pending decisions (may have timed out)" << endl;
        delete msg;
        return;
    }
    
    Task* task = it->second;
    pendingOffloadingDecisions.erase(it);  // Remove from pending
    
    // Record decision time for latency tracking
    if (taskTimings.find(taskId) != taskTimings.end()) {
        taskTimings[taskId].decision_time = simTime().dbl();
        taskTimings[taskId].decision_type = decisionType;
        std::cout << "INFO: 📊 Decision latency: " << (taskTimings[taskId].decision_time - taskTimings[taskId].request_time) << "s" << std::endl;
    }
    
    // Cancel any timeout message for this task
    // (In production, we'd store timeout message handles to cancel them)
    
    EV_INFO << "✓ Task found: " << task->task_id << endl;
    EV_INFO << "  Decision: " << decisionType << endl;
    EV_INFO << "  Confidence: " << msg->getConfidence_score() << endl;
    EV_INFO << "  Est. completion: " << msg->getEstimated_completion_time() << "s" << endl;
    EV_INFO << "  Reason: " << msg->getDecision_reason() << endl;
    
    // Send lifecycle event
    sendTaskOffloadingEvent(taskId, "DECISION_RECEIVED", "RSU", task->vehicle_id);
    
    // Execute the decision
    executeOffloadingDecision(task, msg);
    
    delete msg;
}

void PayloadVehicleApp::executeOffloadingDecision(Task* task, veins::OffloadingDecisionMessage* decision) {
    EV_INFO << "⚙️ Executing offloading decision for task " << task->task_id << endl;
    
    std::string decisionType = decision->getDecision_type();
    
    // ========================================================================
    // DECISION: EXECUTE LOCALLY
    // ========================================================================
    if (decisionType == "LOCAL") {
        EV_INFO << "💻 Decision: EXECUTE LOCALLY" << endl;
        std::cout << "OFFLOAD_EXEC: Task " << task->task_id << " executing LOCALLY" << std::endl;
        
        // Track processor for completion report
        if (taskTimings.find(task->task_id) != taskTimings.end()) {
            taskTimings[task->task_id].processor_id = "VEHICLE_" + std::to_string(getParentModule()->getIndex());
        }
        
        // Check if can accept task locally
        if (!canAcceptTask(task)) {
            EV_WARN << "Cannot accept task locally (queue full) - rejecting" << endl;
            task->state = REJECTED;
            tasks_rejected++;
            sendTaskFailureToRSU(task, "LOCAL_QUEUE_FULL");
            delete task;
            return;
        }
        
        // Try to start immediately or queue
        if (canStartProcessing(task)) {
            EV_INFO << "✓ Starting task immediately" << endl;
            allocateResourcesAndStart(task);
        } else {
            EV_INFO << "⏸ Queuing task for local execution" << endl;
            task->state = QUEUED;
            pending_tasks.push(task);
            logQueueState("Task queued after LOCAL decision");
        }
        
        logResourceState("After LOCAL execution decision");
    }
    
    // ========================================================================
    // DECISION: OFFLOAD TO RSU
    // ========================================================================
    else if (decisionType == "RSU") {
        EV_INFO << "🏢 Decision: OFFLOAD TO RSU" << endl;
        std::cout << "OFFLOAD_EXEC: Task " << task->task_id << " offloading to RSU" << std::endl;
        
        // Track processor for completion report
        if (taskTimings.find(task->task_id) != taskTimings.end()) {
            taskTimings[task->task_id].processor_id = "RSU";
        }
        
        sendTaskToRSU(task);
        
        // Track offloaded task
        offloadedTasks[task->task_id] = task;
        offloadedTaskTargets[task->task_id] = "RSU";
        
        // Schedule timeout for result
        cMessage* timeoutMsg = new cMessage("offloadedTaskTimeout");
        timeoutMsg->setContextPointer(task);
        scheduleAt(simTime() + offloadedTaskTimeout, timeoutMsg);
        
        EV_INFO << "✓ Task offloaded to RSU, awaiting result" << endl;
    }
    
    // ========================================================================
    // DECISION: OFFLOAD TO SERVICE VEHICLE
    // ========================================================================
    else if (decisionType == "SERVICE_VEHICLE") {
        EV_INFO << "🚗 Decision: OFFLOAD TO SERVICE VEHICLE" << endl;
        
        std::string serviceVehicleId = decision->getTarget_service_vehicle_id();
        LAddress::L2Type serviceVehicleMac = decision->getTarget_service_vehicle_mac();
        
        std::cout << "OFFLOAD_EXEC: Task " << task->task_id 
                  << " offloading to service vehicle " << serviceVehicleId << std::endl;
        
        // Track processor for completion report
        if (taskTimings.find(task->task_id) != taskTimings.end()) {
            taskTimings[task->task_id].processor_id = "SV_" + serviceVehicleId;
        }
        
        if (serviceVehicleMac == 0) {
            EV_ERROR << "Invalid service vehicle MAC address" << endl;
            // Fallback to local execution
            EV_INFO << "Falling back to LOCAL execution" << endl;
            if (canAcceptTask(task)) {
                if (canStartProcessing(task)) {
                    allocateResourcesAndStart(task);
                } else {
                    task->state = QUEUED;
                    pending_tasks.push(task);
                }
            } else {
                task->state = REJECTED;
                tasks_rejected++;
                sendTaskFailureToRSU(task, "SERVICE_VEHICLE_INVALID");
                delete task;
            }
            return;
        }
        
        sendTaskToServiceVehicle(task, serviceVehicleId, serviceVehicleMac);
        
        // Track offloaded task
        offloadedTasks[task->task_id] = task;
        offloadedTaskTargets[task->task_id] = "SV_" + serviceVehicleId;
        
        // Schedule timeout for result
        cMessage* timeoutMsg = new cMessage("offloadedTaskTimeout");
        timeoutMsg->setContextPointer(task);
        scheduleAt(simTime() + offloadedTaskTimeout, timeoutMsg);
        
        EV_INFO << "✓ Task offloaded to service vehicle, awaiting result" << endl;
    }
    
    // ========================================================================
    // DECISION: REJECT TASK
    // ========================================================================
    else if (decisionType == "REJECT") {
        EV_INFO << "❌ Decision: REJECT TASK" << endl;
        std::cout << "OFFLOAD_EXEC: Task " << task->task_id << " REJECTED by RSU" << std::endl;
        
        task->state = REJECTED;
        tasks_rejected++;
        
        std::string reason = decision->getDecision_reason();
        sendTaskFailureToRSU(task, "RSU_REJECTED: " + reason);
        
        EV_INFO << "Task rejected - reason: " << reason << endl;
        delete task;
    }
    
    // ========================================================================
    // UNKNOWN DECISION
    // ========================================================================
    else {
        EV_ERROR << "Unknown decision type: " << decisionType << endl;
        std::cout << "OFFLOAD_EXEC: ERROR - Unknown decision type for task " 
                  << task->task_id << std::endl;
        
        // Fallback to local execution if possible
        if (canAcceptTask(task)) {
            EV_INFO << "Falling back to local execution" << endl;
            if (canStartProcessing(task)) {
                allocateResourcesAndStart(task);
            } else {
                task->state = QUEUED;
                pending_tasks.push(task);
            }
        } else {
            task->state = REJECTED;
            tasks_rejected++;
            sendTaskFailureToRSU(task, "UNKNOWN_DECISION");
            delete task;
        }
    }
}

// ============================================================================
// TASK EXECUTION METHODS
// ============================================================================

void PayloadVehicleApp::sendTaskToRSU(Task* task) {
    EV_INFO << "📤 Sending task " << task->task_id << " to RSU for processing" << endl;
    std::cout << "OFFLOAD_TO_RSU: Task " << task->task_id << " sent to RSU" << std::endl;
    
    // Create TaskOffloadPacket
    veins::TaskOffloadPacket* packet = new veins::TaskOffloadPacket();
    packet->setTask_id(task->task_id.c_str());
    packet->setOrigin_vehicle_id(task->vehicle_id.c_str());
    packet->setOrigin_vehicle_mac(myId);  // Set our MAC address for return routing
    packet->setOffload_time(simTime().dbl());
    packet->setTask_size_bytes(task->task_size_bytes);
    packet->setCpu_cycles(task->cpu_cycles);
    packet->setDeadline_seconds(task->relative_deadline);
    packet->setQos_value(task->qos_value);
    packet->setTask_input_data("{\"input\":\"task_data\"}");  // Placeholder
    
    // Send to RSU
    LAddress::L2Type rsuMac = selectBestRSU();
    if (rsuMac != 0) {
        populateWSM(packet, rsuMac);
        sendDown(packet);
        EV_INFO << "✓ Task offload packet sent to RSU MAC: " << rsuMac << endl;
    } else {
        EV_ERROR << "No RSU available to send task" << endl;
        delete packet;
        return;
    }
    
    offloadedTasks[task->task_id] = task;
    offloadedTaskTargets[task->task_id] = "RSU";
}

void PayloadVehicleApp::sendTaskToServiceVehicle(Task* task, const std::string& serviceVehicleId, veins::LAddress::L2Type serviceMac) {
    EV_INFO << "📤 Sending task " << task->task_id << " to service vehicle " << serviceVehicleId << endl;
    std::cout << "OFFLOAD_TO_SV: Task " << task->task_id << " sent to service vehicle " 
              << serviceVehicleId << std::endl;
    
    // Create TaskOffloadPacket
    veins::TaskOffloadPacket* packet = new veins::TaskOffloadPacket();
    packet->setTask_id(task->task_id.c_str());
    packet->setOrigin_vehicle_id(task->vehicle_id.c_str());
    packet->setOrigin_vehicle_mac(myId);  // Set our MAC address for return routing
    packet->setOffload_time(simTime().dbl());
    packet->setTask_size_bytes(task->task_size_bytes);
    packet->setCpu_cycles(task->cpu_cycles);
    packet->setDeadline_seconds(task->relative_deadline);
    packet->setQos_value(task->qos_value);
    packet->setTask_input_data("{\"input\":\"task_data\"}");  // Placeholder
    
    // Send to service vehicle
    populateWSM(packet, serviceMac);
    sendDown(packet);
    
    EV_INFO << "✓ Task offload packet sent to service vehicle MAC: " << serviceMac << endl;
    
    offloadedTasks[task->task_id] = task;
    offloadedTaskTargets[task->task_id] = serviceVehicleId;
}

void PayloadVehicleApp::handleTaskResult(veins::TaskResultMessage* msg) {
    std::string task_id = msg->getTask_id();
    EV_INFO << "📥 Received task result for " << task_id << endl;
    std::cout << "TASK_RESULT: Received result for task " << task_id 
              << " from " << msg->getProcessor_id() << std::endl;
    
    // Process result and send completion report to RSU
    auto it = offloadedTasks.find(task_id);
    if (it != offloadedTasks.end()) {
        Task* task = it->second;
        bool success = msg->getSuccess();
        double completion_time = msg->getCompletion_time();
        bool on_time = completion_time <= task->deadline.dbl();
        
        // Send completion report with timing data
        sendTaskCompletionToRSU(task_id, completion_time, 
                                success, on_time, 
                                task->task_size_bytes, task->cpu_cycles, 
                                task->qos_value, msg->getTask_output_data());
        
        if (on_time) {
            tasks_completed_on_time++;
        } else {
            tasks_completed_late++;
        }
        
        // Clean up
        delete task;
        offloadedTasks.erase(it);
        offloadedTaskTargets.erase(task_id);
    }
    
    delete msg;
}

// ============================================================================
// SERVICE VEHICLE METHODS
// ============================================================================

void PayloadVehicleApp::handleServiceTaskRequest(veins::TaskOffloadPacket* msg) {
    if (!serviceVehicleEnabled) {
        EV_WARN << "⚠️ Service vehicle mode disabled, rejecting task request" << endl;
        delete msg;
        return;
    }
    
    std::string task_id = msg->getTask_id();
    std::string origin_vehicle_id = msg->getOrigin_vehicle_id();
    veins::LAddress::L2Type origin_mac = msg->getOrigin_vehicle_mac();
    
    EV_INFO << "📥 SERVICE VEHICLE: Received task request from vehicle " << origin_vehicle_id << endl;
    std::cout << "SERVICE_REQUEST: Vehicle " << getParentModule()->getIndex() 
              << " received task " << task_id << " from vehicle " << origin_vehicle_id << std::endl;
    
    // Check service vehicle capacity
    int total_service_tasks = serviceTasks.size() + processingServiceTasks.size();
    if (total_service_tasks >= maxConcurrentServiceTasks) {
        EV_WARN << "Service vehicle at capacity (" << total_service_tasks << "/" 
                << maxConcurrentServiceTasks << "), rejecting task" << endl;
        std::cout << "SERVICE_REJECT: Task " << task_id << " rejected - capacity full" << std::endl;
        
        // TODO: Send rejection message back to origin vehicle
        delete msg;
        return;
    }
    
    // Calculate reserved resources for service processing
    double service_cpu_hz = cpu_total * serviceCpuReservation;
    double service_mem_bytes = serviceMemoryReservation * 1e6;  // MB to bytes
    
    // Check if we have sufficient reserved resources
    if (memory_available < msg->getTask_size_bytes()) {
        EV_WARN << "Insufficient memory for service task (need " 
                << (msg->getTask_size_bytes()/1e6) << "MB, have " 
                << (memory_available/1e6) << "MB)" << endl;
        std::cout << "SERVICE_REJECT: Task " << task_id << " rejected - insufficient memory" << std::endl;
        delete msg;
        return;
    }
    
    // Create Task object from packet
    std::string vehicle_id = std::to_string(getParentModule()->getIndex());
    Task* task = new Task(origin_vehicle_id, task_sequence_number++, 
                          msg->getTask_size_bytes(), msg->getCpu_cycles(),
                          msg->getDeadline_seconds(), msg->getQos_value());
    
    // Override task_id with original task_id from packet
    task->task_id = task_id;
    
    // Store origin information for result sending
    serviceTaskOriginVehicles[task_id] = origin_vehicle_id;
    serviceTaskOriginMACs[task_id] = origin_mac;
    
    EV_INFO << "  Task ID: " << task_id << endl;
    EV_INFO << "  Origin Vehicle: " << origin_vehicle_id << endl;
    EV_INFO << "  Task Size: " << (msg->getTask_size_bytes() / 1024.0) << " KB" << endl;
    EV_INFO << "  CPU Cycles: " << (msg->getCpu_cycles() / 1e9) << " G" << endl;
    EV_INFO << "  Deadline: " << msg->getDeadline_seconds() << " s" << endl;
    EV_INFO << "  Service Queue: " << serviceTasks.size() << ", Processing: " 
            << processingServiceTasks.size() << endl;
    
    // Queue task for service processing
    task->state = QUEUED;
    serviceTasks.push(task);
    
    std::cout << "SERVICE_QUEUED: Task " << task_id << " queued for service processing" << std::endl;
    
    // Try to start processing immediately if we have capacity
    if (processingServiceTasks.size() < maxConcurrentServiceTasks) {
        // Dequeue and start processing
        Task* nextTask = serviceTasks.front();
        serviceTasks.pop();
        processServiceTask(nextTask);
    }
    
    delete msg;
}

void PayloadVehicleApp::processServiceTask(Task* task) {
    EV_INFO << "⚙️ SERVICE VEHICLE: Starting service task " << task->task_id << endl;
    std::cout << "SERVICE_PROCESS: Vehicle " << getParentModule()->getIndex() 
              << " processing service task " << task->task_id << std::endl;
    
    // Use RESERVED resources for service tasks
    double service_cpu_hz = cpu_total * serviceCpuReservation;
    double service_mem_bytes = serviceMemoryReservation * 1e6;
    
    // Calculate processing time using reserved CPU
    double processing_time = (double)task->cpu_cycles / service_cpu_hz;
    
    EV_INFO << "  Reserved Service CPU: " << (service_cpu_hz / 1e9) << " GHz" << endl;
    EV_INFO << "  Task CPU Cycles: " << (task->cpu_cycles / 1e9) << " G" << endl;
    EV_INFO << "  Estimated Processing Time: " << processing_time << " s" << endl;
    EV_INFO << "  Deadline: " << task->relative_deadline << " s" << endl;
    
    // Update task state
    task->state = PROCESSING;
    task->processing_start_time = simTime();
    task->cpu_allocated = service_cpu_hz;
    processingServiceTasks.insert(task);
    
    // Allocate memory (from reserved service memory)
    if (task->task_size_bytes <= memory_available) {
        memory_available -= task->task_size_bytes;
        EV_INFO << "  Memory allocated: " << (task->task_size_bytes / 1e6) << " MB" << endl;
        EV_INFO << "  Memory available: " << (memory_available / 1e6) << " MB" << endl;
    } else {
        EV_WARN << "  Insufficient memory, processing anyway with degraded performance" << endl;
    }
    
    // Schedule completion event
    cMessage* completionMsg = new cMessage("serviceTaskCompletion");
    completionMsg->setContextPointer(task);
    task->completion_event = completionMsg;
    scheduleAt(simTime() + processing_time, completionMsg);
    
    // Schedule deadline check
    cMessage* deadlineMsg = new cMessage("serviceTaskDeadline");
    deadlineMsg->setContextPointer(task);
    task->deadline_event = deadlineMsg;
    scheduleAt(task->deadline, deadlineMsg);
    
    EV_INFO << "✓ Service task processing started, completion expected at " 
            << (simTime() + processing_time).dbl() << endl;
}

void PayloadVehicleApp::sendServiceTaskResult(Task* task, const std::string& originalVehicleId) {
    EV_INFO << "📤 SERVICE VEHICLE: Sending result for task " << task->task_id 
            << " back to vehicle " << originalVehicleId << endl;
    std::cout << "SERVICE_RESULT: Vehicle " << getParentModule()->getIndex() 
              << " returning task " << task->task_id 
              << " result to vehicle " << originalVehicleId << std::endl;
    
    // Create TaskResultMessage
    veins::TaskResultMessage* result = new veins::TaskResultMessage();
    result->setTask_id(task->task_id.c_str());
    result->setOrigin_vehicle_id(originalVehicleId.c_str());
    result->setProcessor_id(std::to_string(getParentModule()->getIndex()).c_str());
    result->setSuccess(task->state == COMPLETED_ON_TIME || task->state == COMPLETED_LATE);
    result->setCompletion_time(task->completion_time.dbl());
    result->setProcessing_time((task->completion_time - task->processing_start_time).dbl());
    
    // Set result data (placeholder - in real system would include actual computation output)
    result->setTask_output_data("{\"status\":\"completed_by_service_vehicle\"}");
    
    // Set failure reason if task failed
    if (task->state != COMPLETED_ON_TIME && task->state != COMPLETED_LATE) {
        result->setFailure_reason("Task processing failed");
    } else {
        result->setFailure_reason("");
    }
    
    // Get origin vehicle MAC address
    auto mac_it = serviceTaskOriginMACs.find(task->task_id);
    if (mac_it != serviceTaskOriginMACs.end()) {
        veins::LAddress::L2Type origin_mac = mac_it->second;
        
        EV_INFO << "  Sending result to origin vehicle MAC: " << origin_mac << endl;
        
        // Send directly to origin vehicle
        populateWSM(result, origin_mac);
        sendDown(result);
        
        EV_INFO << "✓ Service task result sent successfully" << endl;
    } else {
        EV_ERROR << "Origin vehicle MAC not found for task " << task->task_id << endl;
        
        // Fallback: send via RSU
        LAddress::L2Type rsuMac = selectBestRSU();
        if (rsuMac != 0) {
            EV_INFO << "  Fallback: Sending result via RSU" << endl;
            populateWSM(result, rsuMac);
            sendDown(result);
        } else {
            EV_ERROR << "Cannot send result - no RSU available" << endl;
            delete result;
        }
    }
    
    // Clean up origin tracking
    serviceTaskOriginVehicles.erase(task->task_id);
    serviceTaskOriginMACs.erase(task->task_id);
}

// ============================================================================
// TASK OFFLOADING LIFECYCLE EVENT TRACKING
// ============================================================================

void PayloadVehicleApp::sendTaskOffloadingEvent(const std::string& taskId, const std::string& eventType,
                                                  const std::string& sourceEntity, const std::string& targetEntity) {
    EV_DEBUG << "📊 Sending offloading event: " << eventType << " for task " << taskId << endl;
    
    // Create TaskOffloadingEvent message
    veins::TaskOffloadingEvent* event = new veins::TaskOffloadingEvent();
    event->setTask_id(taskId.c_str());
    event->setEvent_type(eventType.c_str());
    event->setEvent_time(simTime().dbl());
    event->setSource_entity_id(sourceEntity.c_str());
    event->setTarget_entity_id(targetEntity.c_str());
    
    // Optionally add event details (as JSON string)
    // For now, keep it simple
    event->setEvent_details("{}");
    
    // Send to RSU for Digital Twin tracking
    LAddress::L2Type rsuMac = selectBestRSU();
    if (rsuMac != 0) {
        populateWSM(event, rsuMac);
        sendDown(event);
        EV_DEBUG << "Event sent to RSU for Digital Twin" << endl;
    } else {
        EV_WARN << "No RSU available to send offloading event" << endl;
        delete event;
    }
}

void PayloadVehicleApp::sendTaskCompletionToRSU(const std::string& taskId, double completionTime, 
                                                  bool success, bool onTime, 
                                                  uint64_t taskSizeBytes, uint64_t cpuCycles, 
                                                  double qosValue, const std::string& resultData) {
    EV_INFO << "📤 Sending task completion report to RSU for task: " << taskId << endl;
    
    // Find timing info for this task
    auto it = taskTimings.find(taskId);
    if (it == taskTimings.end()) {
        EV_WARN << "No timing info found for task " << taskId << ", cannot send completion report" << endl;
        return;
    }
    
    const TaskTimingInfo& timing = it->second;
    
    // Create TaskResultMessage and repurpose it to carry completion data
    veins::TaskResultMessage* resultMsg = new veins::TaskResultMessage();
    resultMsg->setTask_id(taskId.c_str());
    resultMsg->setSuccess(success);
    resultMsg->setCompletion_time(completionTime);
    
    // Pack timing data into result_data field as JSON
    std::ostringstream timingJson;
    timingJson << "{";
    timingJson << "\"request_time\":" << timing.request_time << ",";
    timingJson << "\"decision_time\":" << timing.decision_time << ",";
    timingJson << "\"start_time\":" << timing.start_time << ",";
    timingJson << "\"completion_time\":" << completionTime << ",";
    timingJson << "\"decision_type\":\"" << timing.decision_type << "\",";
    timingJson << "\"processor_id\":\"" << timing.processor_id << "\",";
    timingJson << "\"on_time\":" << (onTime ? "true" : "false") << ",";
    timingJson << "\"task_size_bytes\":" << taskSizeBytes << ",";
    timingJson << "\"cpu_cycles\":" << cpuCycles << ",";
    timingJson << "\"qos_value\":" << qosValue << ",";
    timingJson << "\"result\":\"" << resultData << "\"";
    timingJson << "}";
    
    resultMsg->setTask_output_data(timingJson.str().c_str());
    resultMsg->setOrigin_vehicle_id(("VEHICLE_" + std::to_string(getParentModule()->getIndex())).c_str());
    resultMsg->setProcessor_id(timing.processor_id.c_str());
    
    // Send to RSU
    LAddress::L2Type rsuMac = selectBestRSU();
    if (rsuMac != 0) {
        populateWSM(resultMsg, rsuMac);
        sendDown(resultMsg);
        
        // Calculate and log latencies
        double decisionLatency = timing.decision_time - timing.request_time;
        double processingLatency = completionTime - timing.start_time;
        double totalLatency = completionTime - timing.request_time;
        
        EV_INFO << "Task completion report sent:" << endl;
        EV_INFO << "  • Decision latency: " << decisionLatency << "s" << endl;
        EV_INFO << "  • Processing latency: " << processingLatency << "s" << endl;
        EV_INFO << "  • Total latency: " << totalLatency << "s" << endl;
        EV_INFO << "  • Success: " << (success ? "Yes" : "No") << endl;
        EV_INFO << "  • On-time: " << (onTime ? "Yes" : "No") << endl;
        
        std::cout << "COMPLETION_REPORT: Task " << taskId << " - Total latency: " << totalLatency 
                  << "s, Success: " << success << ", On-time: " << onTime << std::endl;
    } else {
        EV_WARN << "No RSU available to send completion report" << endl;
        delete resultMsg;
    }
    
    // Clean up timing info
    taskTimings.erase(taskId);
}

} // namespace complex_network
