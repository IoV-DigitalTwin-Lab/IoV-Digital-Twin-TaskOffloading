#ifndef SINGLEMESSAGERSUAPP_H
#define SINGLEMESSAGERSUAPP_H

#include "veins/modules/application/ieee80211p/DemoBaseApplLayer.h"
#include "veins/modules/messages/DemoSafetyMessage_m.h"

using namespace veins;

namespace complex_network {

class SingleMessageRSUApp : public DemoBaseApplLayer {
public:
    void initialize(int stage) override;
    void handleMessage(cMessage* msg) override;  // ADD THIS LINE
    void receiveSignal(cComponent* source, simsignal_t signalID, cObject* obj, cObject* details) override;  // ADD THIS LINE

protected:
    void onWSM(BaseFrame1609_4* wsm) override;
    void handleSelfMsg(cMessage* msg) override;

private:
    bool messageSent = false; // Flag to ensure we only send once
};

} // namespace complex_network

#endif // SINGLEMESSAGERSUAPP_H
