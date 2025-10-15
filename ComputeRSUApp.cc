#include "ComputeRSUApp.h"
#include "veins/modules/messages/DemoSafetyMessage_m.h"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace complex_network {

Define_Module(ComputeRSUApp);

void ComputeRSUApp::initialize(int stage) {
    DemoBaseApplLayer::initialize(stage);
    
    if (stage == 0) {
        // ==================== CPU & Compute Parameters ====================
        totalCpuFreqHz = par("totalCpuFreqHz").doubleValue();
        availableCpuFreqHz = totalCpuFreqHz;
        currentTaskQueueSize = 0;
        currentProcessingLoad = 0.0;
        
        // ==================== Communication Parameters ====================
        transmitPowerW = par("transmitPowerW").doubleValue();
        totalBandwidthHz = par("totalBandwidthHz").doubleValue();
        availableBandwidthHz = totalBandwidthHz;
        
        // ==================== Energy Parameters ====================
        energyIdle_mW = par("energyIdle_mW").doubleValue();
        energyTx_mW = par("energyTx_mW").doubleValue();
        energyRx_mW = par("energyRx_mW").doubleValue();
        energyCompute_mW_per_GHz = par("energyCompute_mW_per_GHz").doubleValue();
        totalEnergyConsumed_J = 0.0;
        lastEnergyUpdateTime = simTime();
        currentEnergyState = IDLE;
        
        // ==================== Intervals ====================
        heartbeatInterval = par("heartbeatInterval").doubleValue();
        energyUpdateInterval = par("energyUpdateInterval").doubleValue();
        
        // ==================== Position (Static) ====================
        cModule* mobilityModule = getParentModule()->getSubmodule("mobility");
        if (mobilityModule) {
            double x = mobilityModule->par("x").doubleValue();
            double y = mobilityModule->par("y").doubleValue();
            position = Coord(x, y);
        }
        
        // ==================== Statistics Signals ====================
        sigCpuUtilization = registerSignal("rsuCpuUtilization");
        sigQueueSize = registerSignal("rsuQueueSize");
        sigEnergyConsumed = registerSignal("rsuEnergyConsumed");
        sigProcessingDelay = registerSignal("rsuProcessingDelay");
        sigBandwidthUtilization = registerSignal("rsuBandwidthUtilization");
        
        // ==================== Timer Setup ====================
        heartbeatMsg = new cMessage("rsuHeartbeat");
        energyUpdateMsg = new cMessage("rsuEnergyUpdate");
        
        scheduleAt(simTime() + uniform(0, heartbeatInterval), heartbeatMsg);
        scheduleAt(simTime() + energyUpdateInterval, energyUpdateMsg);
        
        EV_INFO << "ComputeRSU[" << getParentModule()->getIndex() 
                << "] initialized at (" << position.x << "," << position.y << ")"
                << " with CPU=" << (totalCpuFreqHz/1e9) << " GHz, "
                << "TxPower=" << transmitPowerW << " W, "
                << "Bandwidth=" << (totalBandwidthHz/1e6) << " MHz" << endl;
    }
    else if (stage == 1) {
        // Get MAC address in stage 1
        DemoBaseApplLayerToMac1609_4Interface* macInterface =
            FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(getParentModule());

        if (macInterface) {
            myMacAddress = macInterface->getMACAddress();
            EV_INFO << "RSU[" << getParentModule()->getIndex() << "] MAC address: " << myMacAddress << endl;
            std::cout << "CONSOLE: RSU[" << getParentModule()->getIndex() << "] MAC: " << myMacAddress << std::endl;
        } else {
            std::cout << "CONSOLE: ERROR - RSU[" << getParentModule()->getIndex() << "] MAC interface NOT found!" << std::endl;
            myMacAddress = 0;
        }
        
        // If this is RSU[1], schedule periodic DT updates to RSU[0]
        if (getParentModule()->getIndex() == 1) {
            std::cout << "CONSOLE: RSU[1] will send periodic DT updates to RSU[0]" << std::endl;
        }
    }
}

void ComputeRSUApp::finish() {
    DemoBaseApplLayer::finish();
    
    // Record final statistics
    recordScalar("RSU.totalEnergyConsumed_J", totalEnergyConsumed_J);
    recordScalar("RSU.avgCpuUtilization", currentProcessingLoad);
    recordScalar("RSU.totalTasksProcessed", totalTasksProcessed);
    recordScalar("RSU.totalCpuFreqGHz", totalCpuFreqHz / 1e9);
    recordScalar("RSU.position.x", position.x);
    recordScalar("RSU.position.y", position.y);
    
    // Cleanup
    cancelAndDelete(heartbeatMsg);
    cancelAndDelete(energyUpdateMsg);
}

