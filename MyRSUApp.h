#ifndef __MYRSUAPP_H_
#define __MYRSUAPP_H_

#include "veins/modules/application/ieee80211p/DemoBaseApplLayer.h"
#include "veins/modules/messages/DemoSafetyMessage_m.h"

using namespace veins;

namespace complex_network {

class MyRSUApp : public DemoBaseApplLayer {
public:
    void initialize(int stage) override;

protected:
    void onWSM(BaseFrame1609_4* wsm) override;
    void handleSelfMsg(cMessage* msg) override;
};

} // namespace communication_network

#endif
