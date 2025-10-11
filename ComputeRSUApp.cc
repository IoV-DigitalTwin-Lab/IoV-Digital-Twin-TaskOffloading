#include "ComputeRSUApp.h"
#include "veins/modules/messages/DemoSafetyMessage_m.h"
#include <cmath>
#include <sstream>

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
        
        // Check if this is a task offload request
        if (payload.find("TASK_OFFLOAD") != std::string::npos) {
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
    
    // TODO: Send response back to vehicle
    setEnergyState(TX);
    
    // Create response message
    DemoSafetyMessage* response = new DemoSafetyMessage("TaskOffloadResponse");
    populateWSM(response);
    
    std::ostringstream payload;
    payload << "TASK_RESULT|RSU:" << getParentModule()->getIndex()
            << "|ProcessingTime:" << processingDelay;
    response->setName(payload.str().c_str());
    response->setSenderPos(position);
    response->setUserPriority(7);
    
    sendDown(response);
    
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

} // namespace complex_network
