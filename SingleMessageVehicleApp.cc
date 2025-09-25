<<<<<<< HEAD

#include <omnetpp.h>
#include "SingleMessageVehicleApp.h"

using namespace omnetpp;

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
        cModule* rsuModule = nullptr;
        if (networkModule) {
            std::cout << "CONSOLE: DEBUG - Found network module: " << networkModule->getFullName() << std::endl;
            EV << "VehicleApp: DEBUG - Found network module: " << networkModule->getFullName() << endl;

            // Method 1: Try direct submodule access
            rsuModule = networkModule->getSubmodule("rsu", 1);  // Get rsu[1]
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
    if (recipientAddr == myMacAddress) {
        // Only visualize if this is a true unicast (not broadcast)
        DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
        if (dsm) {
            // Emit a custom signal for visualization
            cModule* senderModule = wsm->getSenderModule();
            if (senderModule) {
                cModule* thisModule = getParentModule();
                simsignal_t unicastSignal = registerSignal("validUnicastCommunication");
                thisModule->emit(unicastSignal, senderModule); // Pass sender module pointer
            }
            std::cout << "CONSOLE: Successfully received DemoSafetyMessage (UNICAST)" << std::endl;
        }
        DemoBaseApplLayer::onWSM(wsm);
    } else if (recipientAddr == broadcastAddr) {
        // Accept broadcast, but do not emit unicast visualization
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
=======
#include "SingleMessageVehicleApp.h"

using namespace veins;

namespace complex_network {

Define_Module(SingleMessageVehicleApp);

void SingleMessageVehicleApp::initialize(int stage) {
    DemoBaseApplLayer::initialize(stage);
    if (stage == 0) {
        EV_INFO << "VehicleApp: Initialized" << endl;
        std::cout << "CONSOLE: VehicleApp initialized at " << simTime() << std::endl;

        // Get total number of vehicles and build vehicle registry
        totalVehicles = countVehiclesInSimulation();

        int myId = getParentModule()->getIndex();

        // Random chance this vehicle becomes a sender (e.g., 30%)
        if (uniform(0, 1) < 0.3) {
            // Choose random target vehicle (different from myself)
            do {
                targetVehicleId = intuniform(0, totalVehicles - 1);
            } while (targetVehicleId == myId); // Ensure not targeting myself

            // Schedule first message at random time
            cMessage* sendMsg = new cMessage("sendVehicleMsg");
            double firstSendTime = uniform(5, 20); // Between 5-20 seconds
            scheduleAt(simTime() + firstSendTime, sendMsg);

            std::cout << "CONSOLE: Vehicle[" << myId << "] will send TRUE UNICAST to Vehicle["
                      << targetVehicleId << "] starting at " << (simTime() + firstSendTime) << std::endl;
            EV_INFO << "VehicleApp: Vehicle[" << myId << "] targeting Vehicle["
                    << targetVehicleId << "] for TRUE UNICAST" << endl;
        }
    }
}

void SingleMessageVehicleApp::handleSelfMsg(cMessage* msg) {
    if (strcmp(msg->getName(), "sendVehicleMsg") == 0) {
        int myId = getParentModule()->getIndex();

        // Optionally pick a new random target for each message (10% chance)
        if (uniform(0, 1) < 0.1) {  // 10% chance to change target
            int oldTarget = targetVehicleId;
            do {
                targetVehicleId = intuniform(0, totalVehicles - 1);
            } while (targetVehicleId == myId);

            std::cout << "CONSOLE: Vehicle[" << myId << "] changed target from Vehicle["
                      << oldTarget << "] to Vehicle[" << targetVehicleId << "]" << std::endl;
        }

        // Send true unicast message
        sendUnicastMessage(targetVehicleId);

        std::cout << "CONSOLE: Vehicle[" << myId << "] sent TRUE UNICAST to Vehicle["
                  << targetVehicleId << "] at " << simTime() << std::endl;
        EV_INFO << "VehicleApp: Vehicle[" << myId << "] sent true unicast to Vehicle["
                << targetVehicleId << "] at " << simTime() << endl;

        // Schedule next message at random interval (10-30 seconds)
        double nextSendTime = uniform(10, 30);
        scheduleAt(simTime() + nextSendTime, msg);

    } else {
        DemoBaseApplLayer::handleSelfMsg(msg);
    }
}

void SingleMessageVehicleApp::sendUnicastMessage(int targetVehicleId) {
    // Get target vehicle's MAC address
    LAddress::L2Type targetMac = getMacAddressOfVehicle(targetVehicleId);

    if (targetMac == LAddress::L2NULL()) {
        std::cout << "CONSOLE: Could not find MAC address for Vehicle[" << targetVehicleId << "]" << std::endl;
        return;
    }

    // Create and populate message
    DemoSafetyMessage* wsm = new DemoSafetyMessage();
    populateWSM(wsm);
    wsm->setSenderPos(mobility->getPositionAt(simTime()));

    // Set for TRUE UNICAST transmission
    wsm->setRecipientAddress(targetMac);
    wsm->setBitLength(dataLengthBits);

    // Set message name for identification
    char msgName[50];
    sprintf(msgName, "TrueUnicastTo_%d", targetVehicleId);
    wsm->setName(msgName);

    // Send unicast message
    sendDown(wsm);
}

LAddress::L2Type SingleMessageVehicleApp::getMacAddressOfVehicle(int vehicleId) {
    // Check if we already have the MAC address cached
    if (vehicleMacMap.find(vehicleId) != vehicleMacMap.end()) {
        return vehicleMacMap[vehicleId];
    }

    // Simple approach: Generate MAC address based on vehicle ID
    // This is commonly used in VEINS simulations
    LAddress::L2Type macAddr = LAddress::L2Type(vehicleId + 1); // +1 to avoid 0 address

    // Cache the MAC address for future use
    vehicleMacMap[vehicleId] = macAddr;
    std::cout << "CONSOLE: Generated MAC address for Vehicle[" << vehicleId
              << "]: " << macAddr << std::endl;
    return macAddr;
}

cModule* SingleMessageVehicleApp::findVehicleModule(int vehicleId) {
    // Check cache first
    if (vehicleModuleMap.find(vehicleId) != vehicleModuleMap.end()) {
        return vehicleModuleMap[vehicleId];
    }

    cModule* sim = getSystemModule();
    int currentIndex = 0;

    for (cModule::SubmoduleIterator it(sim); !it.end(); it++) {
        cModule* mod = *it;
        // Check if this module is a vehicle (has mobility)
        if (mod->getSubmodule("mobility") != nullptr) {
            if (currentIndex == vehicleId) {
                // Cache the module for future use
                vehicleModuleMap[vehicleId] = mod;
                return mod;
            }
            currentIndex++;
        }
    }

    return nullptr;
}

int SingleMessageVehicleApp::countVehiclesInSimulation() {
    int count = 0;
    cModule* sim = getSystemModule();

    for (cModule::SubmoduleIterator it(sim); !it.end(); it++) {
        cModule* mod = *it;
        // Check if this module is a vehicle (has mobility)
        if (mod->getSubmodule("mobility") != nullptr) {
            count++;
        }
    }

    std::cout << "CONSOLE: Counted " << count << " vehicles in simulation" << std::endl;
    return count;
}

void SingleMessageVehicleApp::onWSM(BaseFrame1609_4* wsm) {
    std::cout << "CONSOLE: onWSM() called at time " << simTime() << std::endl;

    DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
    if (dsm) {
        int myId = getParentModule()->getIndex();
        std::string msgName = dsm->getName();

        // Check if this is a true unicast message for me
        if (msgName.find("TrueUnicastTo_") == 0) {
            int targetId = std::stoi(msgName.substr(14)); // "TrueUnicastTo_" = 14 chars

            if (targetId == myId) {
                Coord senderPos = dsm->getSenderPos();
                Coord myPos = mobility->getPositionAt(simTime());
                double distance = senderPos.distance(myPos);

                // Get my MAC address for verification
                LAddress::L2Type myMac = getMacAddressOfVehicle(myId);

                std::cout << "CONSOLE: Vehicle[" << myId
                          << "] received TRUE UNICAST message (distance: "
                          << distance << "m) at " << simTime()
                          << " - MAC verified: " << (wsm->getRecipientAddress() == myMac ? "YES" : "NO") << std::endl;
                EV_INFO << "VehicleApp: Vehicle[" << myId
                        << "] received TRUE UNICAST message" << endl;
            }
        }
    }

    DemoBaseApplLayer::onWSM(wsm);
}

void SingleMessageVehicleApp::handleMessage(cMessage* msg) {
    std::cout << "CONSOLE: handleMessage() called with message: " << msg->getName()
              << " at time " << simTime() << std::endl;
    EV << "VehicleApp: handleMessage() called with " << msg->getName() << endl;

    // Check if it's a BaseFrame1609_4 message
    BaseFrame1609_4* wsm = dynamic_cast<BaseFrame1609_4*>(msg);
    if (wsm) {
        std::cout << "CONSOLE: handleMessage received BaseFrame1609_4 - Recipient: "
                  << wsm->getRecipientAddress() << std::endl;
        onWSM(wsm);
        return;
    }

    // Call parent class method
    DemoBaseApplLayer::handleMessage(msg);
}

void SingleMessageVehicleApp::receiveSignal(cComponent* source, simsignal_t signalID, cObject* obj, cObject* details) {
    std::cout << "CONSOLE: receiveSignal() called at time " << simTime() << std::endl;
    EV << "VehicleApp: receiveSignal() called" << endl;
    DemoBaseApplLayer::receiveSignal(source, signalID, obj, details);
}

} // namespace complex_network
>>>>>>> e5db28b (Added Vehicle Parameters)
