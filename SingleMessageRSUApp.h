#ifndef SINGLEMESSAGERSUAPP_H
#define SINGLEMESSAGERSUAPP_H

#include "veins/modules/application/ieee80211p/DemoBaseApplLayer.h"
#include "veins/modules/messages/DemoSafetyMessage_m.h"

using namespace veins;

namespace complex_network {

class SingleMessageRSUApp : public DemoBaseApplLayer {
public:
    // Standard OMNeT++ initialization
    virtual void initialize(int stage) override;

    // Handle any incoming message (self or from other modules)
    virtual void handleMessage(cMessage* msg) override;

    // Receive signals (for statistics, mobility updates, etc.)
    virtual void receiveSignal(cComponent* source, simsignal_t signalID, cObject* obj, cObject* details) override;

protected:
    // Handle incoming WSM messages
    virtual void onWSM(BaseFrame1609_4* wsm) override;

    // Handle self messages (timers, scheduled events)
    virtual void handleSelfMsg(cMessage* msg) override;

private:
    bool messageSent = false;          // Ensure RSU sends only once
    LAddress::L2Type myMacAddress;     // For unicast or filtering if needed
};

} // namespace complex_network

#endif // SINGLEMESSAGERSUAPP_H
