#include "UnicastVisualizer.h"

namespace complex_network {

Define_Module(UnicastVisualizer);

void UnicastVisualizer::initialize() {
    // Subscribe to the custom signal emitted by RSU and Vehicle apps
    getParentModule()->subscribe("validUnicastCommunication", this);
}

void UnicastVisualizer::handleMessage(cMessage *msg) {
    // No self-messages expected
    delete msg;
}

void UnicastVisualizer::handleUnicastCommunication(cModule* senderModule, cModule* receiverModule) {
     // Draw a blue dotted line between sender and receiver using cDisplayString
     if (!senderModule || !receiverModule) return;

     // Remove any previous custom arrow by setting it to empty
     receiverModule->getDisplayString().setTagArg("arrows", 0, "");

     // Draw a blue dotted arrow from sender to receiver
     std::string arrowSpec = std::string("m, ") + std::to_string(senderModule->getId()) + ",blue,2,dotted";
     receiverModule->getDisplayString().setTagArg("arrows", 0, arrowSpec.c_str());

     EV << "UnicastVisualizer: Drawing blue dotted line from "
         << senderModule->getFullPath() << " to " << receiverModule->getFullPath() << endl;
}

void UnicastVisualizer::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) {
    // Only handle our custom signal
    cModule* senderModule = dynamic_cast<cModule*>(obj);
    cModule* receiverModule = getParentModule();
    if (senderModule && receiverModule) {
        handleUnicastCommunication(senderModule, receiverModule);
    }
}

} // namespace complex_network
