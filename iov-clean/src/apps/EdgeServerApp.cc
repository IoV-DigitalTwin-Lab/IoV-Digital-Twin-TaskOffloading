#include <omnetpp.h>
#include "msgs/Offload_m.h"

using namespace omnetpp;
using namespace iovmini;

class EdgeServerApp : public cSimpleModule {
  private:
    double edgeMips;
    simtime_t extraQ;

  protected:
    virtual void initialize() override {
        edgeMips = par("edgeMips").doubleValue();
        extraQ = par("extraQueueDelay");
    }

    virtual void handleMessage(cMessage* msg) override {
        if (auto* req = dynamic_cast<OffloadRequest*>(msg)) {
            // which RSU index delivered this?
            int inIndex = msg->getArrivalGate()->getIndex();  // instead of any previous trick
            // compute processing time
            simtime_t tproc = req->getCycles() / edgeMips;
            req->setRsuIndex(inIndex);
            // complete processing
            auto* done = new OffloadResult("result");
            done->setSrcModuleId(req->getSrcModuleId());
            done->setRsuIndex(req->getRsuIndex());
            done->setCreated(req->getCreated());
            delete req;
            // send after processing (queue + compute)
            scheduleAt(simTime() + extraQ + tproc, done);
            // send out through the same RSU index when ready
            // (use self-message; on delivery, it enters here again)
        }
        else if (auto* res = dynamic_cast<OffloadResult*>(msg)) {
            res->setFinishedAt(simTime());
            int outIndex = res->getRsuIndex();
            send(res, "rsuOut", outIndex);
        }
        else delete msg;
    }
};
Define_Module(EdgeServerApp);
