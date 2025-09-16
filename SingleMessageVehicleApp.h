#ifndef SINGLEMESSAGEVEHICLEAPP_H
#define SINGLEMESSAGEVEHICLEAPP_H

#include "veins/modules/application/ieee80211p/DemoBaseApplLayer.h"
#include "veins/modules/messages/DemoSafetyMessage_m.h"

using namespace veins;

namespace complex_network {

class SingleMessageVehicleApp : public DemoBaseApplLayer {
protected:
    virtual void initialize(int stage) override;
    virtual void onWSM(BaseFrame1609_4* wsm) override;
    virtual void handleMessage(cMessage* msg) override;
    virtual void receiveSignal(cComponent* source, simsignal_t signalID, cObject* obj, cObject* details) override;
};

} // namespace complex_network

#endif // SINGLEMESSAGEVEHICLEAPP_H