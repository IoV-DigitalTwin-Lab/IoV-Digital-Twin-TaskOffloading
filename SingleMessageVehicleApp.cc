#include "SingleMessageVehicleApp.h"

using namespace veins;

namespace complex_network {

Define_Module(SingleMessageVehicleApp);

void SingleMessageVehicleApp::initialize(int stage) {
    DemoBaseApplLayer::initialize(stage);
    if (stage == 0) {
        EV_INFO << "VehicleApp: Initialized" << endl;
        std::cout << "CONSOLE: VehicleApp initialized at " << simTime() << std::endl;

        // Get our own MAC address for comparison
        myMacAddress = FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(getParentModule())->getMACAddress();
        std::cout << "CONSOLE: My MAC address is: " << myMacAddress << std::endl;

        // Schedule a self-message to send the message once
        cMessage* sendMsgEvent = new cMessage("sendSingleMessage");
        scheduleAt(simTime() + 7, sendMsgEvent); // send after 7s
    }
}

void SingleMessageVehicleApp::handleSelfMsg(cMessage* msg) {
    if (strcmp(msg->getName(), "sendSingleMessage") == 0) {
        std::cout << "CONSOLE: handleSelfMsg - sendSingleMessage triggered" << std::endl;
        EV << "VehicleApp: handleSelfMsg - sendSingleMessage triggered" << endl;

        // Get the MAC address of RSU[1] for unicast
        LAddress::L2Type rsu1MacAddress = LAddress::L2Type();
        bool foundRSU = false;

        // Debug: Print all available modules first
        std::cout << "CONSOLE: DEBUG - Looking for RSU modules..." << std::endl;
        EV << "VehicleApp: DEBUG - Looking for RSU modules..." << endl;

        // Try multiple methods to find RSU[1]
        cModule* networkModule = getModuleByPath("^.^");  // Get the network module
        if (networkModule) {
            std::cout << "CONSOLE: DEBUG - Found network module: " << networkModule->getFullName() << std::endl;
            EV << "VehicleApp: DEBUG - Found network module: " << networkModule->getFullName() << endl;

            // Method 1: Try direct submodule access
            cModule* rsuModule = networkModule->getSubmodule("rsu", 1);  // Get rsu[1]
            if (rsuModule) {
                std::cout << "CONSOLE: DEBUG - Found RSU[1] module: " << rsuModule->getFullPath() << std::endl;
                EV << "VehicleApp: DEBUG - Found RSU[1] module: " << rsuModule->getFullPath() << endl;

                // Find the MAC interface in RSU[1] - CORRECTED METHOD
                DemoBaseApplLayerToMac1609_4Interface* rsuMacInterface =
                    FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(rsuModule);
                if (rsuMacInterface) {
                    rsu1MacAddress = rsuMacInterface->getMACAddress();
                    foundRSU = true;
                    std::cout << "CONSOLE: DEBUG - Found RSU[1] MAC interface, address: " << rsu1MacAddress << std::endl;
                    EV << "VehicleApp: DEBUG - Found RSU[1] MAC interface, address: " << rsu1MacAddress << endl;
                } else {
                    std::cout << "CONSOLE: DEBUG - Could not find MAC interface in RSU[1], trying alternative method" << std::endl;
                    EV << "VehicleApp: DEBUG - Could not find MAC interface in RSU[1]" << endl;

                    // Alternative method: Search in NIC submodule
                    cModule* nicModule = rsuModule->getSubmodule("nic");
                    if (nicModule) {
                        DemoBaseApplLayerToMac1609_4Interface* altMacInterface =
                            FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(nicModule);
                        if (altMacInterface) {
                            rsu1MacAddress = altMacInterface->getMACAddress();
                            foundRSU = true;
                            std::cout << "CONSOLE: DEBUG - Found RSU[1] MAC via alternative method: " << rsu1MacAddress << std::endl;
                        }
                    }
                }
            } else {
                std::cout << "CONSOLE: DEBUG - Could not find RSU[1] submodule" << std::endl;
                EV << "VehicleApp: DEBUG - Could not find RSU[1] submodule" << endl;

                // Method 2: Try direct path
                rsuModule = getModuleByPath("^.^.rsu[1]");
                if (rsuModule) {
                    std::cout << "CONSOLE: DEBUG - Found RSU[1] via direct path" << std::endl;
                    DemoBaseApplLayerToMac1609_4Interface* rsuMacInterface =
                        FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(rsuModule);
                    if (rsuMacInterface) {
                        rsu1MacAddress = rsuMacInterface->getMACAddress();
                        foundRSU = true;
                        std::cout << "CONSOLE: DEBUG - MAC address via direct path: " << rsu1MacAddress << std::endl;
                    }
                }
            }
        } else {
            std::cout << "CONSOLE: DEBUG - Could not find network module" << std::endl;
            EV << "VehicleApp: DEBUG - Could not find network module" << endl;
        }

        // Create the safety message
        DemoSafetyMessage* wsm = new DemoSafetyMessage();

        // Debug: Check broadcast address
        LAddress::L2Type broadcastAddr = LAddress::L2BROADCAST();
        std::cout << "CONSOLE: DEBUG - Broadcast address is: " << broadcastAddr << std::endl;
        EV << "VehicleApp: DEBUG - Broadcast address is: " << broadcastAddr << endl;

        if (foundRSU) {
            std::cout << "CONSOLE: DEBUG - About to call populateWSM with recipient: " << rsu1MacAddress << std::endl;
            EV << "VehicleApp: DEBUG - About to call populateWSM with recipient: " << rsu1MacAddress << endl;

            // Check if the address is valid (not broadcast and not empty)
            if (rsu1MacAddress != broadcastAddr && rsu1MacAddress != LAddress::L2Type()) {
                populateWSM(wsm, rsu1MacAddress);
                std::cout << "CONSOLE: DEBUG - populateWSM called for UNICAST to: " << rsu1MacAddress << std::endl;
                EV << "VehicleApp: DEBUG - populateWSM called for UNICAST to: " << rsu1MacAddress << endl;

                // Store the intended recipient for our own reference
                targetMacAddress = rsu1MacAddress;

                // Double-check the recipient address after populate
                std::cout << "CONSOLE: DEBUG - Message recipient after populate: " << wsm->getRecipientAddress() << std::endl;
                EV << "VehicleApp: DEBUG - Message recipient after populate: " << wsm->getRecipientAddress() << endl;

            } else {
                std::cout << "CONSOLE: DEBUG - Invalid MAC address, falling back to broadcast" << std::endl;
                populateWSM(wsm);
                targetMacAddress = broadcastAddr; // Mark as broadcast
            }
        } else {
            std::cout << "CONSOLE: DEBUG - RSU not found, using broadcast" << std::endl;
            EV << "VehicleApp: DEBUG - RSU not found, using broadcast" << endl;
            populateWSM(wsm);
            targetMacAddress = broadcastAddr; // Mark as broadcast
        }

        // Set additional properties
        wsm->setSenderPos(curPosition);
        wsm->setUserPriority(7);

        // Final debug before sending
        std::cout << "CONSOLE: DEBUG - Final message details before sending:" << std::endl;
        std::cout << "CONSOLE: DEBUG - Recipient: " << wsm->getRecipientAddress() << std::endl;
        std::cout << "CONSOLE: DEBUG - Is broadcast: " << (wsm->getRecipientAddress() == broadcastAddr) << std::endl;
        EV << "VehicleApp: DEBUG - Final recipient: " << wsm->getRecipientAddress() << endl;

        sendDown(wsm);
        delete msg;
    } else {
        DemoBaseApplLayer::handleSelfMsg(msg);
    }
}

void SingleMessageVehicleApp::onWSM(BaseFrame1609_4* wsm) {
    std::cout << "CONSOLE: onWSM() called at time " << simTime() << std::endl;
    EV << "=== VehicleApp: onWSM() called at time " << simTime() << " ===" << endl;

    // UNICAST FILTERING: Check if this message is intended for us
    LAddress::L2Type recipientAddr = wsm->getRecipientAddress();
    LAddress::L2Type broadcastAddr = LAddress::L2BROADCAST();

    std::cout << "CONSOLE: DEBUG - Message recipient: " << recipientAddr << std::endl;
    std::cout << "CONSOLE: DEBUG - My MAC address: " << myMacAddress << std::endl;
    std::cout << "CONSOLE: DEBUG - Is broadcast: " << (recipientAddr == broadcastAddr) << std::endl;
    EV << "VehicleApp: DEBUG - Message recipient: " << recipientAddr << endl;
    EV << "VehicleApp: DEBUG - My MAC address: " << myMacAddress << endl;

    // Only process the message if:
    // 1. It's a broadcast message (recipient == -1), OR
    // 2. It's specifically addressed to us (recipient == myMacAddress)
    if (recipientAddr == broadcastAddr || recipientAddr == myMacAddress) {
        std::cout << "CONSOLE: Message accepted - addressed to me or broadcast" << std::endl;
        EV << "VehicleApp: Message accepted - addressed to me or broadcast" << endl;

        DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
        if (dsm) {
            std::cout << "CONSOLE: Successfully received DemoSafetyMessage" << std::endl;
            EV_INFO << "VehicleApp: RECEIVED MESSAGE at time "
                    << simTime()
                    << " from sender at pos=" << dsm->getSenderPos()
                    << " (recipient: " << recipientAddr << ")"
                    << endl;
            EV << "VehicleApp: Message received successfully!" << endl;
        } else {
            std::cout << "CONSOLE: Failed to cast to DemoSafetyMessage" << std::endl;
            EV << "VehicleApp: Received non-DemoSafetyMessage" << endl;
        }

        // Call parent's onWSM only for accepted messages
        DemoBaseApplLayer::onWSM(wsm);
    } else {
        std::cout << "CONSOLE: Message IGNORED - not addressed to me (recipient: "
                  << recipientAddr << ", my address: " << myMacAddress << ")" << std::endl;
        EV << "VehicleApp: Message IGNORED - not addressed to me (recipient: "
           << recipientAddr << ", my address: " << myMacAddress << ")" << endl;

        // Don't call parent's onWSM for ignored messages
        // This prevents further processing of unicast messages not intended for us
    }
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
