#ifndef __MYRSUAPP_H_
#define __MYRSUAPP_H_

#include "veins/modules/application/ieee80211p/DemoBaseApplLayer.h"
#include "veins/modules/messages/DemoSafetyMessage_m.h"
#include "rsu_http_poster.h"

using namespace veins;

namespace complex_network {

class MyRSUApp : public DemoBaseApplLayer {
public:
    void initialize(int stage) override;
    void finish() override;

protected:
    void onWSM(BaseFrame1609_4* wsm) override;
    void handleSelfMsg(cMessage* msg) override;
    void handleLowerMsg(cMessage* msg) override;  // Add this!
    void handleMessage(cMessage* msg) override;

private:
    RSUHttpPoster poster{"http://127.0.0.1:8000/ingest"};
    // self-message used for periodic beacons; keep as member so we can cancel/delete safely
    omnetpp::cMessage* beaconMsg{nullptr};
    
    // Edge server resource parameters (static values)
    double edgeCPU_GHz = 0.0;           // Edge server CPU capacity (GHz)
    double edgeMemory_GB = 0.0;         // Edge server memory capacity (GB)
    int maxVehicles = 0;                // Maximum vehicles this RSU can serve
    double processingDelay_ms = 0.0;    // Base processing delay (ms)
};

} // namespace complex_network

#endif