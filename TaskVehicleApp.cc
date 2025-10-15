#include "TaskVehicleApp.h"
#include "veins/modules/messages/DemoSafetyMessage_m.h"
#include <cmath>
#include <sstream>

namespace complex_network {

Define_Module(TaskVehicleApp);

void TaskVehicleApp::initialize(int stage) {
    DemoBaseApplLayer::initialize(stage);
    
    if (stage == 0) {
        // ==================== CPU Parameters ====================
        localCpuFreqHz = par("localCpuFreqHz").doubleValue();
        
        // ==================== Task Generation Parameters ====================
        taskCycles_min = par("taskCycles_min").doubleValue();
        taskCycles_max = par("taskCycles_max").doubleValue();
        taskSize_min = par("taskSize_min").doubleValue();
        taskSize_max = par("taskSize_max").doubleValue();
        taskDelay_min = par("taskDelay_min").doubleValue();
        taskDelay_max = par("taskDelay_max").doubleValue();
        taskGenerationInterval = par("taskGenerationInterval").doubleValue();
        
        // ==================== Communication Parameters ====================
        transmitPowerW = par("transmitPowerW").doubleValue();
        v2vCommRange = par("v2vCommRange").doubleValue();
        v2iCommRange = par("v2iCommRange").doubleValue();
        
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
        sigTaskGenerated = registerSignal("tvTaskGenerated");
        sigTaskOffloaded = registerSignal("tvTaskOffloaded");
        sigEnergyConsumed = registerSignal("tvEnergyConsumed");
        
        // ==================== Timer Setup ====================
        heartbeatMsg = new cMessage("tvHeartbeat");
        neighborDiscoveryMsg = new cMessage("tvDiscovery");
        taskGenerationMsg = new cMessage("tvTaskGen");
        energyUpdateMsg = new cMessage("tvEnergyUpdate");
        
        scheduleAt(simTime() + uniform(0, heartbeatInterval), heartbeatMsg);
        scheduleAt(simTime() + uniform(0, discoveryInterval), neighborDiscoveryMsg);
        scheduleAt(simTime() + uniform(0, taskGenerationInterval), taskGenerationMsg);
        scheduleAt(simTime() + energyUpdateInterval, energyUpdateMsg);
        
        EV_INFO << "TaskVehicle[" << getParentModule()->getIndex() 
                << "] initialized with LocalCPU=" << (localCpuFreqHz/1e9) << " GHz, "
                << "TxPower=" << transmitPowerW << " W" << endl;
    }
    else if (stage == 1) {
        // Get MAC address in stage 1
        DemoBaseApplLayerToMac1609_4Interface* macInterface =
            FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(getParentModule());

        if (macInterface) {
            myMac = macInterface->getMACAddress();
            EV_INFO << "TV[" << getParentModule()->getIndex() 
                    << "] MAC address: " << myMac << endl;
        } else {
            EV_WARN << "TV[" << getParentModule()->getIndex() 
                    << "] MAC interface NOT found!" << endl;
            myMac = 0;
        }
    }
}

void TaskVehicleApp::finish() {
    DemoBaseApplLayer::finish();
    
    // Record final statistics
    recordScalar("TV.totalEnergyConsumed_J", totalEnergyConsumed_J);
    recordScalar("TV.totalTasksGenerated", totalTasksGenerated);
    recordScalar("TV.totalTasksOffloaded", totalTasksOffloaded);
    recordScalar("TV.offloadingRatio", totalTasksGenerated > 0 ? 
                 (double)totalTasksOffloaded / totalTasksGenerated : 0.0);
    
    // Cleanup
    cancelAndDelete(heartbeatMsg);
    cancelAndDelete(neighborDiscoveryMsg);
    cancelAndDelete(taskGenerationMsg);
    cancelAndDelete(energyUpdateMsg);
}

void TaskVehicleApp::handleSelfMsg(cMessage* msg) {
    if (msg == heartbeatMsg) {
        sendHeartbeat();
        scheduleAt(simTime() + heartbeatInterval, heartbeatMsg);
        return;
    }
    
    if (msg == neighborDiscoveryMsg) {
        discoverServiceVehicles();
        discoverRSUs();
        scheduleAt(simTime() + discoveryInterval, neighborDiscoveryMsg);
        return;
    }
    
    if (msg == taskGenerationMsg) {
        generateTask();
        scheduleAt(simTime() + exponential(taskGenerationInterval), taskGenerationMsg);
        return;
    }
    
    if (msg == energyUpdateMsg) {
        updateEnergyConsumption();
        scheduleAt(simTime() + energyUpdateInterval, energyUpdateMsg);
        return;
    }
    
    DemoBaseApplLayer::handleSelfMsg(msg);
}

void TaskVehicleApp::sendHeartbeat() {
    // Update current mobility info
    if (mobility) {
        position = mobility->getPositionAt(simTime());
        velocity = mobility->getSpeed();
    }
    
    EV_INFO << "TV[" << getParentModule()->getIndex() << "] Heartbeat @ t=" << simTime()
            << " | Pos=(" << position.x << "," << position.y << ")"
            << " | Vel=" << velocity << " m/s"
            << " | LocalCPU=" << (localCpuFreqHz/1e9) << " GHz"
            << " | SVs in range=" << servicevehiclesInRange.size()
            << " | RSUs in range=" << rsusInRange.size()
            << " | Tasks pending=" << pendingTasks.size()
            << " | E=" << totalEnergyConsumed_J << " J" << endl;
    
    // Record metrics
    recordMetrics();
    
    // Emit statistics
    emit(sigEnergyConsumed, totalEnergyConsumed_J);
}

void TaskVehicleApp::discoverServiceVehicles() {
    // Get network module
    cModule* network = getModuleByPath("^.^");
    if (!network) return;
    
    // Clear stale entries (older than 2 seconds)
    auto it = servicevehiclesInRange.begin();
    while (it != servicevehiclesInRange.end()) {
        if (simTime() - it->second.lastSeen > 2.0) {
            EV_INFO << "TV[" << getParentModule()->getIndex() 
                    << "] Lost SV[" << it->first << "]" << endl;
            it = servicevehiclesInRange.erase(it);
        } else {
            ++it;
        }
    }
    
    // Scan for Service Vehicles in range
    // Assuming SVs are node[0-2] (hardcoded as per requirement)
    for (int i = 0; i <= 2; i++) {
        cModule* svModule = network->getSubmodule("node", i);
        if (!svModule) continue;
        
        // Get SV mobility
        TraCIMobility* svMobility = FindModule<TraCIMobility*>::findSubModule(svModule);
        if (!svMobility) continue;
        
        Coord svPos = svMobility->getPositionAt(simTime());
        double distance = position.distance(svPos);
        
        // Check if in V2V communication range
        if (distance <= v2vCommRange) {
            SVInfo svInfo;
            svInfo.nodeIndex = i;
            svInfo.position = svPos;
            svInfo.velocity = svMobility->getSpeed();
            svInfo.distance = distance;
            svInfo.channelGain_dB = calculateChannelGain(distance);
            svInfo.rssi_dBm = 10 * log10(transmitPowerW * 1000) + svInfo.channelGain_dB; // dBm
            svInfo.lastSeen = simTime();
            
            servicevehiclesInRange[i] = svInfo;
            
            EV_DEBUG << "TV[" << getParentModule()->getIndex() 
                     << "] Discovered SV[" << i << "] @ distance=" << distance 
                     << " m, CG=" << svInfo.channelGain_dB << " dB" << endl;
        }
    }
}

void TaskVehicleApp::discoverRSUs() {
    // Get network module
    cModule* network = getModuleByPath("^.^");
    if (!network) return;
    
    // Clear stale entries
    auto it = rsusInRange.begin();
    while (it != rsusInRange.end()) {
        if (simTime() - it->second.lastSeen > 2.0) {
            EV_INFO << "TV[" << getParentModule()->getIndex() 
                    << "] Lost RSU[" << it->first << "]" << endl;
            it = rsusInRange.erase(it);
        } else {
            ++it;
        }
    }
    
    // Scan for RSUs in range (assuming RSU[0] and RSU[1])
    for (int i = 0; i <= 1; i++) {
        cModule* rsuModule = network->getSubmodule("rsu", i);
        if (!rsuModule) continue;
        
        // RSUs have fixed positions - get from mobility
        cModule* mobilityModule = rsuModule->getSubmodule("mobility");
        if (!mobilityModule) continue;
        
        double rsuX = mobilityModule->par("x").doubleValue();
        double rsuY = mobilityModule->par("y").doubleValue();
        Coord rsuPos(rsuX, rsuY);
        
        double distance = position.distance(rsuPos);
        
        // Check if in V2I communication range
        if (distance <= v2iCommRange) {
            RSUInfo rsuInfo;
            rsuInfo.rsuIndex = i;
            rsuInfo.position = rsuPos;
            rsuInfo.distance = distance;
            rsuInfo.channelGain_dB = calculateChannelGain(distance);
            rsuInfo.rssi_dBm = 10 * log10(transmitPowerW * 1000) + rsuInfo.channelGain_dB;
            rsuInfo.lastSeen = simTime();
            
            rsusInRange[i] = rsuInfo;
            
            EV_DEBUG << "TV[" << getParentModule()->getIndex() 
                     << "] Discovered RSU[" << i << "] @ distance=" << distance 
                     << " m, CG=" << rsuInfo.channelGain_dB << " dB" << endl;
        }
    }
}

void TaskVehicleApp::generateTask() {
    TaskInfo task;
    task.taskId = nextTaskId++;
    task.cpuCyclesRequired = uniform(taskCycles_min, taskCycles_max);
    task.dataSizeBytes = uniform(taskSize_min, taskSize_max);
    task.maxTolerableDelay = uniform(taskDelay_min, taskDelay_max);
    task.generationTime = simTime();
    task.offloaded = false;
    
    pendingTasks.push_back(task);
    totalTasksGenerated++;
    
    EV_INFO << "TV[" << getParentModule()->getIndex() 
            << "] Generated Task #" << task.taskId
            << " | Cycles=" << (task.cpuCyclesRequired/1e6) << "M"
            << " | Size=" << (task.dataSizeBytes/1e3) << " KB"
            << " | MaxDelay=" << task.maxTolerableDelay << " s" << endl;
    
    emit(sigTaskGenerated, task.taskId);
    
    // Decide whether to offload
    if (shouldOffloadTask(task)) {
        offloadTask(task);
    } else {
        EV_INFO << "TV[" << getParentModule()->getIndex() 
                << "] Processing Task #" << task.taskId << " locally" << endl;
        setEnergyState(COMPUTE);
        // Local processing simulation (simplified)
        double localExecTime = task.cpuCyclesRequired / localCpuFreqHz;
        EV_INFO << "TV[" << getParentModule()->getIndex() 
                << "] Local execution time: " << localExecTime << " s" << endl;
    }
}

bool TaskVehicleApp::shouldOffloadTask(const TaskInfo& task) {
    // Simple offloading decision logic
    // Offload if: SVs or RSUs available AND task is large enough
    
    if (servicevehiclesInRange.empty() && rsusInRange.empty()) {
        return false; // No offloading targets available
    }
    
    // Estimate local execution time
    double localExecTime = task.cpuCyclesRequired / localCpuFreqHz;
    
    // If local execution exceeds max delay, must offload
    if (localExecTime > task.maxTolerableDelay) {
        return true;
    }
    
    // Offload large tasks to save energy
    if (task.cpuCyclesRequired > 5e8) { // > 500M cycles
        return true;
    }
    
    return false;
}

void TaskVehicleApp::offloadTask(const TaskInfo& task) {
    setEnergyState(TX);
    
    // Select best offloading target (closest SV or RSU with best channel gain)
    int bestTargetIndex = -1;
    bool targetIsRSU = false;
    double bestMetric = -999.0; // metric = channel gain - distance penalty
    
    for (const auto& sv : servicevehiclesInRange) {
        double metric = sv.second.channelGain_dB - (sv.second.distance / 100.0);
        if (metric > bestMetric) {
            bestMetric = metric;
            bestTargetIndex = sv.first;
            targetIsRSU = false;
        }
    }
    
    for (const auto& rsu : rsusInRange) {
        double metric = rsu.second.channelGain_dB - (rsu.second.distance / 100.0);
        if (metric > bestMetric) {
            bestMetric = metric;
            bestTargetIndex = rsu.first;
            targetIsRSU = true;
        }
    }
    
    if (bestTargetIndex >= 0) {
        EV_INFO << "TV[" << getParentModule()->getIndex() 
                << "] Offloading Task #" << task.taskId
                << " to " << (targetIsRSU ? "RSU" : "SV") << "[" << bestTargetIndex << "]"
                << " | Metric=" << bestMetric << endl;
        
        totalTasksOffloaded++;
        emit(sigTaskOffloaded, task.taskId);
        
        // Find target MAC address for unicast
        LAddress::L2Type targetMacAddress = findTargetMacAddress(targetIsRSU, bestTargetIndex);
        
        // Create and send offloading message
        DemoSafetyMessage* wsm = new DemoSafetyMessage("TaskOffloadRequest");
        
        // Use unicast if MAC address found, otherwise broadcast
        if (targetMacAddress != 0) {
            populateWSM(wsm, targetMacAddress);
            EV_INFO << "TV[" << getParentModule()->getIndex() 
                    << "] Using UNICAST to MAC: " << targetMacAddress << endl;
        } else {
            populateWSM(wsm);
            EV_WARN << "TV[" << getParentModule()->getIndex() 
                    << "] MAC not found, using BROADCAST" << endl;
        }
        
        std::ostringstream payload;
        payload << "TASK_OFFLOAD|TaskID:" << task.taskId
                << "|Cycles:" << task.cpuCyclesRequired
                << "|Size:" << task.dataSizeBytes
                << "|Deadline:" << task.maxTolerableDelay
                << "|SenderMAC:" << myMac;  // Include sender MAC for response
        wsm->setName(payload.str().c_str());
        wsm->setSenderPos(position);
        wsm->setUserPriority(7);
        
        sendDown(wsm);
    }
    
    setEnergyState(IDLE);
}

void TaskVehicleApp::updateEnergyConsumption() {
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
            powerConsumption_mW = energyCompute_mW_per_GHz * (localCpuFreqHz / 1e9);
            break;
    }
    
