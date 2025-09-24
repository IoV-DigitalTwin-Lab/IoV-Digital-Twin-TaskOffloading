#ifndef __COMPLEX_NETWORK_UNICASTVISUALIZER_H_
#define __COMPLEX_NETWORK_UNICASTVISUALIZER_H_

#include <omnetpp.h>

using namespace omnetpp;

namespace complex_network {

class UnicastVisualizer : public cSimpleModule, public cListener
{
  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    void handleUnicastCommunication(cModule* senderModule, cModule* receiverModule);
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) override;
};

} // namespace complex_network

#endif
