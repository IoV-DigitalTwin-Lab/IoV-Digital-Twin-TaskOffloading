#include "VehicleApp.h"

namespace complex_network {

Define_Module(VehicleApp);

void VehicleApp::initialize(int stage) {
    DemoBaseApplLayer::initialize(stage);
    if (stage != 0) return;

    // mobility hookup
    mobility = FindModule<TraCIMobility*>::findSubModule(getParentModule());
    if (mobility) subscribe(mobility->mobilityStateChangedSignal, this);

    // vehicle knobs
    flocHz       = par("initFlocHz").doubleValue();
    txPower_mW   = par("initTxPower_mW").doubleValue();
    heartbeatInterval = par("heartbeatIntervalS").doubleValue();
    taskIntervalMean  = par("taskGenIntervalS").doubleValue();
    jitterFactor      = par("paramJitterFactor").doubleValue();

    // task distribution config
    taskSizeB_min = par("taskSizeB_min").doubleValue();
    taskSizeB_max = par("taskSizeB_max").doubleValue();
    cpuPB_min     = par("cpuPerByte_min").doubleValue();
    cpuPB_max     = par("cpuPerByte_max").doubleValue();
    ddl_min       = par("deadlineS_min").doubleValue();
    ddl_max       = par("deadlineS_max").doubleValue();

    // signals
    sigHeartbeat = registerSignal("vehHeartbeat");
    sigTaskArrive = registerSignal("taskArrive");

    // timers
    hbMsg = new cMessage("vehHeartbeat");
    taskMsg = new cMessage("taskArrival");

    // stagger start
    scheduleAt(simTime() + uniform(0, heartbeatInterval), hbMsg);
    scheduleNextTask();
}

void VehicleApp::handleSelfMsg(cMessage* msg) {
    if (msg == hbMsg) {
        // update live kinematics snapshot
        if (mobility) {
            pos = mobility->getPositionAt(simTime());
            speed = mobility->getSpeed();
        }

        maybeDriftVehicleKnobs(); // optional slow drift

        // HEARTBEAT: vehicle status only (no task meta here)
        EV_INFO << "HB V[" << getParentModule()->getIndex()
                << "] pos=(" << pos.x << "," << pos.y << ") v=" << speed << " m/s"
                << " | Floc=" << flocHz << " Hz, Ptx=" << txPower_mW << " mW"
                << " @t=" << simTime() << endl;

        emit(sigHeartbeat, 1);
        recordHeartbeatScalars();

        // next heartbeat
        scheduleAt(simTime() + heartbeatInterval, hbMsg);
        return;
    }

    if (msg == taskMsg) {
        // TASK ARRIVAL: generate task metadata only now
        const double sizeB      = uniform(taskSizeB_min, taskSizeB_max);
        const double cpuPerByte = uniform(cpuPB_min, cpuPB_max);
        double       deadlineS  = uniform(ddl_min, ddl_max);

        // read current kinematics (for tagging this task)
        if (mobility) {
            pos = mobility->getPositionAt(simTime());
            speed = mobility->getSpeed();
        }
        if (speed > 20.0) deadlineS = std::max(0.05, deadlineS * 0.9);

        EV_INFO << "TASK V[" << getParentModule()->getIndex() << "] "
                << "size=" << sizeB << "B, c/B=" << cpuPerByte
                << ", ddl=" << deadlineS
                << " | at pos=(" << pos.x << "," << pos.y << "), v=" << speed
                << " m/s | Floc=" << flocHz << " Hz, Ptx=" << txPower_mW << " mW"
                << " @t=" << simTime() << endl;

        emit(sigTaskArrive, 1);
        recordTaskScalars(sizeB, cpuPerByte, deadlineS);

        // TODO later: send DTTelemetry to RSU/DT here

        // schedule next task arrival
        scheduleNextTask();
        return;
    }

    DemoBaseApplLayer::handleSelfMsg(msg);
}

void VehicleApp::receiveSignal(cComponent* src, simsignal_t id, cObject* obj, cObject* details) {
    if (mobility && id == mobility->mobilityStateChangedSignal) {
        // we refresh speed/pos on heartbeat/task timers; no action needed here
    }
    DemoBaseApplLayer::receiveSignal(src, id, obj, details);
}

void VehicleApp::scheduleNextTask() {
    // draw next inter-arrival (you can switch to exponential if desired)
    const double dt = taskIntervalMean; // keep mean as-is; replace with e.g., exponential(dt) if needed
    scheduleAt(simTime() + uniform(0.8*dt, 1.2*dt), taskMsg);  // scheduleAt(simTime() + exponential(taskIntervalMean), taskMsg);
}

void VehicleApp::maybeDriftVehicleKnobs() {
    if (jitterFactor <= 0) return;
    auto rw = [this](double v) {
        double delta = uniform(-jitterFactor, jitterFactor) * v;
        return std::max(0.0, v + delta);
    };
    flocHz     = rw(flocHz);
    txPower_mW = rw(txPower_mW);
}

void VehicleApp::recordHeartbeatScalars() {
    recordScalar("veh.FlocHz", flocHz);
    recordScalar("veh.TxPower_mW", txPower_mW);
    recordScalar("veh.speed", speed);
    recordScalar("veh.pos.x", pos.x);
    recordScalar("veh.pos.y", pos.y);
}

void VehicleApp::recordTaskScalars(double sizeB, double cpuPerByte, double ddl) {
    recordScalar("task.sizeB", sizeB);
    recordScalar("task.cpuPerByte", cpuPerByte);
    recordScalar("task.deadlineS", ddl);
}

} // namespace complex_network
