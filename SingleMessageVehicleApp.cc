#include "SingleMessageVehicleApp.h"

using namespace veins;

namespace complex_network {

Define_Module(SingleMessageVehicleApp);

void SingleMessageVehicleApp::initialize(int stage) {
    DemoBaseApplLayer::initialize(stage);
    if (stage == 0) {
        EV_INFO << "VehicleApp: Initialized" << endl;
        std::cout << "CONSOLE: VehicleApp initialized at " << simTime() << std::endl;
    }
}

void SingleMessageVehicleApp::onWSM(BaseFrame1609_4* wsm) {
    // Force console output for debugging
    std::cout << "CONSOLE: onWSM() called at time " << simTime() << std::endl;
    
    // Force EV output
    EV << "=== VehicleApp: onWSM() called at time " << simTime() << " ===" << endl;
    
    DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
    if (dsm) {
        std::cout << "CONSOLE: Successfully received DemoSafetyMessage" << std::endl;
        
        EV_INFO << "VehicleApp: RECEIVED MESSAGE at time "
                << simTime()
                << " from sender at pos=" << dsm->getSenderPos()
                << endl;
                
        // Also use regular EV
        EV << "VehicleApp: Message received successfully!" << endl;
    } else {
        std::cout << "CONSOLE: Failed to cast to DemoSafetyMessage" << std::endl;
        EV << "VehicleApp: Received non-DemoSafetyMessage" << endl;
    }
    
    // Always call parent
    DemoBaseApplLayer::onWSM(wsm);
}

void SingleMessageVehicleApp::handleMessage(cMessage* msg) {
    std::cout << "CONSOLE: handleMessage() called with message: " << msg->getName() 
              << " at time " << simTime() << std::endl;
    EV << "VehicleApp: handleMessage() called with " << msg->getName() << endl;
    
    // Check if it's a BaseFrame1609_4 message
    BaseFrame1609_4* wsm = dynamic_cast<BaseFrame1609_4*>(msg);
    if (wsm) {
        std::cout << "CONSOLE: handleMessage received BaseFrame1609_4!" << std::endl;
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