void ComputeRSUApp::handleSelfMsg(cMessage* msg) {
    if (msg == heartbeatMsg) {
        sendHeartbeat();
        scheduleAt(simTime() + heartbeatInterval, heartbeatMsg);
        return;
    }
    
    if (msg == energyUpdateMsg) {
        updateEnergyConsumption();
        scheduleAt(simTime() + energyUpdateInterval, energyUpdateMsg);
        return;
    }
    
    DemoBaseApplLayer::handleSelfMsg(msg);
}

void ComputeRSUApp::handleMessage(cMessage* msg) {
    EV_INFO << "RSU[" << getParentModule()->getIndex() 
            << "] handleMessage() called with: " << msg->getName() << endl;
    
    BaseFrame1609_4* wsm = dynamic_cast<BaseFrame1609_4*>(msg);
    if (wsm) {
        EV_INFO << "RSU[" << getParentModule()->getIndex() 
                << "] Received BaseFrame1609_4, routing to onWSM()" << endl;
        onWSM(wsm);
        return;
    }
    
    DemoBaseApplLayer::handleMessage(msg);
}

void ComputeRSUApp::sendHeartbeat() {
    // Calculate metrics
    double cpuUtilization = 1.0 - (availableCpuFreqHz / totalCpuFreqHz);
    double bandwidthUtilization = 1.0 - (availableBandwidthHz / totalBandwidthHz);
    double expectedDelay = calculateExpectedDelay();
    
    EV_INFO << "RSU[" << getParentModule()->getIndex() << "] Heartbeat @ t=" << simTime()
            << " | Pos=(" << position.x << "," << position.y << ")"
            << " | CPU Util=" << (cpuUtilization*100) << "%"
            << " | BW Util=" << (bandwidthUtilization*100) << "%"
            << " | Queue=" << currentTaskQueueSize
            << " | Vehicles in range=" << vehiclesInRange.size()
            << " | E=" << totalEnergyConsumed_J << " J"
            << " | ExpDelay=" << expectedDelay << " s" << endl;
    
    // Record metrics
    recordMetrics();
    
    // Emit statistics
    emit(sigCpuUtilization, cpuUtilization);
    emit(sigQueueSize, currentTaskQueueSize);
    emit(sigEnergyConsumed, totalEnergyConsumed_J);
    emit(sigProcessingDelay, expectedDelay);
    emit(sigBandwidthUtilization, bandwidthUtilization);
    
    // ==================== RSU[1] sends DT update to RSU[0] ====================
    if (getParentModule()->getIndex() == 1) {
        sendRSUStateToRSU0();
    }
    
    // ==================== RSU[0] logs DT inventory ====================
    if (getParentModule()->getIndex() == 0) {
        std::cout << "CONSOLE: ==================== RSU[0] Digital Twin Inventory ====================" << std::endl;
        std::cout << "CONSOLE: Total Digital Twins: " << digitalTwins.size() << std::endl;
        for (const auto& pair : digitalTwins) {
            const DigitalTwin& dt = pair.second;
            std::cout << "CONSOLE:   - " << dt.nodeType << "[" << dt.nodeId << "] "
                      << "@ (" << dt.posX << "," << dt.posY << ") "
                      << "vel=" << dt.velocity << " m/s "
                      << "lastUpdate=" << dt.lastUpdateTime << std::endl;
        }
        std::cout << "CONSOLE: =======================================================================" << std::endl;
    }
}

void ComputeRSUApp::updateEnergyConsumption() {
    simtime_t dt = simTime() - lastEnergyUpdateTime;
    if (dt <= 0) return;
    
    double powerConsumption_mW = 0.0;
    
    switch (currentEnergyState) {
        case IDLE:
            powerConsumption_mW = energyIdle_mW;
            break;
        case TX:
            powerConsumption_mW = energyTx_mW;
            break;
        case RX:
            powerConsumption_mW = energyRx_mW;
            break;
        case COMPUTE:
            // Power = base compute + utilization-based scaling
            powerConsumption_mW = energyCompute_mW_per_GHz * (totalCpuFreqHz / 1e9) * 
                                  (1.0 - availableCpuFreqHz / totalCpuFreqHz);
            break;
    }
    
    // Energy = Power * Time
    double energyDelta_J = (powerConsumption_mW / 1000.0) * dt.dbl();
    totalEnergyConsumed_J += energyDelta_J;
    
    lastEnergyUpdateTime = simTime();
}

