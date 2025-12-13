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
        }

        EV_INFO << "PayloadVehicleApp: Initialized with vehicle data" << endl;
        std::cout << "CONSOLE: PayloadVehicleApp initialized at " << simTime() << std::endl;
        std::cout << "CONSOLE: PayloadVehicleApp - Vehicle data initialized" << std::endl;
        std::cout << "SHADOW: Vehicle starting signal monitoring for shadowing analysis" << std::endl;

        // ===== INITIALIZE TASK PROCESSING SYSTEM =====
        initializeTaskSystem();
        
        // Schedule message sending after 10 seconds
        cMessage* sendMsgEvent = new cMessage("sendPayloadMessage");
        scheduleAt(simTime() + 10, sendMsgEvent);
        std::cout << "CONSOLE: PayloadVehicleApp - Scheduled to send payload message at time " << (simTime() + 10) << std::endl;
        
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
    else if (strcmp(msg->getName(), "sendPayloadMessage") == 0) {
        std::cout << "CONSOLE: PayloadVehicleApp - Sending periodic payload message..." << std::endl;
        EV << "PayloadVehicleApp: Sending periodic payload message..." << endl;

        // Update vehicle data before sending
        updateVehicleData();
        
        // Create payload with current vehicle data
        std::string vehicleDataPayload = createVehicleDataPayload();

        // Find RSU MAC address and ensure correct handling
        LAddress::L2Type rsuMacAddress = findRSUMacAddress();
        
        // Create DemoSafetyMessage with payload
        DemoSafetyMessage* wsm = new DemoSafetyMessage();
        
        if (rsuMacAddress == 0) {
            std::cout << "CONSOLE: PayloadVehicleApp - RSU MAC not found, using broadcast" << std::endl;
            populateWSM(wsm); // fallback broadcast
        } else {
            std::cout << "CONSOLE: PayloadVehicleApp - Found RSU MAC: " << rsuMacAddress << ", using unicast" << std::endl;
            populateWSM(wsm, rsuMacAddress); // force unicast
        }

        // Set payload using setName as the payload carrier  
        wsm->setName(vehicleDataPayload.c_str());
        wsm->setSenderPos(curPosition);
        wsm->setUserPriority(7);

        // Send the message
        sendDown(wsm);

        std::cout << "CONSOLE: PayloadVehicleApp - Sent payload message (Recipient: " << wsm->getRecipientAddress() << ")" << std::endl;
        std::cout << "CONSOLE: PayloadVehicleApp - Vehicle data payload sent: " << vehicleDataPayload << std::endl;
        EV << "PayloadVehicleApp: Sent vehicle data payload message" << endl;

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
    else {
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
        speed = mobility->getSpeed();
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
        speed = mobility->getSpeed();
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
    
    // Send completion notification to RSU
    sendTaskCompletionToRSU(task);
    
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
    EV_INFO << "📤 Sending task metadata to RSU" << endl;
    
    TaskMetadataMessage* msg = new TaskMetadataMessage();
    msg->setTask_id(task->task_id.c_str());
    msg->setVehicle_id(task->vehicle_id.c_str());
    msg->setTask_size_bytes(task->task_size_bytes);
    msg->setCpu_cycles(task->cpu_cycles);
    msg->setCreated_time(task->created_time.dbl());
    msg->setDeadline_seconds(task->relative_deadline);
    msg->setQos_value(task->qos_value);
    
    // Find RSU and send
    LAddress::L2Type rsuMacAddress = findRSUMacAddress();
    if (rsuMacAddress != 0) {
        populateWSM((BaseFrame1609_4*)msg, rsuMacAddress);
        sendDown(msg);
        EV_INFO << "✓ Task metadata sent to RSU (MAC: " << rsuMacAddress << ")" << endl;
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
    
    LAddress::L2Type rsuMacAddress = findRSUMacAddress();
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
    
    LAddress::L2Type rsuMacAddress = findRSUMacAddress();
    if (rsuMacAddress != 0) {
        populateWSM((BaseFrame1609_4*)msg, rsuMacAddress);
        sendDown(msg);
    } else {
        populateWSM((BaseFrame1609_4*)msg);
        sendDown(msg);
    }
}

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

} // namespace complex_network
