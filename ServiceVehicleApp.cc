#include "ServiceVehicleApp.h"
#include "veins/modules/messages/DemoSafetyMessage_m.h"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace complex_network {

Define_Module(ServiceVehicleApp);

void ServiceVehicleApp::initialize(int stage) {
    DemoBaseApplLayer::initialize(stage);
    
    if (stage == 0) {
        // ==================== CPU & Compute Parameters ====================
        totalCpuFreqHz = par("totalCpuFreqHz").doubleValue();
        availableCpuFreqHz = totalCpuFreqHz;
        currentTaskQueueSize = 0;
        currentProcessingLoad = 0.0;
        
        // ==================== Communication Parameters ====================
        transmitPowerW = par("transmitPowerW").doubleValue();
        availableBandwidthHz = par("availableBandwidthHz").doubleValue();
        v2vCommRange = par("v2vCommRange").doubleValue();
        
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
        discoveryInterval = par("discoveryInterval").doubleValue();
        energyUpdateInterval = par("energyUpdateInterval").doubleValue();
        
        // ==================== Mobility Setup ====================
        mobility = FindModule<TraCIMobility*>::findSubModule(getParentModule());
        if (mobility) {
            subscribe(mobility->mobilityStateChangedSignal, this);
            position = mobility->getPositionAt(simTime());
            velocity = mobility->getSpeed();
        }
        
        // ==================== Statistics Signals ====================
        sigCpuUtilization = registerSignal("svCpuUtilization");
        sigQueueSize = registerSignal("svQueueSize");
        sigEnergyConsumed = registerSignal("svEnergyConsumed");
        sigProcessingDelay = registerSignal("svProcessingDelay");
        
        // ==================== Timer Setup ====================
        heartbeatMsg = new cMessage("svHeartbeat");
        neighborDiscoveryMsg = new cMessage("svDiscovery");
        energyUpdateMsg = new cMessage("svEnergyUpdate");
        
        scheduleAt(simTime() + uniform(0, heartbeatInterval), heartbeatMsg);
        scheduleAt(simTime() + uniform(0, discoveryInterval), neighborDiscoveryMsg);
        scheduleAt(simTime() + energyUpdateInterval, energyUpdateMsg);
        
        EV_INFO << "ServiceVehicle[" << getParentModule()->getIndex() 
                << "] initialized with CPU=" << (totalCpuFreqHz/1e9) << " GHz, "
                << "TxPower=" << transmitPowerW << " W, "
                << "Bandwidth=" << (availableBandwidthHz/1e6) << " MHz" << endl;
    }
    else if (stage == 1) {
        // Get MAC address in stage 1 (for DT updates)
        DemoBaseApplLayerToMac1609_4Interface* macInterface =
            FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(getParentModule());

        if (macInterface) {
            myMacAddress = macInterface->getMACAddress();
            EV_INFO << "SV[" << getParentModule()->getIndex() << "] MAC address: " << myMacAddress << endl;
            std::cout << "CONSOLE: SV[" << getParentModule()->getIndex() << "] MAC: " << myMacAddress << std::endl;
        } else {
            std::cout << "CONSOLE: ERROR - SV[" << getParentModule()->getIndex() << "] MAC interface NOT found!" << std::endl;
            myMacAddress = 0;
        }
    }
}

void ServiceVehicleApp::finish() {
    DemoBaseApplLayer::finish();
    
    // Record final statistics
    recordScalar("SV.totalEnergyConsumed_J", totalEnergyConsumed_J);
    recordScalar("SV.avgCpuUtilization", currentProcessingLoad);
    recordScalar("SV.totalCpuFreqGHz", totalCpuFreqHz / 1e9);
    
    // Cleanup
    cancelAndDelete(heartbeatMsg);
    cancelAndDelete(neighborDiscoveryMsg);
    cancelAndDelete(energyUpdateMsg);
}

void ServiceVehicleApp::handleSelfMsg(cMessage* msg) {
    if (msg == heartbeatMsg) {
        sendHeartbeat();
        scheduleAt(simTime() + heartbeatInterval, heartbeatMsg);
        return;
    }
    
    if (msg == neighborDiscoveryMsg) {
        discoverTaskVehicles();
        scheduleAt(simTime() + discoveryInterval, neighborDiscoveryMsg);
        return;
    }
    
    if (msg == energyUpdateMsg) {
        updateEnergyConsumption();
        scheduleAt(simTime() + energyUpdateInterval, energyUpdateMsg);
        return;
    }
    
    DemoBaseApplLayer::handleSelfMsg(msg);
}

