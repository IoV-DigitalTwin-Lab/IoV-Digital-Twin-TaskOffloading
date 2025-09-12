#include <omnetpp.h>
#include "msgs/Offload_m.h"

using namespace omnetpp;
using namespace iovmini;

class RsuApp : public cSimpleModule {
  protected:
    virtual void handleMessage(cMessage* msg) override {
        if (auto* req = dynamic_cast<OffloadRequest*>(msg)) {
            // forward to Edge via wired gate
            send(req, "edgeOut");
        }
        else if (auto* res = dynamic_cast<OffloadResult*>(msg)) {
            // send back to the originating car via direct send
            int id = res->getSrcModuleId();
            cModule* car = getSimulation()->getModule(id);
            if (car)
                sendDirect(res, car->getSubmodule("app"), "in");
            else
                delete res;
        }
        else delete msg;
    }
};
Define_Module(RsuApp);
