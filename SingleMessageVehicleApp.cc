
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

                // Schedule a self-message to send the message once
        cMessage* sendMsgEvent = new cMessage("sendSingleMessage");
//        scheduleAt(simTime() + 7, sendMsgEvent); // send after 7s
        scheduleAt(simTime() + par("sendTime").doubleValue(), new cMessage("sendVehicleMsg"));
        
    }
    else if (stage == 1) {
        // Get MAC address in stage 1
        DemoBaseApplLayerToMac1609_4Interface* macInterface =
            FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(getParentModule());

        if (macInterface) {
            myMacAddress = macInterface->getMACAddress();
            std::cout << "CONSOLE: PayloadVehicleApp MAC address: " << myMacAddress << std::endl;
            EV_INFO << "PayloadVehicleApp MAC address: " << myMacAddress << endl;
        } else {
            std::cout << "CONSOLE: ERROR - PayloadVehicleApp MAC interface NOT found!" << std::endl;
            myMacAddress = 0;
        }
    }
    
}

void SingleMessageVehicleApp::handleSelfMsg(cMessage* msg) {
    // Unified V2V / V2RSU unicast sender
    if (msg->isSelfMessage() && strcmp(msg->getName(), "sendVehicleMsg") == 0) {
        delete msg;

        const std::string kind = par("targetKind").stdstringValue(); // "vehicle" or "rsu"
        const auto bcast = LAddress::L2BROADCAST();
        LAddress::L2Type dst;

        if (kind == "vehicle") {
            int idx = par("targetVehicleIndex");
            dst = getVehicleMacByIndex(idx);
        } else { // "rsu"
            int idx = par("targetRsuIndex");
            dst = getRsuMacByIndex(idx);
        }

        auto* wsm = new DemoSafetyMessage(kind == "vehicle" ? "TrueUnicastV2V" : "TrueUnicastV2I");

        if (dst != bcast && dst != LAddress::L2Type()) {
            // TRUE MAC unicast
            populateWSM(wsm, dst);
            targetMacAddress = dst;                // keep for debug/metrics
            wsm->setSenderPos(curPosition);
            wsm->setUserPriority(7);

            EV_INFO << "Unicast (" << kind << ") to " << dst << " at " << simTime() << endl;
            std::cout << "CONSOLE: Unicast (" << kind << ") to " << dst
                      << " at " << simTime() << std::endl;

            sendDown(wsm);
        } else {
            EV_WARN << "Destination MAC not resolved for kind=" << kind << "; not sending.\n";
            std::cout << "CONSOLE: Destination MAC not resolved for kind=" << kind
                      << "; not sending." << std::endl;
            delete wsm;
        }
        return;
    }

    // Fallback to base for anything else
    DemoBaseApplLayer::handleSelfMsg(msg);
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

// --- Helper: resolve vehicle MAC by index (member function definition) ---
veins::LAddress::L2Type complex_network::SingleMessageVehicleApp::getVehicleMacByIndex(int index) {
    omnetpp::cModule* net = getParentModule()->getParentModule();   // up to network (e.g., ComplexNetwork)
    if (!net || index < 0) return veins::LAddress::L2Type();

    omnetpp::cModule* veh = net->getSubmodule("node", index);       // vehicles live under node[]
    if (!veh) return veins::LAddress::L2Type();

    if (auto* macIf =
            veins::FindModule<veins::DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(veh))
        return macIf->getMACAddress();

    if (auto* nic = veh->getSubmodule("nic")) {
        if (auto* macIf2 =
                veins::FindModule<veins::DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(nic))
            return macIf2->getMACAddress();
    }
    return veins::LAddress::L2Type();
}

// --- Helper: resolve RSU MAC by index (member function definition) ---
veins::LAddress::L2Type complex_network::SingleMessageVehicleApp::getRsuMacByIndex(int index) {
    omnetpp::cModule* net = getParentModule()->getParentModule();   // up to network
    if (!net || index < 0) return veins::LAddress::L2Type();

    omnetpp::cModule* rsu = net->getSubmodule("rsu", index);        // RSUs live under rsu[]
    if (!rsu) return veins::LAddress::L2Type();

    if (auto* macIf =
            veins::FindModule<veins::DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(rsu))
        return macIf->getMACAddress();

    if (auto* nic = rsu->getSubmodule("nic")) {
        if (auto* macIf2 =
                veins::FindModule<veins::DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(nic))
            return macIf2->getMACAddress();
    }
    return veins::LAddress::L2Type();
}