void ServiceVehicleApp::sendHeartbeat() {
    // Update current mobility info
    if (mobility) {
        position = mobility->getPositionAt(simTime());
        velocity = mobility->getSpeed();
    }
    
    // Calculate metrics
    double cpuUtilization = 1.0 - (availableCpuFreqHz / totalCpuFreqHz);
    double expectedDelay = calculateExpectedDelay();
    
    EV_INFO << "SV[" << getParentModule()->getIndex() << "] Heartbeat @ t=" << simTime()
            << " | Pos=(" << position.x << "," << position.y << ")"
            << " | Vel=" << velocity << " m/s"
            << " | CPU Util=" << (cpuUtilization*100) << "%"
            << " | Queue=" << currentTaskQueueSize
            << " | TVs in range=" << taskvehiclesInRange.size()
            << " | E=" << totalEnergyConsumed_J << " J"
            << " | ExpDelay=" << expectedDelay << " s" << endl;
    
    // Record metrics
    recordMetrics();
    
    // Emit statistics
    emit(sigCpuUtilization, cpuUtilization);
    emit(sigQueueSize, currentTaskQueueSize);
    emit(sigEnergyConsumed, totalEnergyConsumed_J);
    emit(sigProcessingDelay, expectedDelay);
    
    // ==================== Send DT Update to RSU[0] ====================
    sendDTUpdateToRSU0();
}

void ServiceVehicleApp::discoverTaskVehicles() {
    // Get network module
    cModule* network = getModuleByPath("^.^");
    if (!network) return;
    
    // Clear stale entries (older than 2 seconds)
    auto it = taskvehiclesInRange.begin();
    while (it != taskvehiclesInRange.end()) {
        if (simTime() - it->second.lastSeen > 2.0) {
            EV_INFO << "SV[" << getParentModule()->getIndex() 
                    << "] Lost TV[" << it->first << "]" << endl;
            it = taskvehiclesInRange.erase(it);
        } else {
            ++it;
        }
    }
    
    // Scan for Task Vehicles in range
    // Assuming TVs are node[3] onwards (hardcoded as per requirement)
    for (int i = 3; i < 20; i++) {  // Scan up to 20 vehicles
        cModule* tvModule = network->getSubmodule("node", i);
        if (!tvModule) continue;
        
        // Get TV mobility
        TraCIMobility* tvMobility = FindModule<TraCIMobility*>::findSubModule(tvModule);
        if (!tvMobility) continue;
        
        Coord tvPos = tvMobility->getPositionAt(simTime());
        double distance = position.distance(tvPos);
        
        // Check if in V2V communication range
        if (distance <= v2vCommRange) {
            TVInfo tvInfo;
            tvInfo.nodeIndex = i;
            tvInfo.position = tvPos;
            tvInfo.velocity = tvMobility->getSpeed();
            tvInfo.distance = distance;
            tvInfo.channelGain_dB = calculateChannelGain(distance);
            tvInfo.rssi_dBm = 10 * log10(transmitPowerW * 1000) + tvInfo.channelGain_dB; // dBm
            tvInfo.lastSeen = simTime();
            
            taskvehiclesInRange[i] = tvInfo;
            
            EV_DEBUG << "SV[" << getParentModule()->getIndex() 
                     << "] Discovered TV[" << i << "] @ distance=" << distance 
                     << " m, CG=" << tvInfo.channelGain_dB << " dB" << endl;
        }
    }
}

void ServiceVehicleApp::updateEnergyConsumption() {
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
    
    // Energy = Power * Time (convert mW to W, then to J)
    double energyDelta_J = (powerConsumption_mW / 1000.0) * dt.dbl();
    totalEnergyConsumed_J += energyDelta_J;
    
    lastEnergyUpdateTime = simTime();
}

double ServiceVehicleApp::calculateChannelGain(double distance) {
    if (distance <= 0) distance = 0.1; // Avoid log(0)
    
    // Free-space path loss model: PL(dB) = 20*log10(d) + 20*log10(f) + 20*log10(4π/c)
    // For 5.9 GHz (DSRC): simplified formula
    double frequency_GHz = 5.9;
    double pathLoss_dB = 20 * log10(distance) + 20 * log10(frequency_GHz) + 32.44;
    
    // Channel gain is negative of path loss
    double channelGain_dB = -pathLoss_dB;
    
    // Add shadowing/fading (Rayleigh/Rician fading - simplified)
    double shadowFading_dB = normal(0, 4.0); // 4 dB std deviation
    
    return channelGain_dB + shadowFading_dB;
}

