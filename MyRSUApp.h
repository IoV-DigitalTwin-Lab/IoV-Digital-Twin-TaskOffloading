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

private:
    RSUHttpPoster poster{"http://127.0.0.1:8000/ingest"};
    // self-message used for periodic beacons; keep as member so we can cancel/delete safely
    omnetpp::cMessage* beaconMsg{nullptr};
};

} // namespace complex_network

#endif