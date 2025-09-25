<<<<<<< HEAD
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
    virtual void handleSelfMsg(cMessage* msg) override;
    virtual void receiveSignal(cComponent* source, simsignal_t signalID, cObject* obj, cObject* details) override;

private:
    LAddress::L2Type myMacAddress;      // Store our own MAC address
    LAddress::L2Type targetMacAddress;  // Store the intended recipient address
};

} // namespace complex_network

#endif // SINGLEMESSAGEVEHICLEAPP_H
=======
#ifndef SINGLEMESSAGEVEHICLEAPP_H
#define SINGLEMESSAGEVEHICLEAPP_H

#include "veins/modules/application/ieee80211p/DemoBaseApplLayer.h"
#include "veins/modules/messages/DemoSafetyMessage_m.h"
#include "veins/base/utils/Coord.h"

using namespace veins;

namespace complex_network {

class SingleMessageVehicleApp : public DemoBaseApplLayer {
protected:
    virtual void initialize(int stage) override;
    virtual void handleSelfMsg(cMessage* msg) override;
    virtual void onWSM(BaseFrame1609_4* wsm) override;
    virtual void handleMessage(cMessage* msg) override;
    virtual void receiveSignal(cComponent* source, simsignal_t signalID, cObject* obj, cObject* details) override;

    // Helper methods for true unicast
    int countVehiclesInSimulation();
    LAddress::L2Type getMacAddressOfVehicle(int vehicleId);
    cModule* findVehicleModule(int vehicleId);
    void sendUnicastMessage(int targetVehicleId);

    // Member variables for random targeting
    int targetVehicleId = -1;
    int totalVehicles = 0;

    // Vehicle registry for MAC address mapping
    std::map<int, LAddress::L2Type> vehicleMacMap;
    std::map<int, cModule*> vehicleModuleMap;
};

} // namespace complex_network

#endif // SINGLEMESSAGEVEHICLEAPP_H
>>>>>>> e5db28b (Added Vehicle Parameters)