double ComputeRSUApp::calculateChannelGain(double distance, Coord vehiclePos) {
    if (distance <= 0) distance = 0.1;
    
    // Free-space path loss model for 5.9 GHz
    double frequency_GHz = 5.9;
    double pathLoss_dB = 20 * log10(distance) + 20 * log10(frequency_GHz) + 32.44;
    double channelGain_dB = -pathLoss_dB;
    
    // Add shadowing
    double shadowFading_dB = normal(0, 3.0); // 3 dB std deviation (less than V2V)
    
    return channelGain_dB + shadowFading_dB;
}

double ComputeRSUApp::calculateExpectedDelay() {
    // M/M/1 queueing model estimation
    double avgTaskCycles = 1e8;  // 100M cycles per task
    double serviceRate = availableCpuFreqHz / avgTaskCycles;
    double arrivalRate = currentTaskQueueSize / (heartbeatInterval + 0.01);
    
    double processingDelay = 0.0;
    if (serviceRate > arrivalRate && serviceRate > 0) {
        processingDelay = 1.0 / (serviceRate - arrivalRate);
    } else {
        processingDelay = currentTaskQueueSize * (avgTaskCycles / availableCpuFreqHz);
    }
    
    double queueingDelay = currentTaskQueueSize * 0.01; // 10ms per queued task
    
    return processingDelay + queueingDelay;
}

void ComputeRSUApp::onWSM(BaseFrame1609_4* wsm) {
    setEnergyState(RX);
    
    DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
    if (dsm) {
        std::string payload = dsm->getName();
        
        // ==================== Check for Digital Twin Update ====================
        if (payload.find("DT_UPDATE") != std::string::npos) {
            EV_INFO << "RSU[" << getParentModule()->getIndex() 
                    << "] Received DT update: " << payload << endl;
            std::cout << "CONSOLE: RSU[" << getParentModule()->getIndex() 
                      << "] <- DT Update: " << payload << std::endl;
            
            processDTUpdate(wsm);
        }
        // Check if this is a task offload request
        else if (payload.find("TASK_OFFLOAD") != std::string::npos) {
            EV_INFO << "RSU[" << getParentModule()->getIndex() 
                    << "] Received task offload request: " << payload << endl;
            
            processTaskOffloadRequest(wsm);
        } else {
            EV_INFO << "RSU[" << getParentModule()->getIndex() 
                    << "] Received message" << endl;
        }
        
        // Track vehicle in range
        Coord senderPos = dsm->getSenderPos();
        double distance = position.distance(senderPos);
        
        // Use sender position as key (convert to int for map)
        int vehicleKey = (int)(senderPos.x * 1000 + senderPos.y);
        
        VehicleInfo vInfo;
        vInfo.nodeIndex = -1; // Unknown index from message
        vInfo.lastKnownPosition = senderPos;
        vInfo.distance = distance;
        vInfo.channelGain_dB = calculateChannelGain(distance, senderPos);
        vInfo.rssi_dBm = 10 * log10(transmitPowerW * 1000) + vInfo.channelGain_dB;
        vInfo.lastSeen = simTime();
        
        vehiclesInRange[vehicleKey] = vInfo;
    }
    
    DemoBaseApplLayer::onWSM(wsm);
    setEnergyState(IDLE);
}

