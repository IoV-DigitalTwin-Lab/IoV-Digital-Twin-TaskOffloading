#ifndef PAYLOADVEHICLEAPP_H
#define PAYLOADVEHICLEAPP_H

#include "veins/modules/application/ieee80211p/DemoBaseApplLayer.h"
#include "veins/modules/messages/DemoSafetyMessage_m.h"
#include "veins/modules/mobility/traci/TraCIMobility.h"
#include "veins/base/utils/FindModule.h"
#include "veins/base/utils/Coord.h"

namespace complex_network {

class PayloadVehicleApp : public veins::DemoBaseApplLayer {
protected:
    virtual void initialize(int stage) override;
    virtual void onWSM(veins::BaseFrame1609_4* wsm) override;
    virtual void handleMessage(cMessage* msg) override;
    virtual void handleSelfMsg(cMessage* msg) override;
    virtual void receiveSignal(cComponent* src, simsignal_t id, cObject* obj, cObject* details) override;

private:
    veins::LAddress::L2Type myMacAddress;  // Store our own MAC address
    bool messageSent = false;              // To send only once
    
    // Vehicle data members (similar to VehicleApp)
    double flocHz_max = 0.0;         // Maximum CPU capacity (Hz)
    double flocHz_available = 0.0;   // Current available CPU capacity (Hz)
    double txPower_mW = 0.0;         // mW
    double speed = 0.0;              // Vehicle speed
    veins::Coord pos;                // Vehicle position
    veins::TraCIMobility* mobility = nullptr;  // Mobility module
    
    // CPU variation parameters
    double cpuLoadFactor = 0.0;      // Current CPU load (0.0-1.0)
    double lastCpuUpdateTime = 0.0;  // Last time CPU load was updated
    
    // Helper methods
    veins::LAddress::L2Type findRSUMacAddress();
    std::string createVehicleDataPayload();  // Create payload with actual vehicle data
    void updateVehicleData();                // Update current vehicle parameters
};

} // namespace complex_network

#endif // PAYLOADVEHICLEAPP_H
