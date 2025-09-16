#include "SingleMessageRSUApp.h"

using namespace veins;

namespace complex_network {

Define_Module(SingleMessageRSUApp);

void SingleMessageRSUApp::initialize(int stage) {
    DemoBaseApplLayer::initialize(stage);
    if(stage == 0) {
        messageSent = false;
        EV_INFO << "RSUApp: Initialized" << endl;
        std::cout << "CONSOLE: RSUApp initialized at " << simTime() << std::endl;

        // Check if we should send a message
        if(par("sendOnce").boolValue()) {
            // Schedule a self-message to send the message once
            cMessage* msg = new cMessage("sendSingleMessage");
            double sendTime = par("sendTime").doubleValue();
            scheduleAt(simTime() + sendTime, msg);
            EV << "SingleMessageRSU: Scheduled to send message at time " << (simTime() + sendTime) << endl;
            std::cout << "CONSOLE: RSU scheduled to send message at time " << (simTime() + sendTime) << std::endl;
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

        std::string senderName = getParentModule()->getFullName();
        EV << "SingleMessageRSU: " << senderName << " sent single message at time " << simTime() << endl;
        std::cout << "CONSOLE: RSU " << senderName << " sent message at " << simTime() << std::endl;
        delete msg;
    } else {
        DemoBaseApplLayer::handleSelfMsg(msg);
    }
}

void SingleMessageRSUApp::onWSM(BaseFrame1609_4* wsm) {
    // Force console output for debugging
    std::cout << "CONSOLE: RSU onWSM() called at time " << simTime() << std::endl;

    // Force EV output
    EV << "=== RSUApp: onWSM() called at time " << simTime() << " ===" << endl;

    DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
    if(dsm) {
        std::cout << "CONSOLE: RSU successfully received DemoSafetyMessage" << std::endl;

        // Get sender information
        cModule* senderModule = wsm->getSenderModule();
        std::string senderName = senderModule ? senderModule->getFullName() : "(unknown)";

        std::cout << "CONSOLE: RSU received DemoSafetyMessage from " << senderName
                  << " at time " << simTime()
                  << " from pos=" << dsm->getSenderPos() << std::endl;

        EV_INFO << "RSUApp: RECEIVED MESSAGE at time " << simTime()
                << " from sender module: " << senderName
                << " at pos=" << dsm->getSenderPos() << endl;

        // Also use regular EV
        EV << "RSUApp: Message received successfully!" << endl;

        // Optionally, process the received message further here
    } else {
        std::cout << "CONSOLE: RSU failed to cast to DemoSafetyMessage" << std::endl;
        EV << "RSUApp: Received non-DemoSafetyMessage" << endl;
    }

    // Always call parent
    DemoBaseApplLayer::onWSM(wsm);
}

void SingleMessageRSUApp::handleMessage(cMessage* msg) {
    std::cout << "CONSOLE: RSU handleMessage() called with message: " << msg->getName()
              << " at time " << simTime() << std::endl;
    EV << "RSUApp: handleMessage() called with " << msg->getName() << endl;

    // Check if it's a BaseFrame1609_4 message
    BaseFrame1609_4* wsm = dynamic_cast<BaseFrame1609_4*>(msg);
    if (wsm) {
        std::cout << "CONSOLE: RSU handleMessage received BaseFrame1609_4!" << std::endl;
        onWSM(wsm);
        return;
    }

    // Call parent class method
    DemoBaseApplLayer::handleMessage(msg);
}

void SingleMessageRSUApp::receiveSignal(cComponent* source, simsignal_t signalID, cObject* obj, cObject* details) {
    std::cout << "CONSOLE: RSU receiveSignal() called at time " << simTime() << std::endl;
    EV << "RSUApp: receiveSignal() called" << endl;
    DemoBaseApplLayer::receiveSignal(source, signalID, obj, details);
}

} // namespace complex_network
