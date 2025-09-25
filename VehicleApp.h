#pragma once

#include <omnetpp.h>
#include "veins/modules/application/ieee80211p/DemoBaseApplLayer.h"
#include "veins/modules/mobility/traci/TraCIMobility.h"
#include "veins/base/utils/FindModule.h"
#include "veins/base/utils/Coord.h"

namespace complex_network {

using namespace omnetpp;
using namespace veins;

class VehicleApp : public DemoBaseApplLayer {
  protected:
    // ---- Vehicle (persistent) state ----
    double flocHz = 0.0;         // Hz
    double txPower_mW = 0.0;     // mW

    // ---- Schedules ----
    double heartbeatInterval = 0.5; // s
    double taskIntervalMean = 2.0;  // s (mean)
    double jitterFactor = 0.0;      // slow drift for floc/tx (optional)

    // ---- Task distribution config ----
    double taskSizeB_min = 0.0, taskSizeB_max = 0.0;
    double cpuPB_min = 0.0, cpuPB_max = 0.0;
    double ddl_min = 0.0, ddl_max = 0.0;
    int    divs_min = 1, divs_max = 1;

    // ---- Infra ----
    cMessage* hbMsg   = nullptr; // heartbeat timer
    cMessage* taskMsg = nullptr; // task-arrival timer
    TraCIMobility* mobility = nullptr;

    // ---- Live kinematics ----
    double speed = 0.0;
    Coord  pos;

    // ---- Signals / metrics ----
    simsignal_t sigHeartbeat;
    simsignal_t sigTaskArrive;

    // lifecycle
    virtual void initialize(int stage) override;
    virtual void handleSelfMsg(cMessage* msg) override;
    virtual void receiveSignal(cComponent* src, simsignal_t id, cObject* obj, cObject* details) override;

    // helpers
    void scheduleNextTask();
    void maybeDriftVehicleKnobs(); // optional slow drift of floc/tx
    void recordHeartbeatScalars();
    void recordTaskScalars(double sizeB, double cpuPerByte, double deadlineS);
};

} // namespace complex_network
