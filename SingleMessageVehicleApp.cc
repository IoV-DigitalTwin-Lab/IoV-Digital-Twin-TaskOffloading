#include "SingleMessageVehicleApp.h"

using namespace veins;

namespace complex_network {

Define_Module(SingleMessageVehicleApp);

void SingleMessageVehicleApp::initialize(int stage) {
    DemoBaseApplLayer::initialize(stage);
    if (stage == 0) {
        EV_INFO << "VehicleApp: Initialized" << endl;
        std::cout << "CONSOLE: VehicleApp initialized at " << simTime() << std::endl;

        // Schedule a self-message to send the message once
        cMessage* sendMsgEvent = new cMessage("sendSingleMessage");
        scheduleAt(simTime() + 7, sendMsgEvent); // send after 7s
    }
}

void SingleMessageVehicleApp::handleSelfMsg(cMessage* msg) {
    if (strcmp(msg->getName(), "sendSingleMessage") == 0) {

        // Get the MAC address of RSU[1] for unicast
        LAddress::L2Type rsu1MacAddress = LAddress::L2Type();
        bool foundRSU = false;

        cModule* rsuModule = getModuleByPath("rsu[1]");
        if (rsuModule) {
            // Find the MAC interface in RSU[1]
            DemoBaseApplLayerToMac1609_4Interface* rsuMacInterface =
                FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(rsuModule);
            if (rsuMacInterface) {
                rsu1MacAddress = rsuMacInterface->getMACAddress();
                foundRSU = true;
                EV << "VehicleApp: Found RSU[1] MAC address: " << rsu1MacAddress << endl;
                std::cout << "CONSOLE: Found RSU[1] MAC address: " << rsu1MacAddress << std::endl;
            } else {
                EV << "VehicleApp: Could not find MAC interface in RSU[1]" << endl;
                std::cout << "CONSOLE: Could not find MAC interface in RSU[1]" << std::endl;
            }
        }

        // Create the safety message
        DemoSafetyMessage* wsm = new DemoSafetyMessage();

        if (foundRSU) {
            // Use unicast to specific RSU
            populateWSM(wsm, rsu1MacAddress, 0); // Use the 3-parameter version for unicast
            EV << "VehicleApp: Sending unicast message to RSU[1] at time " << simTime() << endl;
            std::cout << "CONSOLE: Sending unicast message to RSU[1] at time " << simTime() << std::endl;
        } else {
            // Fallback to broadcast
            populateWSM(wsm); // Use broadcast (no recipient specified)
            EV << "VehicleApp: RSU[1] not found, broadcasting message at time " << simTime() << endl;
            std::cout << "CONSOLE: RSU[1] not found, broadcasting message at time " << simTime() << std::endl;
        }

        wsm->setSenderPos(curPosition);
        wsm->setUserPriority(7);

        sendDown(wsm);
        delete msg;
    } else {
        DemoBaseApplLayer::handleSelfMsg(msg);
    }
}

void SingleMessageVehicleApp::onWSM(BaseFrame1609_4* wsm) {
    std::cout << "CONSOLE: onWSM() called at time " << simTime() << std::endl;
    EV << "=== VehicleApp: onWSM() called at time " << simTime() << " ===" << endl;

    DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
    if (dsm) {
        std::cout << "CONSOLE: Successfully received DemoSafetyMessage" << std::endl;
        EV_INFO << "VehicleApp: RECEIVED MESSAGE at time "
                << simTime()
                << " from sender at pos=" << dsm->getSenderPos()
                << endl;
        EV << "VehicleApp: Message received successfully!" << endl;
    } else {
        std::cout << "CONSOLE: Failed to cast to DemoSafetyMessage" << std::endl;
        EV << "VehicleApp: Received non-DemoSafetyMessage" << endl;
    }

    DemoBaseApplLayer::onWSM(wsm);
}

void SingleMessageVehicleApp::handleMessage(cMessage* msg) {
    std::cout << "CONSOLE: handleMessage() called with message: " << msg->getName()
              << " at time " << simTime() << std::endl;
    EV << "VehicleApp: handleMessage() called with " << msg->getName() << endl;

    BaseFrame1609_4* wsm = dynamic_cast<BaseFrame1609_4*>(msg);
    if (wsm) {
        std::cout << "CONSOLE: handleMessage received BaseFrame1609_4!" << std::endl;
        onWSM(wsm);
        return;
    }

    DemoBaseApplLayer::handleMessage(msg);
}

void SingleMessageVehicleApp::receiveSignal(cComponent* source, simsignal_t signalID, cObject* obj, cObject* details) {
    std::cout << "CONSOLE: receiveSignal() called at time " << simTime() << std::endl;
    EV << "VehicleApp: receiveSignal() called" << endl;
    DemoBaseApplLayer::receiveSignal(source, signalID, obj, details);
}

} // namespace complex_network