double ServiceVehicleApp::calculateExpectedDelay() {
    // Simple M/M/1 queueing model estimation
    // Expected delay = 1/(μ - λ)
    // where μ = service rate, λ = arrival rate
    
    // Estimate service rate based on CPU capacity
    double avgTaskCycles = 1e8;  // Example: 100M cycles per task
    double serviceRate = availableCpuFreqHz / avgTaskCycles; // tasks/second
    
    // Estimate arrival rate based on queue size
    double arrivalRate = currentTaskQueueSize / (heartbeatInterval + 0.01); // avoid div by 0
    
    // Processing delay
    double processingDelay = 0.0;
    if (serviceRate > arrivalRate && serviceRate > 0) {
        processingDelay = 1.0 / (serviceRate - arrivalRate);
    } else {
        processingDelay = currentTaskQueueSize * (avgTaskCycles / availableCpuFreqHz);
    }
    
    // Queueing delay (simple estimation)
    double queueingDelay = currentTaskQueueSize * 0.01; // 10ms per queued task
    
    return processingDelay + queueingDelay;
}

void ServiceVehicleApp::onWSM(BaseFrame1609_4* wsm) {
    setEnergyState(RX);
    
    DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
    if (dsm) {
        std::string payload = dsm->getName();
        
        // Check if this is a task offload request
        if (payload.find("TASK_OFFLOAD") != std::string::npos) {
            EV_INFO << "SV[" << getParentModule()->getIndex() 
                    << "] Received task offload request: " << payload << endl;
            
            // Process task offloading request
            processTaskOffloadRequest(wsm);
        } else {
            EV_INFO << "SV[" << getParentModule()->getIndex() 
                    << "] Received message: " << payload << endl;
            currentTaskQueueSize++;
        }
    }
    
    DemoBaseApplLayer::onWSM(wsm);
    setEnergyState(IDLE);
}

void ServiceVehicleApp::receiveSignal(cComponent* src, simsignal_t id, cObject* obj, cObject* details) {
    if (mobility && id == mobility->mobilityStateChangedSignal) {
        position = mobility->getPositionAt(simTime());
        velocity = mobility->getSpeed();
    }
    DemoBaseApplLayer::receiveSignal(src, id, obj, details);
}

void ServiceVehicleApp::setEnergyState(EnergyState newState) {
    if (currentEnergyState != newState) {
        updateEnergyConsumption(); // Update before changing state
        currentEnergyState = newState;
    }
}

void ServiceVehicleApp::processTaskOffloadRequest(BaseFrame1609_4* wsm) {
    setEnergyState(COMPUTE);
    
    currentTaskQueueSize++;
    
    // Simulate task processing
    double processingDelay = calculateExpectedDelay();
    
    EV_INFO << "SV[" << getParentModule()->getIndex() 
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
            EV_INFO << "SV[" << getParentModule()->getIndex() 
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
                    EV_INFO << "SV[" << getParentModule()->getIndex() 
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
        EV_INFO << "SV[" << getParentModule()->getIndex() 
                << "] Sending UNICAST response to MAC: " << senderMac << endl;
    } else {
        populateWSM(response);
        EV_WARN << "SV[" << getParentModule()->getIndex() 
                << "] Sender MAC not found, using BROADCAST" << endl;
    }
    
    std::ostringstream payload;
    payload << "TASK_RESULT|SV:" << getParentModule()->getIndex()
            << "|ProcessingTime:" << processingDelay;
    response->setName(payload.str().c_str());
    response->setSenderPos(position);
    response->setUserPriority(7);
    
    sendDown(response);
    
    EV_INFO << "SV[" << getParentModule()->getIndex() 
            << "] Sent task result response" << endl;
    
    currentTaskQueueSize--;
    setEnergyState(IDLE);
}

