#ifndef TASKGENERATIONCONTROL_H
#define TASKGENERATIONCONTROL_H

#include <omnetpp.h>

namespace complex_network {

using namespace omnetpp;

class TaskGenerationControl : public cSimpleModule {
protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void handleParameterChange(const char *parname) override;
    
private:
    void broadcastManualTaskToVehicles(const std::string &taskType);
    void generateTaskForVehicle(int vehicleId, const std::string &taskType);

    int pendingVehicleId = -1;
    std::string pendingTaskType;
    int pendingRetriesLeft = 0;
    static constexpr int kMaxVehicleLookupRetries = 20;
};

} // namespace complex_network

#endif // TASKGENERATIONCONTROL_H
