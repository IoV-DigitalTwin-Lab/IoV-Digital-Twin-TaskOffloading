#include "SingleMessageRSUApp.h"

using namespace veins;

namespace complex_network {

Define_Module(SingleMessageRSUApp);

void SingleMessageRSUApp::initialize(int stage) {
    // OMNeT++ initializes modules in multiple stages. Here, we handle stage 0 and stage 1.
    DemoBaseApplLayer::initialize(stage);
    if(stage == 0) {
        // Marks that no message has been sent yet
        messageSent = false;
        EV_INFO << "RSUApp: Initialized" << endl;
        std::cout << "CONSOLE: RSUApp initialized at " << simTime() << std::endl;

        if(par("sendOnce").boolValue()) {
            cMessage* msg = new cMessage("sendSingleMessage");
            // If NED parameter sendOnce is true → creates a self-message "sendSingleMessage".
            double sendTime = par("sendTime").doubleValue();
            // Schedules it to trigger at sendTime seconds from now.
            scheduleAt(simTime() + sendTime, msg);
            EV << "SingleMessageRSU: Scheduled to send message at time " << (simTime() + sendTime) << endl;
            std::cout << "CONSOLE: RSU scheduled to send message at time " << (simTime() + sendTime) << std::endl;
        }
    }
    else if (stage == 1) {
        // Get MAC address in stage 1, like DemoBaseApplLayer does
        // At this point, the MAC layer should be fully initialized
        DemoBaseApplLayerToMac1609_4Interface* macInterface =
            FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(getParentModule());

        if (macInterface) {
            myMacAddress = macInterface->getMACAddress();
            std::cout << "CONSOLE: RSU MAC interface found in stage 1, address is: " << myMacAddress << std::endl;
            EV_INFO << "RSU MAC interface found, address: " << myMacAddress << endl;
        } else {
            std::cout << "CONSOLE: ERROR - RSU MAC interface NOT found in stage 1!" << std::endl;
            EV_ERROR << "RSU MAC interface NOT found!" << endl;
            myMacAddress = 0; // Default to 0
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
    EV << "=== RSUApp: onWSM() called at time " << simTime() << " ===" << endl;

    // UNICAST FILTERING: Check if this message is intended for us
    LAddress::L2Type recipientAddr = wsm->getRecipientAddress();
    LAddress::L2Type broadcastAddr = LAddress::L2BROADCAST();

    std::cout << "CONSOLE: RSU DEBUG - Message recipient: " << recipientAddr << std::endl;
    std::cout << "CONSOLE: RSU DEBUG - My MAC address: " << myMacAddress << std::endl;
    std::cout << "CONSOLE: RSU DEBUG - Is broadcast: " << (recipientAddr == broadcastAddr) << std::endl;
    EV << "RSUApp: DEBUG - Message recipient: " << recipientAddr << endl;
    EV << "RSUApp: DEBUG - My MAC address: " << myMacAddress << endl;

    // Only process the message if:
    // 1. It's a broadcast message (recipient == -1), OR
    // 2. It's specifically addressed to us (recipient == myMacAddress)
    if (recipientAddr == myMacAddress) {
        // Only visualize if this is a true unicast (not broadcast)
        DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
        if(dsm) {
            // Emit a custom signal for visualization
            cModule* senderModule = wsm->getSenderModule();
            if (senderModule) {
                cModule* thisModule = getParentModule();
                simsignal_t unicastSignal = registerSignal("validUnicastCommunication");
                thisModule->emit(unicastSignal, senderModule); // Pass sender module pointer
            }
            std::cout << "CONSOLE: RSU successfully received DemoSafetyMessage (UNICAST)" << std::endl;
            EV << "RSU successfully received DemoSafetyMessage (UNICAST)" << endl;
        }
        DemoBaseApplLayer::onWSM(wsm);
    } else if (recipientAddr == broadcastAddr) {
        // Accept broadcast, but do not emit unicast visualization
        DemoBaseApplLayer::onWSM(wsm);
    } else {

        // The MAC layer filters packets before handing them to the upper application layer.
        // If a frame’s recipient MAC doesn’t match the local MAC (and it’s not broadcast), the MAC drops it silently.
        // RSU application code (onWSM()) never even sees the message if it wasn’t addressed to that RSU.

        std::cout << "CONSOLE: RSU - Message IGNORED - not addressed to me (recipient: "
                  << recipientAddr << ", my address: " << myMacAddress << ")" << std::endl;
        EV << "RSUApp: Message IGNORED - not addressed to me (recipient: "
           << recipientAddr << ", my address: " << myMacAddress << ")" << endl;

        // Don't call parent's onWSM for ignored messages
        // This prevents further processing of unicast messages not intended for us
    }
}

void SingleMessageRSUApp::handleMessage(cMessage* msg) {
    std::cout << "CONSOLE: RSU handleMessage() called with message: " << msg->getName()
              << " at time " << simTime() << std::endl;
    EV << "RSUApp: handleMessage() called with " << msg->getName() << endl;

    BaseFrame1609_4* wsm = dynamic_cast<BaseFrame1609_4*>(msg);
    if (wsm) {
        std::cout << "CONSOLE: RSU handleMessage received BaseFrame1609_4!" << std::endl;
        onWSM(wsm);
        return;
    }

    DemoBaseApplLayer::handleMessage(msg);
}

void SingleMessageRSUApp::receiveSignal(cComponent* source, simsignal_t signalID, cObject* obj, cObject* details) {
    std::cout << "CONSOLE: RSU receiveSignal() called at time " << simTime() << std::endl;
    EV << "RSUApp: receiveSignal() called" << endl;
    DemoBaseApplLayer::receiveSignal(source, signalID, obj, details);
}

} // namespace complex_network
