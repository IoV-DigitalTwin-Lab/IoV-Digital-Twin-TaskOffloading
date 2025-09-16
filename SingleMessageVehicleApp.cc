#include "SingleMessageVehicleApp.h"

using namespace veins;

namespace complex_network {

Define_Module(SingleMessageVehicleApp);

void SingleMessageVehicleApp::initialize(int stage) {
    DemoBaseApplLayer::initialize(stage);
    if (stage == 0) {
        EV_INFO << "VehicleApp: Initialized" << endl;
    }
}

void SingleMessageVehicleApp::onWSM(BaseFrame1609_4* wsm) {
    // Add this debug line first
    EV << "VehicleApp: onWSM() called at time " << simTime() << endl;

    DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
    if (dsm) {
        EV << "VehicleApp: Successfully cast to DemoSafetyMessage" << endl;
        EV_INFO << "VehicleApp: Received WSM at time "
                << simTime()
                << ", senderPos=" << dsm->getSenderPos()
                << ", recipient=" << dsm->getRecipientAddress()
                << ", timestamp=" << dsm->getTimestamp()
                << endl;
    } else {
        EV << "VehicleApp: Failed to cast to DemoSafetyMessage, wsm type: "
           << wsm->getClassName() << endl;
    }

    // Call parent class method
    DemoBaseApplLayer::onWSM(wsm);
}

} // namespace complex_network
