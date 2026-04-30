#include "TaskGenerationControl.h"

namespace complex_network {

Define_Module(TaskGenerationControl);

void TaskGenerationControl::initialize() {
    // Create a global button on the network canvas
    cCanvas *canvas = getSystemModule()->getCanvas();
    if (canvas) {
        cGroupFigure *btnGroup = new cGroupFigure("GlobalGenerateTaskBtn");

        cRectangleFigure *rect = new cRectangleFigure("bg");
        rect->setBounds(cFigure::Rectangle(50, 50, 150, 40));
        rect->setFillColor(cFigure::Color("lightgreen"));
        rect->setFilled(true);
        rect->setCornerRx(5);
        rect->setCornerRy(5);
        rect->setLineWidth(2);
        rect->setLineColor(cFigure::Color("darkgreen"));

        cTextFigure *text = new cTextFigure("label");
        text->setText("Generate Task (All)");
        text->setPosition(cFigure::Point(125, 70));
        text->setAnchor(cFigure::ANCHOR_CENTER);

        btnGroup->addFigure(rect);
        btnGroup->addFigure(text);
        btnGroup->setAssociatedObject(this);  // Associate with this control module
        canvas->addFigure(btnGroup);
        
        EV_WARN << "TaskGenerationControl: Global button created on network canvas\n";
    } else {
        EV_WARN << "TaskGenerationControl: No canvas available; button not displayed\n";
    }

    // Allow command-line / ini smoke tests by triggering shortly after startup
    // when all vehicle submodules are already initialized.
    if (!par("generateTask").stdstringValue().empty()) {
        scheduleAt(simTime() + SimTime(0.001), new cMessage("deferredStartupGenerateTask"));
    }
}

void TaskGenerationControl::handleMessage(cMessage *msg) {
    if (!msg) return;
    if (strcmp(msg->getName(), "deferredStartupGenerateTask") == 0) {
        handleParameterChange("generateTask");
        delete msg;
        return;
    }

    if (strcmp(msg->getName(), "retryGenerateTask") == 0) {
        if (pendingVehicleId >= 0 && !pendingTaskType.empty()) {
            generateTaskForVehicle(pendingVehicleId, pendingTaskType);
        }
        delete msg;
        return;
    }
    delete msg;
}

void TaskGenerationControl::handleParameterChange(const char *parname) {
    if (parname == nullptr) return;
    
    // Handle quick task generation: format "vehicleId:TaskType" (e.g., "4:Perception")
    if (strcmp(parname, "generateTask") == 0) {
        std::string taskSpec = par("generateTask").stdstringValue();
        if (!taskSpec.empty()) {
            // Parse "vehicleId:TaskType"
            size_t colonPos = taskSpec.find(':');
            if (colonPos != std::string::npos) {
                try {
                    int vehicleId = std::stoi(taskSpec.substr(0, colonPos));
                    std::string taskType = taskSpec.substr(colonPos + 1);
                    
                    EV_WARN << "TaskGenerationControl: Quick task generation: vehicle " << vehicleId 
                            << " task '" << taskType << "'\n";
                    generateTaskForVehicle(vehicleId, taskType);
                } catch (const std::exception &e) {
                    EV_WARN << "TaskGenerationControl: Invalid generateTask format: " << taskSpec 
                            << " (expected 'vehicleId:TaskType')\n";
                }
            } else {
                EV_WARN << "TaskGenerationControl: Invalid generateTask format: " << taskSpec 
                        << " (expected 'vehicleId:TaskType')\n";
            }
            // Clear the parameter for next use
            par("generateTask").setStringValue("");
        }
        return;
    }
    
    // Handle global broadcast (all vehicles)
    if (strcmp(parname, "globalManualTask") == 0) {
        std::string taskType = par("globalManualTask").stdstringValue();
        if (!taskType.empty()) {
            EV_WARN << "TaskGenerationControl: Manual task request for all vehicles: '" << taskType << "'\n";
            broadcastManualTaskToVehicles(taskType);
            // Clear the parameter for next use
            par("globalManualTask").setStringValue("");
        }
    }
    
    // Handle targeted generation (specific vehicle)
    if (strcmp(parname, "targetTaskType") == 0) {
        std::string taskType = par("targetTaskType").stdstringValue();
        int vehicleId = par("targetVehicleId").intValue();
        
        if (!taskType.empty() && vehicleId >= 0) {
            EV_WARN << "TaskGenerationControl: Targeted task request for vehicle " << vehicleId 
                    << ": '" << taskType << "'\n";
            generateTaskForVehicle(vehicleId, taskType);
            // Clear the parameter for next use
            par("targetTaskType").setStringValue("");
        }
    }
}

void TaskGenerationControl::broadcastManualTaskToVehicles(const std::string &taskType) {
    // Find all vehicle nodes in the network
    cModule *network = getSystemModule();
    int nodeCount = network->getSubmoduleVectorSize("node");
    
    EV_WARN << "TaskGenerationControl: Broadcasting task '" << taskType << "' to " << nodeCount << " vehicles\n";
    
    for (int i = 0; i < nodeCount; i++) {
        cModule *vehicleModule = network->getSubmodule("node", i);
        if (vehicleModule) {
            // Get the application layer
            cModule *appModule = vehicleModule->getSubmodule("appl");
            if (appModule) {
                // Trigger manual task generation on each vehicle by setting its parameter
                appModule->par("manualTask").setStringValue(taskType);
            }
        }
    }
}

void TaskGenerationControl::generateTaskForVehicle(int vehicleId, const std::string &taskType) {
    // Generate task for a specific vehicle
    cModule *network = getSystemModule();
    cModule *vehicleModule = network->getSubmodule("node", vehicleId);
    
    if (!vehicleModule) {
        if (pendingRetriesLeft <= 0 || pendingVehicleId != vehicleId || pendingTaskType != taskType) {
            pendingVehicleId = vehicleId;
            pendingTaskType = taskType;
            pendingRetriesLeft = kMaxVehicleLookupRetries;
        }

        pendingRetriesLeft--;
        EV_WARN << "TaskGenerationControl: Vehicle " << vehicleId << " not found, retrying (left="
                << pendingRetriesLeft << ")\n";

        if (pendingRetriesLeft > 0) {
            scheduleAt(simTime() + SimTime(0.5), new cMessage("retryGenerateTask"));
        }
        return;
    }
    
    cModule *appModule = vehicleModule->getSubmodule("appl");
    if (!appModule) {
        EV_WARN << "TaskGenerationControl: Application layer not found for vehicle " << vehicleId << "\n";
        return;
    }
    
    EV_WARN << "TaskGenerationControl: Generating task for vehicle " << vehicleId 
            << " with type '" << taskType << "'\n";

    pendingVehicleId = -1;
    pendingTaskType.clear();
    pendingRetriesLeft = 0;
    
    // Trigger manual task generation by setting the parameter
    appModule->par("manualTask").setStringValue(taskType);
    EV_WARN << "TaskGenerationControl: manualTask written on app='" << appModule->getClassName()
            << "' path='" << appModule->getFullPath() << "' value='"
            << appModule->par("manualTask").stdstringValue() << "'\n";
}

} // namespace complex_network