void ServiceVehicleApp::recordMetrics() {
    recordScalar("SV.position.x", position.x);
    recordScalar("SV.position.y", position.y);
    recordScalar("SV.velocity", velocity);
    recordScalar("SV.availableCpuFreqHz", availableCpuFreqHz);
    recordScalar("SV.cpuUtilization", 1.0 - (availableCpuFreqHz / totalCpuFreqHz));
    recordScalar("SV.queueSize", currentTaskQueueSize);
    recordScalar("SV.tvsInRange", (double)taskvehiclesInRange.size());
    recordScalar("SV.transmitPowerW", transmitPowerW);
    recordScalar("SV.bandwidthHz", availableBandwidthHz);
    recordScalar("SV.energyConsumed_J", totalEnergyConsumed_J);
    recordScalar("SV.expectedDelay", calculateExpectedDelay());
}

PhyLayer80211p* ServiceVehicleApp::getPhyLayer() {
    return FindModule<PhyLayer80211p*>::findSubModule(getParentModule());
}

// ==================== Digital Twin Functions (Reused from PayloadVehicleApp) ====================

std::string ServiceVehicleApp::createDTUpdatePayload() {
    // Create structured DT update payload with real-time SV state
    std::ostringstream payload;
    
    double cpuUtilization = 1.0 - (availableCpuFreqHz / totalCpuFreqHz);
    
    payload << "DT_UPDATE|"
            << "Type:SV|"
            << "NodeID:" << getParentModule()->getIndex() << "|"
            << "Time:" << std::fixed << std::setprecision(3) << simTime().dbl() << "|"
            << "PosX:" << std::fixed << std::setprecision(2) << position.x << "|"
            << "PosY:" << std::fixed << std::setprecision(2) << position.y << "|"
            << "Velocity:" << std::fixed << std::setprecision(2) << velocity << "|"
            << "MAC:" << myMacAddress << "|"
            << "CPUFreqHz:" << std::fixed << std::setprecision(0) << totalCpuFreqHz << "|"
            << "CPUUtil:" << std::fixed << std::setprecision(2) << cpuUtilization << "|"
            << "QueueSize:" << currentTaskQueueSize << "|"
            << "EnergyJ:" << std::fixed << std::setprecision(3) << totalEnergyConsumed_J << "|"
            << "TxPowerW:" << std::fixed << std::setprecision(2) << transmitPowerW << "|"
            << "BandwidthHz:" << std::fixed << std::setprecision(0) << availableBandwidthHz;
    
    return payload.str();
}

LAddress::L2Type ServiceVehicleApp::findRSU0MacAddress() {
    // Get the network module
    cModule* networkModule = getModuleByPath("^.^");
    if (!networkModule) {
        EV_WARN << "SV[" << getParentModule()->getIndex() << "] Network module not found!" << endl;
        return 0;
    }

    // Find RSU[0] (Digital Twin host)
    cModule* rsuModule = networkModule->getSubmodule("rsu", 0);
    if (rsuModule) {
        EV_INFO << "SV[" << getParentModule()->getIndex() << "] Found RSU[0]: " << rsuModule->getFullName() << endl;

        // Get the MAC interface from RSU[0]
        DemoBaseApplLayerToMac1609_4Interface* rsuMacInterface =
            FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(rsuModule);

        if (rsuMacInterface) {
            LAddress::L2Type rsuMacAddress = rsuMacInterface->getMACAddress();
            if (rsuMacAddress != 0) {
                EV_INFO << "SV[" << getParentModule()->getIndex() << "] Found RSU[0] MAC: " << rsuMacAddress << endl;
                return rsuMacAddress;
            }
        }
    }

    EV_WARN << "SV[" << getParentModule()->getIndex() << "] RSU[0] MAC address not found!" << endl;
    return 0;
}

void ServiceVehicleApp::sendDTUpdateToRSU0() {
    // Create DT update payload
    std::string dtPayload = createDTUpdatePayload();
    
    // Find RSU[0] MAC address
    LAddress::L2Type rsu0Mac = findRSU0MacAddress();
    
    if (rsu0Mac == 0) {
        EV_WARN << "SV[" << getParentModule()->getIndex() << "] Cannot send DT update - RSU[0] MAC not found" << endl;
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
    
    EV_INFO << "SV[" << getParentModule()->getIndex() << "] Sent DT update to RSU[0] (MAC: " << rsu0Mac << ")" << endl;
    std::cout << "CONSOLE: SV[" << getParentModule()->getIndex() << "] -> RSU[0] DT Update: " << dtPayload << std::endl;
}

} // namespace complex_network
