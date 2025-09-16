#include "SingleMessageRSUApp.h"

using namespace veins;

namespace complex_network {

Define_Module(SingleMessageRSUApp);

void SingleMessageRSUApp::initialize(int stage) {
    DemoBaseApplLayer::initialize(stage);

    if(stage == 0) {
        messageSent = false;

        // Check if we should send a message
        if(par("sendOnce").boolValue()) {
            // Schedule a self-message to send the message once
            cMessage* msg = new cMessage("sendSingleMessage");
            double sendTime = par("sendTime").doubleValue();
            scheduleAt(simTime() + sendTime, msg);

            EV << "SingleMessageRSU: Scheduled to send message at time " << (simTime() + sendTime) << endl;
        }
    }
}

void SingleMessageRSUApp::handleSelfMsg(cMessage* msg) {
    if (strcmp(msg->getName(), "sendSingleMessage") == 0 && !messageSent) {
        // Create DemoSafetyMessage
        DemoSafetyMessage* wsm = new DemoSafetyMessage();
        populateWSM(wsm);
        wsm->setSenderPos(curPosition);
        wsm->setUserPriority(7); // High priority

        // Send the message
        sendDown(wsm);
        messageSent = true; // Mark as sent

        EV << "SingleMessageRSU: Sent single message at time " << simTime() << endl;

        delete msg;
    } else {
        DemoBaseApplLayer::handleSelfMsg(msg);
    }
}

void SingleMessageRSUApp::onWSM(BaseFrame1609_4* wsm) {
    DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
    if(dsm) {
        EV << "SingleMessageRSU: Received message from vehicle at time " << simTime() << endl;
        // Optional: Process the received message
    }
}

} // namespace complex_network
