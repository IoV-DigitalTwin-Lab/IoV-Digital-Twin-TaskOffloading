#pragma once

#include <omnetpp.h>
#include <map>
#include "veins/modules/application/ieee80211p/DemoBaseApplLayer.h"
#include "veins/base/utils/Coord.h"

namespace complex_network {

using namespace omnetpp;
using namespace veins;

/**
 * Compute-capable RSU - Stationary edge computing node
 * Provides offloading services to Task Vehicles (TVs)
 */
class ComputeRSUApp : public DemoBaseApplLayer {
  protected:
    // ==================== CPU & Compute Resources ====================
    double availableCpuFreqHz = 0.0;      // Available CPU frequency (Hz)
    double totalCpuFreqHz = 0.0;          // Total CPU capacity
    int currentTaskQueueSize = 0;          // Number of tasks in queue
    double currentProcessingLoad = 0.0;    // Current computational load (0-1)
    
    // ==================== Position (Static for RSU) ====================
    Coord position;                        // Fixed position
    
    // ==================== Communication Parameters ====================
    double transmitPowerW = 0.0;           // Transmit power (Watts)
    double availableBandwidthHz = 0.0;     // Available bandwidth share (Hz)
    double totalBandwidthHz = 0.0;         // Total bandwidth capacity
    
    // ==================== Energy Consumption ====================
    double energyIdle_mW = 0.0;            // Idle power consumption (mW)
    double energyTx_mW = 0.0;              // TX power consumption (mW)
    double energyRx_mW = 0.0;              // RX power consumption (mW)
    double energyCompute_mW_per_GHz = 0.0; // Compute power per GHz (mW/GHz)
    double totalEnergyConsumed_J = 0.0;    // Cumulative energy (Joules)
    simtime_t lastEnergyUpdateTime;        // Last energy update timestamp
    
    enum EnergyState { IDLE, TX, RX, COMPUTE };
    EnergyState currentEnergyState = IDLE;
    
    // ==================== Connected Vehicles ====================
    struct VehicleInfo {
        int nodeIndex;
        Coord lastKnownPosition;
        double distance;
        double channelGain_dB;
        double rssi_dBm;
        simtime_t lastSeen;
    };
    std::map<int, VehicleInfo> vehiclesInRange; // Vehicle index -> info
    
    // ==================== Timers & Intervals ====================
    cMessage* heartbeatMsg = nullptr;
    cMessage* energyUpdateMsg = nullptr;
    double heartbeatInterval = 1.0;        // Heartbeat interval (s)
    double energyUpdateInterval = 0.1;     // Energy update interval (s)
    
    // ==================== Statistics ====================
    simsignal_t sigCpuUtilization;
    simsignal_t sigQueueSize;
    simsignal_t sigEnergyConsumed;
    simsignal_t sigProcessingDelay;
    simsignal_t sigBandwidthUtilization;
    int totalTasksProcessed = 0;
    
    // ==================== Lifecycle Methods ====================
    virtual void initialize(int stage) override;
    virtual void finish() override;
    virtual void handleSelfMsg(cMessage* msg) override;
    virtual void onWSM(BaseFrame1609_4* wsm) override;
    
    // ==================== Core Functions ====================
    void sendHeartbeat();
    void updateEnergyConsumption();
    double calculateChannelGain(double distance, Coord vehiclePos);
    double calculateExpectedDelay();
    void setEnergyState(EnergyState newState);
    void processTaskOffloadRequest(BaseFrame1609_4* wsm);
    
    // ==================== Helper Methods ====================
    void recordMetrics();
};

} // namespace complex_network
