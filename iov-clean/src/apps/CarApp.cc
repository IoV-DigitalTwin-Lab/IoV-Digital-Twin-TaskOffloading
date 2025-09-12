#include <omnetpp.h>
#include "veins/modules/mobility/traci/TraCIMobility.h"
#include "msgs/Offload_m.h"

using namespace omnetpp;
using namespace iovmini;

class CarApp : public cSimpleModule {
  private:
    cMessage* tick = nullptr;
    veins::TraCIMobility* mobility = nullptr;

    simtime_t taskInterval;
    double taskCycles;
    int taskBytes;
    double localMips;
    double uplinkBps;

  protected:
    virtual void initialize() override {
        mobility = check_and_cast<veins::TraCIMobility*>(getParentModule()->getSubmodule("mobility"));
        taskInterval = par("taskInterval");
        taskCycles = par("taskCycles").doubleValue();
        taskBytes = par("taskBytes");
        localMips = par("localMips").doubleValue();
        uplinkBps = par("uplinkBps").doubleValue();

        tick = new cMessage("tick");
        scheduleAt(simTime() + uniform(0, taskInterval), tick);
        EV_INFO << "CarApp up for " << mobility->getExternalId() << "\n";
    }

    virtual void handleMessage(cMessage* msg) override {
        if (msg == tick) {
            makeAndDecideTask();
            scheduleAt(simTime() + taskInterval, tick);
        } else if (auto* res = dynamic_cast<OffloadResult*>(msg)) {
            simtime_t rtt = simTime() - res->getCreated();
            EV_INFO << "Task done via RSU[" << res->getRsuIndex()
                    << "], end-to-end latency=" << rtt << "\n";
            delete res;
        } else {
            delete msg;
        }
    }

    void makeAndDecideTask() {
        // Simple estimates (wire model)
        double t_local = taskCycles / localMips;                 // seconds
        double t_uplink = 8.0 * taskBytes / uplinkBps;           // seconds (rough)
        // Edge MIPS from the EdgeServer module
        cModule* edge = getSimulation()->getSystemModule()->getSubmodule("edge");
        double edgeMips = edge->par("edgeMips").doubleValue();
        double t_edge = taskCycles / edgeMips;

        // two-way wired hop ≈ 4 ms in our channels (2 ms each way)
        double t_offload_est = t_uplink + t_edge + 0.004;

        if (t_offload_est < t_local) {
            sendToNearestRsu();
        } else {
            // local processing
            simtime_t done = simTime() + t_local;
            EV_INFO << "Processing locally, finish at " << done << "\n";
        }
    }

    void sendToNearestRsu() {
        // find nearest RSU (by configured posX/posY)
        cModule* top = getSimulation()->getSystemModule();
        cModule* rsuVec = nullptr; // not needed, we iterate directly
        int best = -1;
        double bestd2 = 1e100;

        double x = mobility->getPositionAt(simTime()).x;
        double y = mobility->getPositionAt(simTime()).y;

        int idx = 0;
        while (cModule* rsu = top->getSubmodule("rsu", idx++)) {
            double rx = rsu->par("posX").doubleValue();
            double ry = rsu->par("posY").doubleValue();
            double d2 = (x - rx)*(x - rx) + (y - ry)*(y - ry);
            if (d2 < bestd2) { bestd2 = d2; best = idx - 1; }
        }
        if (best < 0) { EV_WARN << "No RSU found\n"; return; }

        auto* req = new OffloadRequest("offload");
        req->setSrcModuleId(getParentModule()->getId());
        req->setCycles(taskCycles);
        req->setBytes(taskBytes);
        req->setCreated(simTime());
        req->setRsuIndex(best);

        cModule* rsu = top->getSubmodule("rsu", best);
        sendDirect(req, rsu, "inFromCar");   // wired hop: Car -> RSU
        EV_INFO << "Offloaded to RSU[" << best << "]\n";
    }

    virtual void finish() override {
        cancelAndDelete(tick);
    }
};
Define_Module(CarApp);