    // Energy = Power * Time
    double energyDelta_J = (powerConsumption_mW / 1000.0) * dt.dbl();
    totalEnergyConsumed_J += energyDelta_J;
    
    lastEnergyUpdateTime = simTime();
}

double TaskVehicleApp::calculateChannelGain(double distance) {
    if (distance <= 0) distance = 0.1;
    
    // Free-space path loss model for 5.9 GHz
    double frequency_GHz = 5.9;
    double pathLoss_dB = 20 * log10(distance) + 20 * log10(frequency_GHz) + 32.44;
    double channelGain_dB = -pathLoss_dB;
    
    // Add shadowing
    double shadowFading_dB = normal(0, 4.0);
    
    return channelGain_dB + shadowFading_dB;
}

void TaskVehicleApp::onWSM(BaseFrame1609_4* wsm) {
    setEnergyState(RX);
    
    DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
    if (dsm) {
        std::string payload = dsm->getName();
        
        EV_DEBUG << "TV[" << getParentModule()->getIndex() 
                 << "] Received message: " << payload << endl;
        
        if (payload.find("TASK_RESULT") != std::string::npos) {
            EV_INFO << "=========================================" << endl;
            EV_INFO << "TV[" << getParentModule()->getIndex() 
                    << "] ✓ RECEIVED TASK RESULT" << endl;
            EV_INFO << "Payload: " << payload << endl;
            EV_INFO << "Time: " << simTime() << endl;
            EV_INFO << "Sender: " << wsm->getSenderModule()->getFullName() << endl;
            EV_INFO << "=========================================" << endl;
            
            // Also print to console for visibility
            std::cout << "=========================================" << std::endl;
            std::cout << "TV[" << getParentModule()->getIndex() 
                      << "] ✓ RECEIVED TASK RESULT from "
                      << wsm->getSenderModule()->getFullName() << std::endl;
            std::cout << "Payload: " << payload << std::endl;
            std::cout << "Time: " << simTime() << std::endl;
            std::cout << "=========================================" << std::endl;
        }
    }
    
    DemoBaseApplLayer::onWSM(wsm);
    setEnergyState(IDLE);
}

