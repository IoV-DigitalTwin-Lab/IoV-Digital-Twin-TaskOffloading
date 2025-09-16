#include "MyRSUApp.h"
using namespace veins;

namespace complex_network {

Define_Module(MyRSUApp);

void MyRSUApp::initialize(int stage) {
    BaseApplLayer::initialize(stage);  // <-- changed here
    if(stage == 0) {
        double interval = par("beaconInterval").doubleValue();

        cMessage* sendMsg = new cMessage("sendMessage");
        scheduleAt(simTime() + 2.0, sendMsg);

        EV << "RSU initialized with beacon interval: " << interval << "s" << endl;
    }
}

void MyRSUApp::handleSelfMsg(cMessage* msg) {
    if(strcmp(msg->getName(), "sendMessage") == 0) {
        // Create and send your message
        DemoSafetyMessage* wsm = new DemoSafetyMessage();
        populateWSM(wsm);
        wsm->setSenderPos(curPosition);
        wsm->setUserPriority(par("beaconUserPriority").intValue());
        sendDown(wsm);

        EV << "RSU: Sent message at time " << simTime() << endl;

        double interval = par("beaconInterval").doubleValue();
        scheduleAt(simTime() + interval, msg);
    } else {
        BaseApplLayer::handleSelfMsg(msg);  // <-- changed here
    }
}

void MyRSUApp::onWSM(BaseFrame1609_4* wsm) {
    DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
    if(dsm) {
        EV << "RSU: Received message from vehicle at time " << simTime() << endl;
    }
}

} // namespace complex_network
