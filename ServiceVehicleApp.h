#pragma once

#include <omnetpp.h>
#include <map>
#include <vector>
#include "veins/modules/application/ieee80211p/DemoBaseApplLayer.h"
#include "veins/modules/mobility/traci/TraCIMobility.h"
#include "veins/base/utils/FindModule.h"
#include "veins/base/utils/Coord.h"
#include "veins/modules/phy/DeciderResult80211.h"
#include "veins/modules/phy/PhyLayer80211p.h"

namespace complex_network {

using namespace omnetpp;
using namespace veins;

/**
 * Service Vehicle (SV) - Acts as mobile edge compute server
 * Provides offloading services to Task Vehicles (TVs)
 */
class ServiceVehicleApp : public DemoBaseApplLayer {
  protected:
    // ==================== CPU & Compute Resources ====================
    double availableCpuFreqHz = 0.0;      // Available CPU frequency (Hz)
    double totalCpuFreqHz = 0.0;          // Total CPU capacity
    int currentTaskQueueSize = 0;          // Number of tasks in queue
    double currentProcessingLoad = 0.0;    // Current computational load (0-1)
    
    // ==================== Mobility & Position ====================
    TraCIMobility* mobility = nullptr;
    Coord position;                        // Current position
    double velocity = 0.0;                 // Current velocity (m/s)
    
    // ==================== Communication Parameters ====================
    double transmitPowerW = 0.0;           // Transmit power (Watts)
    double availableBandwidthHz = 0.0;     // Available bandwidth (Hz)
    double v2vCommRange = 0.0;             // V2V communication range (m)
    
    // ==================== Energy Consumption ====================
    double energyIdle_mW = 0.0;            // Idle power consumption (mW)
    double energyTx_mW = 0.0;              // TX power consumption (mW)
    double energyRx_mW = 0.0;              // RX power consumption (mW)
    double energyCompute_mW_per_GHz = 0.0; // Compute power per GHz (mW/GHz)
    double totalEnergyConsumed_J = 0.0;    // Cumulative energy (Joules)
    simtime_t lastEnergyUpdateTime;        // Last energy update timestamp
    
    enum EnergyState { IDLE, TX, RX, COMPUTE };
    EnergyState currentEnergyState = IDLE;
    
    // ==================== Task Vehicles in Range ====================
    struct TVInfo {
        int nodeIndex;
        Coord position;
        double velocity;
        double distance;
        double channelGain_dB;           // Channel gain in dB
        double rssi_dBm;                 // Received signal strength
        simtime_t lastSeen;
    };
    std::map<int, TVInfo> taskvehiclesInRange; // TV index -> TV info
    
    // ==================== Timers & Intervals ====================
    cMessage* heartbeatMsg = nullptr;
    cMessage* neighborDiscoveryMsg = nullptr;
    cMessage* energyUpdateMsg = nullptr;
    double heartbeatInterval = 1.0;        // Heartbeat interval (s)
    double discoveryInterval = 0.5;        // Neighbor discovery interval (s)
    double energyUpdateInterval = 0.1;     // Energy update interval (s)
    
    // ==================== Statistics ====================
    simsignal_t sigCpuUtilization;
    simsignal_t sigQueueSize;
    simsignal_t sigEnergyConsumed;
    simsignal_t sigProcessingDelay;
    
    // ==================== Lifecycle Methods ====================
    virtual void initialize(int stage) override;
    virtual void finish() override;
    virtual void handleSelfMsg(cMessage* msg) override;
    virtual void onWSM(BaseFrame1609_4* wsm) override;
    virtual void receiveSignal(cComponent* src, simsignal_t id, cObject* obj, cObject* details) override;
    
    // ==================== Core Functions ====================
    void sendHeartbeat();
    void discoverTaskVehicles();
    void updateEnergyConsumption();
    void updateChannelGain(int tvIndex, double distance);
    double calculateChannelGain(double distance);
    double calculateExpectedDelay();
    double getDistanceToNode(int nodeIndex);
    void setEnergyState(EnergyState newState);
    void processTaskOffloadRequest(BaseFrame1609_4* wsm);
    
    // ==================== Helper Methods ====================
    void recordMetrics();
    PhyLayer80211p* getPhyLayer();
    
    // ==================== Digital Twin Functions ====================
    std::string createDTUpdatePayload();         // Create DT update payload (from PayloadVehicleApp)
    void sendDTUpdateToRSU0();                   // Send DT update to RSU[0]
    LAddress::L2Type findRSU0MacAddress();       // Find RSU[0] MAC address
    LAddress::L2Type myMacAddress = 0;           // Own MAC address
};

} // namespace complex_network