void TaskVehicleApp::receiveSignal(cComponent* src, simsignal_t id, cObject* obj, cObject* details) {
    if (mobility && id == mobility->mobilityStateChangedSignal) {
        position = mobility->getPositionAt(simTime());
        velocity = mobility->getSpeed();
    }
    DemoBaseApplLayer::receiveSignal(src, id, obj, details);
}

void TaskVehicleApp::setEnergyState(EnergyState newState) {
    if (currentEnergyState != newState) {
        updateEnergyConsumption();
        currentEnergyState = newState;
    }
}

void TaskVehicleApp::recordMetrics() {
    recordScalar("TV.position.x", position.x);
    recordScalar("TV.position.y", position.y);
    recordScalar("TV.velocity", velocity);
    recordScalar("TV.localCpuFreqHz", localCpuFreqHz);
    recordScalar("TV.svsInRange", (double)servicevehiclesInRange.size());
    recordScalar("TV.rsusInRange", (double)rsusInRange.size());
    recordScalar("TV.pendingTasks", (double)pendingTasks.size());
    recordScalar("TV.transmitPowerW", transmitPowerW);
    recordScalar("TV.energyConsumed_J", totalEnergyConsumed_J);
    
    // Record distances to all SVs in range
    for (const auto& sv : servicevehiclesInRange) {
        std::ostringstream ss;
        ss << "TV.distanceToSV[" << sv.first << "]";
        recordScalar(ss.str().c_str(), sv.second.distance);
    }
}