void ComputeRSUApp::processTaskOffloadRequest(BaseFrame1609_4* wsm) {
    setEnergyState(COMPUTE);
    
    currentTaskQueueSize++;
    totalTasksProcessed++;
    
    // Simulate task processing
    double processingDelay = calculateExpectedDelay();
    
    EV_INFO << "RSU[" << getParentModule()->getIndex() 
            << "] Processing task, expected delay: " << processingDelay << " s" << endl;
    
    // Get sender's MAC address for unicast response
    LAddress::L2Type senderMac = 0;
    
    // Extract sender MAC from payload (sent by TaskVehicleApp)
    DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
    if (dsm) {
        std::string payload = dsm->getName();
        size_t macPos = payload.find("SenderMAC:");
        if (macPos != std::string::npos) {
            std::string macStr = payload.substr(macPos + 10); // Skip "SenderMAC:"
            senderMac = std::stoi(macStr);
            EV_INFO << "RSU[" << getParentModule()->getIndex() 
                    << "] Extracted sender MAC from payload: " << senderMac << endl;
        }
    }
    
    // Fallback: Try to get from sender module
    if (senderMac == 0) {
        cModule* senderModule = wsm->getSenderModule();
        if (senderModule) {
            // Get parent node module
            cModule* senderNode = senderModule->getParentModule();
            if (senderNode) {
                DemoBaseApplLayerToMac1609_4Interface* senderMacInterface =
                    FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(senderNode);
                if (senderMacInterface) {
                    senderMac = senderMacInterface->getMACAddress();
                    EV_INFO << "RSU[" << getParentModule()->getIndex() 
                            << "] Got sender MAC from module: " << senderMac << endl;
                }
            }
        }
    }
    
    setEnergyState(TX);
    
    // Create response message
    DemoSafetyMessage* response = new DemoSafetyMessage("TaskOffloadResponse");
    
    // Send UNICAST response to the sender
    if (senderMac != 0 && senderMac != LAddress::L2BROADCAST()) {
        populateWSM(response, senderMac);
        EV_INFO << "RSU[" << getParentModule()->getIndex() 
                << "] Sending UNICAST response to MAC: " << senderMac << endl;
    } else {
        populateWSM(response);
        EV_WARN << "RSU[" << getParentModule()->getIndex() 
                << "] Sender MAC not found, using BROADCAST" << endl;
    }
    
    std::ostringstream payload;
    payload << "TASK_RESULT|RSU:" << getParentModule()->getIndex()
            << "|ProcessingTime:" << processingDelay;
    response->setName(payload.str().c_str());
    response->setSenderPos(position);
    response->setUserPriority(7);
    
    sendDown(response);
    
    EV_INFO << "RSU[" << getParentModule()->getIndex() 
            << "] Sent task result response" << endl;
    
    currentTaskQueueSize--;
    setEnergyState(IDLE);
}

void ComputeRSUApp::setEnergyState(EnergyState newState) {
    if (currentEnergyState != newState) {
        updateEnergyConsumption();
        currentEnergyState = newState;
    }
}

void ComputeRSUApp::recordMetrics() {
    recordScalar("RSU.position.x", position.x);
    recordScalar("RSU.position.y", position.y);
    recordScalar("RSU.availableCpuFreqHz", availableCpuFreqHz);
    recordScalar("RSU.cpuUtilization", 1.0 - (availableCpuFreqHz / totalCpuFreqHz));
    recordScalar("RSU.bandwidthUtilization", 1.0 - (availableBandwidthHz / totalBandwidthHz));
    recordScalar("RSU.queueSize", currentTaskQueueSize);
    recordScalar("RSU.vehiclesInRange", (double)vehiclesInRange.size());
    recordScalar("RSU.transmitPowerW", transmitPowerW);
    recordScalar("RSU.energyConsumed_J", totalEnergyConsumed_J);
    recordScalar("RSU.expectedDelay", calculateExpectedDelay());
}

// ==================== Digital Twin Functions ====================

void ComputeRSUApp::processDTUpdate(BaseFrame1609_4* wsm) {
    DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
    if (!dsm) return;
    
    std::string payload = dsm->getName();
    
    // Parse payload: DT_UPDATE|Type:XX|NodeID:XX|...
    DigitalTwin dt;
    dt.lastUpdateTime = simTime();
    
    // Extract fields from payload
    size_t pos = 0;
    std::string token;
    std::istringstream ss(payload);
    
    while (std::getline(ss, token, '|')) {
        size_t colonPos = token.find(':');
        if (colonPos != std::string::npos) {
            std::string key = token.substr(0, colonPos);
            std::string value = token.substr(colonPos + 1);
            
            if (key == "Type") dt.nodeType = value;
            else if (key == "NodeID") dt.nodeId = std::stoi(value);
            else if (key == "PosX") dt.posX = std::stod(value);
            else if (key == "PosY") dt.posY = std::stod(value);
            else if (key == "Velocity") dt.velocity = std::stod(value);
            else if (key == "MAC") dt.macAddress = std::stoi(value);
            else {
                // Store other attributes flexibly
                try {
                    dt.attributes[key] = std::stod(value);
                } catch (...) {
                    // Ignore non-numeric attributes
                }
            }
        }
    }
    
    // Store in DT map
    digitalTwins[dt.nodeId] = dt;
    
    EV_INFO << "RSU[" << getParentModule()->getIndex() << "] Updated DT for " 
            << dt.nodeType << "[" << dt.nodeId << "] @ (" << dt.posX << "," << dt.posY << ")" << endl;
}

