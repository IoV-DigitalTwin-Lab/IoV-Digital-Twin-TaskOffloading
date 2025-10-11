#pragma once

#include <omnetpp.h>
#include <map>
#include <vector>
#include "veins/modules/application/ieee80211p/DemoBaseApplLayer.h"
#include "veins/modules/mobility/traci/TraCIMobility.h"
#include "veins/base/utils/FindModule.h"
#include "veins/base/utils/Coord.h"
#include "veins/modules/phy/PhyLayer80211p.h"

namespace complex_network {

using namespace omnetpp;
using namespace veins;

/**
 * Task Vehicle (TV) - Generates computation tasks for offloading
 * Can offload to Service Vehicles (SVs) or RSUs
 */
class TaskVehicleApp : public DemoBaseApplLayer {
  protected:
    // ==================== CPU & Local Compute ====================
    double localCpuFreqHz = 0.0;           // Local CPU frequency (Hz)
    
    // ==================== Task Information ====================
    struct TaskInfo {
        int taskId;
        double cpuCyclesRequired;          // CPU cycles needed
        double dataSizeBytes;              // Task data size
        double maxTolerableDelay;          // Maximum delay tolerance (s)
        simtime_t generationTime;          // When task was generated
        bool offloaded = false;
    };
    std::vector<TaskInfo> pendingTasks;
    int nextTaskId = 1;
    
    // Task generation parameters
    double taskCycles_min = 0.0, taskCycles_max = 0.0;
    double taskSize_min = 0.0, taskSize_max = 0.0;
    double taskDelay_min = 0.0, taskDelay_max = 0.0;
    double taskGenerationInterval = 0.0;
    
    // ==================== Mobility & Position ====================
    TraCIMobility* mobility = nullptr;
    Coord position;                        // Current position
    double velocity = 0.0;                 // Current velocity (m/s)
    
    // ==================== Communication Parameters ====================
    double transmitPowerW = 0.0;           // Transmit power (Watts)
    double v2vCommRange = 0.0;             // V2V communication range (m)
    double v2iCommRange = 0.0;             // V2I (RSU) communication range (m)
    
    // ==================== Energy Consumption ====================
    double energyIdle_mW = 0.0;            // Idle power consumption (mW)
    double energyTx_mW = 0.0;              // TX power consumption (mW)
    double energyRx_mW = 0.0;              // RX power consumption (mW)
    double energyCompute_mW_per_GHz = 0.0; // Compute power per GHz (mW/GHz)
    double totalEnergyConsumed_J = 0.0;    // Cumulative energy (Joules)
    simtime_t lastEnergyUpdateTime;        // Last energy update timestamp
    
    enum EnergyState { IDLE, TX, RX, COMPUTE };
    EnergyState currentEnergyState = IDLE;
    
    // ==================== Service Vehicles in Range ====================
    struct SVInfo {
        int nodeIndex;
        Coord position;
        double velocity;
        double distance;
        double channelGain_dB;           // Channel gain in dB
        double rssi_dBm;                 // Received signal strength
        simtime_t lastSeen;
    };
    std::map<int, SVInfo> servicevehiclesInRange; // SV index -> SV info
    
    // ==================== RSUs in Range ====================
    struct RSUInfo {
        int rsuIndex;
        Coord position;
        double distance;
        double channelGain_dB;
        double rssi_dBm;
        simtime_t lastSeen;
    };
    std::map<int, RSUInfo> rsusInRange;    // RSU index -> RSU info
    
    // ==================== Timers & Intervals ====================
    cMessage* heartbeatMsg = nullptr;
    cMessage* neighborDiscoveryMsg = nullptr;
    cMessage* taskGenerationMsg = nullptr;
    cMessage* energyUpdateMsg = nullptr;
    double heartbeatInterval = 1.0;        // Heartbeat interval (s)
    double discoveryInterval = 0.5;        // Neighbor discovery interval (s)
    double energyUpdateInterval = 0.1;     // Energy update interval (s)
    
    // ==================== Statistics ====================
    simsignal_t sigTaskGenerated;
    simsignal_t sigTaskOffloaded;
    simsignal_t sigEnergyConsumed;
    int totalTasksGenerated = 0;
    int totalTasksOffloaded = 0;
    
    // ==================== Lifecycle Methods ====================
    virtual void initialize(int stage) override;
    virtual void finish() override;
    virtual void handleSelfMsg(cMessage* msg) override;
    virtual void onWSM(BaseFrame1609_4* wsm) override;
    virtual void receiveSignal(cComponent* src, simsignal_t id, cObject* obj, cObject* details) override;
    
    // ==================== Core Functions ====================
    void sendHeartbeat();
    void discoverServiceVehicles();
    void discoverRSUs();
    void generateTask();
    void updateEnergyConsumption();
    double calculateChannelGain(double distance);
    void setEnergyState(EnergyState newState);
    bool shouldOffloadTask(const TaskInfo& task);
    void offloadTask(const TaskInfo& task);
    
    // ==================== Helper Methods ====================
    void recordMetrics();
    PhyLayer80211p* getPhyLayer();
};

} // namespace complex_network