PhyLayer80211p* TaskVehicleApp::getPhyLayer() {
    return FindModule<PhyLayer80211p*>::findSubModule(getParentModule());
}

LAddress::L2Type TaskVehicleApp::findTargetMacAddress(bool isRSU, int targetIndex) {
    // Get the network module
    cModule* networkModule = getModuleByPath("^.^");
    if (!networkModule) {
        EV_ERROR << "TV[" << getParentModule()->getIndex() 
                 << "] Network module not found!" << endl;
        return 0;
    }
    
    cModule* targetModule = nullptr;
    
    if (isRSU) {
        // Find RSU module
        targetModule = networkModule->getSubmodule("rsu", targetIndex);
        if (!targetModule) {
            EV_WARN << "TV[" << getParentModule()->getIndex() 
                    << "] RSU[" << targetIndex << "] module not found!" << endl;
            return 0;
        }
    } else {
        // Find SV (node) module
        targetModule = networkModule->getSubmodule("node", targetIndex);
        if (!targetModule) {
            EV_WARN << "TV[" << getParentModule()->getIndex() 
                    << "] SV[" << targetIndex << "] module not found!" << endl;
            return 0;
        }
    }
    
    // Get MAC interface from target
    DemoBaseApplLayerToMac1609_4Interface* macInterface =
        FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(targetModule);
    
    if (macInterface) {
        LAddress::L2Type targetMac = macInterface->getMACAddress();
        EV_INFO << "TV[" << getParentModule()->getIndex() 
                << "] Found target MAC: " << targetMac 
                << " for " << (isRSU ? "RSU" : "SV") << "[" << targetIndex << "]" << endl;
        return targetMac;
    } else {
        EV_WARN << "TV[" << getParentModule()->getIndex() 
                << "] MAC interface not found for " 
                << (isRSU ? "RSU" : "SV") << "[" << targetIndex << "]" << endl;
        return 0;
    }
}

} // namespace complex_network