std::string ComputeRSUApp::createRSUDTUpdatePayload() {
    // Create structured DT update payload for RSU state
    std::ostringstream payload;
    
    double cpuUtilization = 1.0 - (availableCpuFreqHz / totalCpuFreqHz);
    double bandwidthUtilization = 1.0 - (availableBandwidthHz / totalBandwidthHz);
    
    payload << "DT_UPDATE|"
            << "Type:RSU|"
            << "NodeID:" << getParentModule()->getIndex() << "|"
            << "Time:" << std::fixed << std::setprecision(3) << simTime().dbl() << "|"
            << "PosX:" << std::fixed << std::setprecision(2) << position.x << "|"
            << "PosY:" << std::fixed << std::setprecision(2) << position.y << "|"
            << "Velocity:0.0|"  // RSU is stationary
            << "MAC:" << myMacAddress << "|"
            << "CPUFreqHz:" << std::fixed << std::setprecision(0) << totalCpuFreqHz << "|"
            << "CPUUtil:" << std::fixed << std::setprecision(2) << cpuUtilization << "|"
            << "BWUtil:" << std::fixed << std::setprecision(2) << bandwidthUtilization << "|"
            << "QueueSize:" << currentTaskQueueSize << "|"
            << "EnergyJ:" << std::fixed << std::setprecision(3) << totalEnergyConsumed_J << "|"
            << "TxPowerW:" << std::fixed << std::setprecision(2) << transmitPowerW << "|"
            << "VehiclesInRange:" << vehiclesInRange.size();
    
    return payload.str();
}

LAddress::L2Type ComputeRSUApp::findRSU0MacAddress() {
    // Get the network module
    cModule* networkModule = getModuleByPath("^.^");
    if (!networkModule) {
        EV_WARN << "RSU[" << getParentModule()->getIndex() << "] Network module not found!" << endl;
        return 0;
    }

    // Find RSU[0] (Digital Twin host)
    cModule* rsuModule = networkModule->getSubmodule("rsu", 0);
    if (rsuModule) {
        EV_INFO << "RSU[" << getParentModule()->getIndex() << "] Found RSU[0]: " << rsuModule->getFullName() << endl;

        // Get the MAC interface from RSU[0]
        DemoBaseApplLayerToMac1609_4Interface* rsuMacInterface =
            FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(rsuModule);

        if (rsuMacInterface) {
            LAddress::L2Type rsuMacAddress = rsuMacInterface->getMACAddress();
            if (rsuMacAddress != 0) {
                EV_INFO << "RSU[" << getParentModule()->getIndex() << "] Found RSU[0] MAC: " << rsuMacAddress << endl;
                return rsuMacAddress;
            }
        }
    }

    EV_WARN << "RSU[" << getParentModule()->getIndex() << "] RSU[0] MAC address not found!" << endl;
    return 0;
}

void ComputeRSUApp::sendRSUStateToRSU0() {
    // Create DT update payload
    std::string dtPayload = createRSUDTUpdatePayload();
    
    // Find RSU[0] MAC address
    LAddress::L2Type rsu0Mac = findRSU0MacAddress();
    
    if (rsu0Mac == 0) {
        EV_WARN << "RSU[" << getParentModule()->getIndex() << "] Cannot send DT update - RSU[0] MAC not found" << endl;
        return;
    }
    
    // Create DemoSafetyMessage with DT payload
    DemoSafetyMessage* dtMsg = new DemoSafetyMessage();
    populateWSM(dtMsg, rsu0Mac); // Unicast to RSU[0]
    dtMsg->setName(dtPayload.c_str());
    dtMsg->setSenderPos(position);
    dtMsg->setUserPriority(6); // High priority for DT updates
    
    // Send to RSU[0]
    sendDown(dtMsg);
    
    EV_INFO << "RSU[" << getParentModule()->getIndex() << "] Sent DT update to RSU[0] (MAC: " << rsu0Mac << ")" << endl;
    std::cout << "CONSOLE: RSU[" << getParentModule()->getIndex() << "] -> RSU[0] DT Update: " << dtPayload << std::endl;
}

} // namespace complex_network
