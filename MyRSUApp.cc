#include "MyRSUApp.h"

using namespace veins;

namespace complex_network {

Define_Module(MyRSUApp);

void MyRSUApp::initialize(int stage) {
    DemoBaseApplLayer::initialize(stage);

    if (stage == 0) {
        // Schedule first message after 2 seconds
        cMessage* sendMsg = new cMessage("sendMessage");
        scheduleAt(simTime() + 2.0, sendMsg);
    }
}

void MyRSUApp::handleSelfMsg(cMessage* msg) {
    if (strcmp(msg->getName(), "sendMessage") == 0) {
        // Create and send message
        DemoSafetyMessage* wsm = new DemoSafetyMessage();
        populateWSM(wsm);
        wsm->setSenderPos(curPosition);
        sendDown(wsm);

        EV << "RSU: Sent message at time " << simTime() << endl;

        // Schedule next message in 5 seconds
        scheduleAt(simTime() + 5.0, msg);
    } else {
        DemoBaseApplLayer::handleSelfMsg(msg);
    }
}

void MyRSUApp::onWSM(BaseFrame1609_4* wsm) {
    DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
    if (dsm) {
        EV << "RSU: Received message from vehicle at time " << simTime() << endl;
    }
}

} // namespace communication_network
