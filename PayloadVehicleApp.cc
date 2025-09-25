#include <cstddef>
#include "PayloadVehicleApp.h"
#include <sstream>
#include <iomanip>

using namespace veins;

namespace complex_network {

Define_Module(PayloadVehicleApp);

void PayloadVehicleApp::initialize(int stage) {
    DemoBaseApplLayer::initialize(stage);

    if (stage == 0) {
        messageSent = false;

        // Initialize vehicle parameters (similar to VehicleApp)
        flocHz = par("initFlocHz").doubleValue();
        txPower_mW = par("initTxPower_mW").doubleValue();
        
        // Setup mobility hookup
        mobility = FindModule<TraCIMobility*>::findSubModule(getParentModule());
        if (mobility) {
            subscribe(mobility->mobilityStateChangedSignal, this);
            // Initialize position and speed
            pos = mobility->getPositionAt(simTime());
            speed = mobility->getSpeed();
        }

        EV_INFO << "PayloadVehicleApp: Initialized with vehicle data" << endl;
        std::cout << "CONSOLE: PayloadVehicleApp initialized at " << simTime() << std::endl;
        std::cout << "CONSOLE: PayloadVehicleApp - Vehicle data initialized" << std::endl;

        // Schedule message sending after 10 seconds
        cMessage* sendMsgEvent = new cMessage("sendPayloadMessage");
        scheduleAt(simTime() + 10, sendMsgEvent);
        std::cout << "CONSOLE: PayloadVehicleApp - Scheduled to send payload message at time " << (simTime() + 10) << std::endl;
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

void PayloadVehicleApp::handleSelfMsg(cMessage* msg) {
    if (strcmp(msg->getName(), "sendPayloadMessage") == 0 && !messageSent) {
        std::cout << "CONSOLE: PayloadVehicleApp - Sending payload message..." << std::endl;
        EV << "PayloadVehicleApp: Sending payload message..." << endl;

        // Update vehicle data before sending
        updateVehicleData();
        
        // Create payload with current vehicle data
        std::string vehicleDataPayload = createVehicleDataPayload();

        // Find RSU MAC address and ensure correct handling
        LAddress::L2Type rsuMacAddress = findRSUMacAddress();
        
        // Create DemoSafetyMessage with payload
        DemoSafetyMessage* wsm = new DemoSafetyMessage();
        
        if (rsuMacAddress == 0) {
            std::cout << "CONSOLE: PayloadVehicleApp - RSU MAC not found, using broadcast" << std::endl;
            populateWSM(wsm); // fallback broadcast
        } else {
            std::cout << "CONSOLE: PayloadVehicleApp - Found RSU MAC: " << rsuMacAddress << ", using unicast" << std::endl;
            populateWSM(wsm, rsuMacAddress); // force unicast
        }

        // Set payload using setName as the payload carrier  
        wsm->setName(vehicleDataPayload.c_str());
        wsm->setSenderPos(curPosition);
        wsm->setUserPriority(7);

        // Send the message
        sendDown(wsm);
        messageSent = true;

        std::cout << "CONSOLE: PayloadVehicleApp - Sent payload message (Recipient: " << wsm->getRecipientAddress() << ")" << std::endl;
        std::cout << "CONSOLE: PayloadVehicleApp - Vehicle data payload sent: " << vehicleDataPayload << std::endl;
        EV << "PayloadVehicleApp: Sent vehicle data payload message" << endl;

        delete msg;
    } else {
        DemoBaseApplLayer::handleSelfMsg(msg);
    }
}

void PayloadVehicleApp::onWSM(BaseFrame1609_4* wsm) {
    std::cout << "CONSOLE: PayloadVehicleApp - Received WSM message!" << std::endl;
    EV << "PayloadVehicleApp: Received WSM message!" << endl;

    // Check if this is a response from RSU
    DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
    if (dsm) {
        std::string receivedPayload = dsm->getName(); // Use getName instead of getPayload
        
        std::cout << "CONSOLE: PayloadVehicleApp - DEBUG - Received message: '" << receivedPayload << "'" << std::endl;

        if (!receivedPayload.empty() && 
            (receivedPayload.find("RSU_RESPONSE") != std::string::npos || 
             receivedPayload.find("RSU Response") != std::string::npos)) {
            std::cout << "CONSOLE: =========================================" << std::endl;
            std::cout << "CONSOLE: PayloadVehicleApp - RECEIVED PAYLOAD FROM RSU:" << std::endl;
            std::cout << "CONSOLE: PayloadVehicleApp - Payload: " << receivedPayload << std::endl;
            std::cout << "CONSOLE: PayloadVehicleApp - Received at time: " << simTime() << std::endl;
            std::cout << "CONSOLE: PayloadVehicleApp - Sender Module: " << wsm->getSenderModule()->getFullName() << std::endl;
            std::cout << "CONSOLE: =========================================" << std::endl;

            EV << "PayloadVehicleApp: RECEIVED PAYLOAD: " << receivedPayload << endl;
        } else {
            std::cout << "CONSOLE: PayloadVehicleApp - DEBUG - Message is not an RSU response" << std::endl;
        }
    }

    DemoBaseApplLayer::onWSM(wsm);
}

void PayloadVehicleApp::handleMessage(cMessage* msg) {
    std::cout << "CONSOLE: PayloadVehicleApp handleMessage() called with: " << msg->getName() << std::endl;
    EV << "PayloadVehicleApp: handleMessage() called with " << msg->getName() << endl;

    BaseFrame1609_4* wsm = dynamic_cast<BaseFrame1609_4*>(msg);
    if (wsm) {
        onWSM(wsm);
        return;
    }

    DemoBaseApplLayer::handleMessage(msg);
}

LAddress::L2Type PayloadVehicleApp::findRSUMacAddress() {
    std::cout << "CONSOLE: PayloadVehicleApp - Looking for RSU MAC address..." << std::endl;

    // Get the network module
    cModule* networkModule = getModuleByPath("^.^");
    if (!networkModule) {
        std::cout << "CONSOLE: PayloadVehicleApp ERROR - Network module not found!" << std::endl;
        return 0;
    }

    // Try to find RSU[1] first (SingleMessageRSUApp), then RSU[0] (MyRSUApp)
    for (int i = 1; i >= 0; i--) {  // Start with RSU[1] first
        cModule* rsuModule = networkModule->getSubmodule("rsu", i);
        if (rsuModule) {
            std::cout << "CONSOLE: PayloadVehicleApp - Found RSU[" << i << "]: " << rsuModule->getFullName() << std::endl;

            // Get the MAC interface from the RSU
            DemoBaseApplLayerToMac1609_4Interface* rsuMacInterface =
                FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(rsuModule);

            if (rsuMacInterface) {
                LAddress::L2Type rsuMacAddress = rsuMacInterface->getMACAddress();
                if (rsuMacAddress != 0) {
                    std::cout << "CONSOLE: PayloadVehicleApp - Found RSU[" << i << "] MAC: " << rsuMacAddress << std::endl;
                    return rsuMacAddress;
                }
            } else {
                std::cout << "CONSOLE: PayloadVehicleApp - RSU[" << i << "] MAC interface not found" << std::endl;
            }
        }
    }

    std::cout << "CONSOLE: PayloadVehicleApp ERROR - No valid RSU MAC address found!" << std::endl;
    return 0;
}

void PayloadVehicleApp::updateVehicleData() {
    // Update live kinematics snapshot from mobility
    if (mobility) {
        pos = mobility->getPositionAt(simTime());
        speed = mobility->getSpeed();
    }
    
    std::cout << "CONSOLE: PayloadVehicleApp - Updated vehicle data: pos=(" << pos.x << "," << pos.y 
              << ") speed=" << speed << " flocHz=" << flocHz << " txPower_mW=" << txPower_mW << std::endl;
}

std::string PayloadVehicleApp::createVehicleDataPayload() {
    // Create structured payload with actual vehicle data (similar to recordHeartbeatScalars format)
    std::ostringstream payload;
    
    payload << "VEHICLE_DATA|"
            << "VehID:" << getParentModule()->getIndex() << "|"
            << "Time:" << std::fixed << std::setprecision(3) << simTime().dbl() << "|"
            << "FlocHz:" << std::fixed << std::setprecision(2) << flocHz << "|"
            << "TxPower_mW:" << std::fixed << std::setprecision(2) << txPower_mW << "|"
            << "Speed:" << std::fixed << std::setprecision(2) << speed << "|"
            << "PosX:" << std::fixed << std::setprecision(2) << pos.x << "|"
            << "PosY:" << std::fixed << std::setprecision(2) << pos.y << "|"
            << "MAC:" << myMacAddress;
    
    return payload.str();
}

void PayloadVehicleApp::receiveSignal(cComponent* src, simsignal_t id, cObject* obj, cObject* details) {
    // Handle mobility state changes
    if (mobility && id == mobility->mobilityStateChangedSignal) {
        // Update position and speed when mobility changes
        pos = mobility->getPositionAt(simTime());
        speed = mobility->getSpeed();
    }
    
    DemoBaseApplLayer::receiveSignal(src, id, obj, details);
}

} // namespace complex_network
