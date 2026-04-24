#include <cstddef>
#include "PayloadVehicleApp.h"
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <array>

using namespace veins;

namespace complex_network {

namespace {
constexpr double kDefaultV2xLinkBandwidthMbps = 6.0;
constexpr int64_t kPacketHeaderBytes = 256;
constexpr int64_t kResultMinPayloadBytes = 512;

std::string inferVehicleTypeFromId(int vehicleIndex) {
    static const std::array<const char*, 6> kBrands = {
        "Toyota", "Tesla", "Nissan", "Mercedes", "BMW", "Audi"
    };
    if (vehicleIndex < 0) return "Toyota";
    return kBrands[static_cast<size_t>(vehicleIndex) % kBrands.size()];
}

int64_t clampPacketBytes(uint64_t payloadBytes) {
    const int64_t maxSafe = static_cast<int64_t>(std::numeric_limits<int32_t>::max() - kPacketHeaderBytes);
    const int64_t payload = static_cast<int64_t>(std::min<uint64_t>(payloadBytes, static_cast<uint64_t>(maxSafe)));
    return std::max<int64_t>(1, kPacketHeaderBytes + payload);
}

void setOffloadPacketSize(veins::TaskOffloadPacket* packet, uint64_t payloadBytes) {
    packet->setByteLength(clampPacketBytes(payloadBytes));
}

void setResultPacketSize(veins::TaskResultMessage* packet, uint64_t outputBytes) {
    const uint64_t payload = std::max<uint64_t>(outputBytes, static_cast<uint64_t>(kResultMinPayloadBytes));
    packet->setByteLength(clampPacketBytes(payload));
}

std::string buildRouteHint(veins::LAddress::L2Type ingressRsuMac, veins::LAddress::L2Type processorRsuMac) {
    std::ostringstream oss;
    oss << "ROUTE|ingress=" << ingressRsuMac << "|processor=" << processorRsuMac << "|";
    return oss.str();
}

std::string buildServiceRouteHint(veins::LAddress::L2Type serviceVehicleMac,
                                  veins::LAddress::L2Type anchorRsuMac,
                                  veins::LAddress::L2Type originVehicleMac) {
    std::ostringstream oss;
    oss << "SVROUTE|sv=" << serviceVehicleMac
        << "|anchor=" << anchorRsuMac
        << "|origin=" << originVehicleMac << "|";
    return oss.str();
}

std::string buildServiceResultRelayHint(veins::LAddress::L2Type targetRsuMac,
                                        const std::string& payload) {
    std::ostringstream oss;
    oss << "SV_RESULT_RELAY|target=" << targetRsuMac << "|" << payload;
    return oss.str();
}

std::string normalizeServiceVehicleLookupId(const std::string& id) {
    if (id.rfind("SV_", 0) == 0) {
        return id.substr(3);
    }
    if (id.rfind("VEHICLE_", 0) == 0) {
        return id.substr(8);
    }
    return id;
}

std::string canonicalServiceVehicleId(const std::string& id) {
    const std::string raw = normalizeServiceVehicleLookupId(id);
    if (raw.empty()) {
        return std::string();
    }
    return "SV_" + raw;
}

double computeQueueUtility(const Task* task, simtime_t now) {
    const double remaining = std::max((task->deadline - now).dbl(), 0.001);
    return task->qos_value / remaining;
}

double estimateQueuedTaskServiceTime(const Task* task, double default_service_time_sec,
                                      const std::map<TaskType, double>& ewma_by_type,
                                      double cpu_allocable_hz, size_t max_concurrent_tasks) {
    auto it = ewma_by_type.find(task->type);
    if (it != ewma_by_type.end() && it->second > 0.0) {
        return it->second;
    }

    const double per_task_cpu_hz = std::max(cpu_allocable_hz / std::max<size_t>(1, max_concurrent_tasks), 1.0e6);
    const double fallback = static_cast<double>(task->cpu_cycles) / per_task_cpu_hz;
    if (std::isfinite(fallback) && fallback > 0.0) {
        return fallback;
    }
    return default_service_time_sec;
}

double getServiceTaskPriorityWeight(double qosValue) {
    // Keep service-vehicle weighting aligned with RSU weighted-share policy.
    double qos = std::max(0.0, std::min(1.0, qosValue));
    if (qos >= 0.80) return 5.0; // high priority
    if (qos >= 0.50) return 3.0; // medium priority
    return 2.0;                  // low/background priority
}
}

Define_Module(PayloadVehicleApp);

void PayloadVehicleApp::initialize(int stage) {
    DemoBaseApplLayer::initialize(stage);

    if (stage == 0) {
        messageSent = false;

        // Initialize vehicle parameters (similar to VehicleApp)
        flocHz_max = par("initFlocHz").doubleValue();
        flocHz_available = flocHz_max;  // Start with full capacity
        txPower_mW = par("initTxPower_mW").doubleValue();
        
        try {
            storage_capacity_gb = par("storage_capacity_gb").doubleValue();
        } catch (...) {
            storage_capacity_gb = 128.0;
        }
        
        try {
            vehicle_type = par("vehicle_type").stdstringValue();
        } catch (...) {
            vehicle_type.clear();
        }
        if (vehicle_type.empty()) {
            vehicle_type = inferVehicleTypeFromId(getParentModule()->getIndex());
        }
        cpuLoadFactor = uniform(0.1, 0.3);  // Initial light load
        lastCpuUpdateTime = simTime().dbl();
        
        // Initialize battery (typical EV battery: 40-100 kWh → 3000-8000 mAh @ 12V equivalent)
        battery_mAh_max = par("initBattery_mAh").doubleValue();
        battery_mAh_current = battery_mAh_max * uniform(0.5, 1.0);  // Start 50-100% charged
        battery_voltage = 12.0;  // Standard automotive 12V system
        lastBatteryUpdateTime = simTime().dbl();
        task_energy_j_total = 0.0;
        task_energy_j_last = 0.0;
        
        // Initialize memory (typical vehicle ECU: 2-8 GB RAM)
        memory_MB_max = par("initMemory_MB").doubleValue();
        memory_MB_available = memory_MB_max * uniform(0.4, 0.7);  // 40-70% initially available
        memoryUsageFactor = 1.0 - (memory_MB_available / memory_MB_max);
        
        // Setup mobility hookup
        mobility = FindModule<TraCIMobility*>::findSubModule(getParentModule());
        if (mobility) {
            subscribe(mobility->mobilityStateChangedSignal, this);
            // Initialize position and speed
            pos = mobility->getPositionAt(simTime());
            speed = mobility->getSpeed();
            prev_speed = speed;
            acceleration = 0.0;
            last_speed_update = simTime();
        }

        EV_INFO << "PayloadVehicleApp: Initialized with vehicle data" << endl;

        motionChannelOnly = par("motionChannelOnly").boolValue();

        // ===== INITIALIZE TASK PROCESSING SYSTEM =====
        if (!motionChannelOnly) {
            initializeTaskSystem();
        } else {
            offloadingEnabled = false;
            serviceVehicleEnabled = false;
            EV_INFO << "PayloadVehicleApp: motionChannelOnly=true, task generation/offloading disabled" << endl;
        }

        routeProgressRedisEnabled = par("routeProgressRedisEnabled").boolValue();
        if (routeProgressRedisEnabled) {
            routeProgressRedisHost = par("routeProgressRedisHost").stdstringValue();
            routeProgressRedisPort = par("routeProgressRedisPort").intValue();
            routeProgressRedisDb = par("routeProgressRedisDb").intValue();
            routeProgressRedisClient = new RedisDigitalTwin(routeProgressRedisHost, routeProgressRedisPort, routeProgressRedisDb);
            if (!routeProgressRedisClient->isConnected()) {
                EV_WARN << "Route-progress Redis unavailable; disabling export" << endl;
                delete routeProgressRedisClient;
                routeProgressRedisClient = nullptr;
                routeProgressRedisEnabled = false;
            }
        }
        
        // Schedule first periodic vehicle status update
        // Allow configuration; default is t=0 so Redis sees vehicles immediately
        double firstStatusDelay = par("firstStatusDelayS").doubleValue();
        if (!sendPayloadEvent) {
            sendPayloadEvent = new cMessage("sendPayloadMessage");
        }
        scheduleAt(simTime() + firstStatusDelay, sendPayloadEvent);
        std::cout << "CONSOLE: PayloadVehicleApp - Scheduled first payload message at time "
              << (simTime() + firstStatusDelay) << std::endl;
        
        // Schedule periodic position monitoring for shadowing analysis
        if (!monitorPositionEvent) {
            monitorPositionEvent = new cMessage("monitorPosition");
        }
        scheduleAt(simTime() + 1, monitorPositionEvent);
    }
    else if (stage == 1) {
        // Get MAC address in stage 1
        DemoBaseApplLayerToMac1609_4Interface* macInterface =
            FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(getParentModule());

        if (macInterface) {
            myMacAddress = macInterface->getMACAddress();
            EV_INFO << "PayloadVehicleApp MAC address: " << myMacAddress << endl;
        } else {
            myMacAddress = 0;
        }
    }
}

void PayloadVehicleApp::handleSelfMsg(cMessage* msg) {
    // Handle task-related events
    if (strcmp(msg->getName(), "taskGenLocalObjDet") == 0) {
        generateTask(TaskType::LOCAL_OBJECT_DETECTION);
        scheduleNextTaskGeneration(TaskType::LOCAL_OBJECT_DETECTION, msg);
        return;
    }
    else if (strcmp(msg->getName(), "taskGenCoopPercep") == 0) {
        generateTask(TaskType::COOPERATIVE_PERCEPTION);
        scheduleNextTaskGeneration(TaskType::COOPERATIVE_PERCEPTION, msg);
        return;
    }
    else if (strcmp(msg->getName(), "taskGenRouteOpt") == 0) {
        generateTask(TaskType::ROUTE_OPTIMIZATION);
        scheduleNextTaskGeneration(TaskType::ROUTE_OPTIMIZATION, msg);
        return;
    }
    else if (strcmp(msg->getName(), "taskGenFleetForecast") == 0) {
        generateTask(TaskType::FLEET_TRAFFIC_FORECAST);
        scheduleNextTaskGeneration(TaskType::FLEET_TRAFFIC_FORECAST, msg);
        return;
    }
    else if (strcmp(msg->getName(), "taskGenVoiceCommand") == 0) {
        generateTask(TaskType::VOICE_COMMAND_PROCESSING);
        scheduleNextTaskGeneration(TaskType::VOICE_COMMAND_PROCESSING, msg);
        return;
    }
    else if (strcmp(msg->getName(), "taskGenSensorHealth") == 0) {
        generateTask(TaskType::SENSOR_HEALTH_CHECK);
        scheduleNextTaskGeneration(TaskType::SENSOR_HEALTH_CHECK, msg);
        return;
    }
    else if (strcmp(msg->getName(), "taskCompletion") == 0) {
        Task* task = (Task*)msg->getContextPointer();
        if (task) task->completion_event = nullptr;
        handleTaskCompletion(task);
        delete msg;
        return;
    }
    else if (strcmp(msg->getName(), "taskDeadline") == 0) {
        Task* task = (Task*)msg->getContextPointer();
        if (task) task->deadline_event = nullptr;
        handleTaskDeadline(task);
        delete msg;
        return;
    }
    else if (strcmp(msg->getName(), "serviceTaskCompletion") == 0) {
        Task* task = (Task*)msg->getContextPointer();
        if (task) task->completion_event = nullptr;
        handleServiceTaskCompletion(task);
        delete msg;
        return;
    }
    else if (strcmp(msg->getName(), "serviceTaskDeadline") == 0) {
        Task* task = (Task*)msg->getContextPointer();
        if (task) task->deadline_event = nullptr;
        handleServiceTaskDeadline(task);
        delete msg;
        return;
    }
    else if (strcmp(msg->getName(), "rsuDecisionTimeout") == 0) {
        std::string timedOutTaskId;
        for (const auto& kv : pendingDecisionTimeouts) {
            if (kv.second == msg) {
                timedOutTaskId = kv.first;
                break;
            }
        }
        if (timedOutTaskId.empty()) {
            EV_WARN << "Ignoring stale rsuDecisionTimeout with no matching task id" << endl;
            delete msg;
            return;
        }

        pendingDecisionTimeouts.erase(timedOutTaskId);

        auto it = pendingOffloadingDecisions.find(timedOutTaskId);
        if (it == pendingOffloadingDecisions.end()) {
            EV_WARN << "Timeout fired for task " << timedOutTaskId
                    << " but no pending offloading decision exists" << endl;
            delete msg;
            return;
        }

        PendingOffloadDecision pending = it->second;
        Task* task = it->second.task;
        pendingOffloadingDecisions.erase(it);

        if (!task) {
            EV_WARN << "RSU decision timeout for task " << timedOutTaskId
                    << " but task pointer is null" << endl;
            delete msg;
            return;
        }

        if (pending.must_offload) {
            EV_WARN << "RSU decision timeout for MUST_OFFLOAD task " << task->task_id
                    << " - no local fallback" << endl;
            task->state = FAILED;
            tasks_failed++;
            if (task->is_profile_task) {
                double latency = (simTime() - task->created_time).dbl();
                MetricsManager::getInstance().recordTaskFailed(task->type, latency);
            }
            sendTaskFailureToRSU(task, "DECISION_TIMEOUT_NO_RSU_DECISION");
            const double now_s = simTime().dbl();
            const double waited_s = std::max(0.0, now_s - task->created_time.dbl());
            const double timeout_budget_s = std::max(0.0, pending.rsu_timeout_budget_seconds);
            sendTaskOffloadingEvent(task->task_id, "OFFLOAD_TIMEOUT_FAIL",
                "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "RSU",
                "{\"timeout_stage\":\"DECISION_WAIT\","
                "\"reason_code\":\"DECISION_TIMEOUT_NO_RSU_DECISION\","
                "\"classification\":\"MUST_OFFLOAD\","
                "\"gate_reason\":\"" + pending.reason + "\","
                "\"must_offload\":true,"
                "\"timeout_budget_s\":" + std::to_string(timeout_budget_s) + ","
                "\"waited_s\":" + std::to_string(waited_s) + ","
                "\"remaining_deadline_s\":" + std::to_string(std::max(0.0, pending.remaining_deadline_seconds)) + "}");
            cleanupTaskEvents(task);
            delete task;
        } else {
            EV_WARN << "RSU decision timeout for task " << task->task_id
                    << " - local fallback allowed" << endl;
            const double now_s = simTime().dbl();
            const double waited_s = std::max(0.0, now_s - task->created_time.dbl());
            const double timeout_budget_s = std::max(0.0, pending.rsu_timeout_budget_seconds);
            if (canAcceptTask(task)) {
                sendTaskOffloadingEvent(task->task_id, "OFFLOAD_TIMEOUT_LOCAL_FALLBACK",
                    "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "SELF",
                    "{\"timeout_stage\":\"DECISION_WAIT\","
                    "\"reason_code\":\"DECISION_TIMEOUT_LOCAL_FALLBACK\","
                    "\"classification\":\"BOTH_FEASIBLE\","
                    "\"gate_reason\":\"" + pending.reason + "\","
                    "\"must_offload\":false,"
                    "\"timeout_budget_s\":" + std::to_string(timeout_budget_s) + ","
                    "\"waited_s\":" + std::to_string(waited_s) + "}");
                // Notify RSU so it writes task:{id}:result = FAILED to Redis.
                // Without this, DRL's pending{} entry for this task waits 30 s
                // for a result that never arrives (task runs locally, result goes
                // to task:{id}:local_result which DRL drains via a separate queue).
                sendTaskFailureToRSU(task, "LOCAL_FALLBACK_AFTER_TIMEOUT");
                if (canStartProcessing(task)) {
                    allocateResourcesAndStart(task);
                } else {
                    task->state = QUEUED;
                    markTaskQueued(task);
                    pending_tasks.push(task);
                }
            } else {
                task->state = REJECTED;
                tasks_rejected++;
                sendTaskFailureToRSU(task, "RSU_DECISION_TIMEOUT_LOCAL_REJECT");
                cleanupTaskEvents(task);
                delete task;
            }
        }
        delete msg;
        return;
    }
    else if (strcmp(msg->getName(), "offloadedTaskTimeout") == 0) {
        std::string timedOutTaskId;
        for (const auto& kv : offloadedTaskTimeouts) {
            if (kv.second == msg) {
                timedOutTaskId = kv.first;
                break;
            }
        }
        if (timedOutTaskId.empty()) {
            EV_WARN << "Ignoring stale offloadedTaskTimeout with no matching task id" << endl;
            delete msg;
            return;
        }

        offloadedTaskTimeouts.erase(timedOutTaskId);

        auto it = offloadedTasks.find(timedOutTaskId);
        if (it == offloadedTasks.end()) {
            EV_WARN << "Offloaded task timeout fired for " << timedOutTaskId
                    << " but task is no longer tracked" << endl;
            delete msg;
            return;
        }

        Task* task = it->second;
        offloadedTasks.erase(it);
        auto targetIt = offloadedTaskTargets.find(timedOutTaskId);
        std::string target = (targetIt != offloadedTaskTargets.end()) ? targetIt->second : "UNKNOWN";
        offloadedTaskTargets.erase(timedOutTaskId);

        if (!task) {
            EV_WARN << "Offloaded task timeout for " << timedOutTaskId
                    << " but task pointer is null" << endl;
            delete msg;
            return;
        }

            task->state = FAILED;
            tasks_failed++;

            double latency = (simTime() - task->created_time).dbl();
            if (task->is_profile_task) {
                MetricsManager::getInstance().recordTaskFailed(task->type, latency);
            }

            // Distinguish execution timeout by target type and link observability.
            std::string failReason;
            if (target == "RSU") {
                failReason = "EXEC_TIMEOUT_RSU_RESULT_MISSING";
            } else {
                Coord myPos = mobility ? mobility->getPositionAt(simTime()) : Coord(0, 0, 0);
                Coord svPos;
                const std::string svLookupId = normalizeServiceVehicleLookupId(target);
                bool svPosKnown = lookupVehiclePositionById(svLookupId, svPos);

                if (!svPosKnown) {
                    failReason = "EXEC_TIMEOUT_SV_POSITION_UNKNOWN";
                } else {
                    double svRssi = estimateV2vRssiDbm(myPos, svPos);
                    if (svRssi < serviceDirectRssiThresholdDbm) {
                        failReason = "EXEC_TIMEOUT_SV_OUT_OF_DIRECT_RANGE";
                    } else {
                        failReason = "EXEC_TIMEOUT_SV_RESULT_MISSING";
                    }
                }
            }

            sendTaskOffloadingEvent(task->task_id, "OFFLOAD_TIMEOUT_FAIL",
                "VEHICLE_" + std::to_string(getParentModule()->getIndex()), target,
                "{\"timeout_stage\":\"EXECUTION_WAIT\","
                "\"reason_code\":\"" + failReason + "\","
                "\"target\":\"" + target + "\","
                "\"timeout_budget_s\":" + std::to_string(std::max(0.0, offloadedTaskTimeout.dbl())) + ","
                "\"waited_s\":" + std::to_string(std::max(0.0, latency)) + "}");

            sendTaskFailureToRSU(task, failReason);
            cleanupTaskEvents(task);
            delete task;
        delete msg;
        return;
    }
    else if (strcmp(msg->getName(), "sendPayloadMessage") == 0) {
        EV << "PayloadVehicleApp: Sending periodic vehicle status update..." << endl;

        // Update vehicle data before sending
        updateVehicleData();

        // Send VehicleResourceStatusMessage to RSU for Digital Twin tracking
        // This includes position, speed, CPU, memory, and task statistics
        sendVehicleResourceStatus();

        // Reuse the same timer for periodic status updates.
        double heartbeatInterval = par("heartbeatIntervalS").doubleValue();
        scheduleAt(simTime() + heartbeatInterval, msg);
    } 
    else if (strcmp(msg->getName(), "monitorPosition") == 0) {
        // Monitor vehicle position and get REAL simulation shadowing data
        if (mobility) {
            Coord currentPos = mobility->getPositionAt(simTime());
            
            // Try to access the actual radio module for real signal measurements
            cModule* nicModule = getParentModule()->getSubmodule("nic");
            if (nicModule) {
                cModule* phyModule = nicModule->getSubmodule("phy80211p");
                if (phyModule) {
                    // Get actual signal parameters from the simulation
                    double currentSensitivity = -85.0; // Default
                    double thermalNoise = -110.0; // Default
                    
                    // Try to get real parameters if available
                    if (phyModule->hasPar("sensitivity")) {
                        currentSensitivity = phyModule->par("sensitivity").doubleValue();
                    }
                    if (phyModule->hasPar("thermalNoise")) {
                        thermalNoise = phyModule->par("thermalNoise").doubleValue();
                    }

                    // Shadowing diagnostics intentionally muted to avoid frequent
                    // per-vehicle console spam during normal simulation runs.
                }
            }
            
            // Reuse the same message for next monitoring instead of creating new one
            scheduleAt(simTime() + 2, msg);
        }
        else {
            // If mobility not available, delete the message
            delete msg;
        }
    } 
    else if (msg->getKind() == SEND_BEACON_EVT) {
        // Handle beacon events ourselves to avoid rescheduling issues
        // Only process if beaconing is actually enabled
        if (par("sendBeacons").boolValue()) {
            DemoSafetyMessage* bsm = new DemoSafetyMessage();
            populateWSM(bsm);
            sendDown(bsm);
            // Cancel before rescheduling to avoid "already scheduled" error
            if (msg->isScheduled()) {
                cancelEvent(msg);
            }
            scheduleAt(simTime() + par("beaconInterval").doubleValue(), msg);
        }
        // Don't delete - message is reused
    }
    else {
        // Let parent class handle other events (like WSA)
        DemoBaseApplLayer::handleSelfMsg(msg);
    }
}

void PayloadVehicleApp::onWSM(BaseFrame1609_4* wsm) {
    // Verbose shadowing traces are disabled to keep runtime logs readable.
    EV << "PayloadVehicleApp: Received WSM message!" << endl;

    // Check if this is a response from RSU
    DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
    if (dsm) {
        std::string receivedPayload = dsm->getName();
        if (!receivedPayload.empty() &&
            (receivedPayload.find("RSU_RESPONSE") != std::string::npos ||
             receivedPayload.find("RSU Response") != std::string::npos)) {
            EV << "PayloadVehicleApp: RECEIVED PAYLOAD: " << receivedPayload << endl;
        }
    }

    DemoBaseApplLayer::onWSM(wsm);
}

void PayloadVehicleApp::handleMessage(cMessage* msg) {
    EV << "PayloadVehicleApp: handleMessage() called with " << msg->getName() << endl;

    // ========================================================================
    // HANDLE OFFLOADING DECISION MESSAGES
    // ========================================================================
    veins::OffloadingDecisionMessage* decisionMsg = dynamic_cast<veins::OffloadingDecisionMessage*>(msg);
    if (decisionMsg) {
        EV_INFO << "Received OffloadingDecisionMessage" << endl;
        handleOffloadingDecisionFromRSU(decisionMsg);
        return;
    }
    
    // ========================================================================
    // HANDLE TASK RESULT MESSAGES
    // ========================================================================
    veins::TaskResultMessage* resultMsg = dynamic_cast<veins::TaskResultMessage*>(msg);
    if (resultMsg) {
        EV_INFO << "Received TaskResultMessage" << endl;
        handleTaskResult(resultMsg);
        return;
    }

    // ========================================================================
    // HANDLE OBJECT DETECTION DATA (COOPERATIVE PERCEPTION)
    // ========================================================================
    veins::ObjectDetectionDataMessage* odMsg = dynamic_cast<veins::ObjectDetectionDataMessage*>(msg);
    if (odMsg) {
        handleObjectDetectionDataMessage(odMsg);
        return;
    }
    
    // ========================================================================
    // HANDLE SERVICE TASK REQUESTS (if service vehicle enabled)
    // ========================================================================
    veins::TaskOffloadPacket* taskPacket = dynamic_cast<veins::TaskOffloadPacket*>(msg);
    if (taskPacket) {
        EV_INFO << "Received TaskOffloadPacket" << endl;
        if (serviceVehicleEnabled) {
            handleServiceTaskRequest(taskPacket);
        } else {
            EV_WARN << "Received service task request but service vehicle not enabled" << endl;
            delete taskPacket;
        }
        return;
    }
    
    // ========================================================================
    // HANDLE GENERAL WSM MESSAGES
    // ========================================================================
    BaseFrame1609_4* wsm = dynamic_cast<BaseFrame1609_4*>(msg);
    if (wsm) {
        onWSM(wsm);
        delete wsm;
        return;
    }

    DemoBaseApplLayer::handleMessage(msg);
}

LAddress::L2Type PayloadVehicleApp::findRSUMacAddress() {
    // Get the network module
    cModule* networkModule = getModuleByPath("^.^");
    if (!networkModule) {
        return 0;
    }

    // Get current vehicle position
    Coord vehiclePos = mobility ? mobility->getPositionAt(simTime()) : Coord(0, 0, 0);

    double minDistance = 999999.0;
    int closestRSUIndex = -1;
    LAddress::L2Type closestRSUMac = 0;

    // Check all RSUs (0, 1, 2) and find the closest one
    for (int i = 0; i < 3; i++) {
        cModule* rsuModule = networkModule->getSubmodule("rsu", i);
        if (rsuModule) {
            cModule* rsuMobilityModule = rsuModule->getSubmodule("mobility");
            if (rsuMobilityModule) {
                double rsuX = rsuMobilityModule->par("x").doubleValue();
                double rsuY = rsuMobilityModule->par("y").doubleValue();

                double dx = vehiclePos.x - rsuX;
                double dy = vehiclePos.y - rsuY;
                double distance = sqrt(dx * dx + dy * dy);

                if (distance < minDistance) {
                    minDistance = distance;
                    closestRSUIndex = i;

                    DemoBaseApplLayerToMac1609_4Interface* rsuMacInterface =
                        FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(rsuModule);

                    if (rsuMacInterface) {
                        closestRSUMac = rsuMacInterface->getMACAddress();
                    }
                }
            }
        }
    }

    if (closestRSUIndex >= 0 && closestRSUMac != 0) {
        return closestRSUMac;
    }

    return 0;
}

void PayloadVehicleApp::updateVehicleData() {
    // Update live kinematics snapshot from mobility
    if (mobility) {
        pos = mobility->getPositionAt(simTime());
        double current_speed = mobility->getSpeed();
        
        // Calculate acceleration
        simtime_t time_delta = simTime() - last_speed_update;
        if (time_delta > 0) {
            acceleration = (current_speed - prev_speed) / time_delta.dbl();
        } else {
            acceleration = 0.0;
        }
        
        prev_speed = speed;
        speed = current_speed;
        last_speed_update = simTime();
    }
    
    // Update CPU availability with dynamic variation model
    double currentTime = simTime().dbl();
    double timeDelta = currentTime - lastCpuUpdateTime;
    
    // Model CPU load variation (simulates background tasks, thermal throttling, etc.)
    // Load factor varies slowly using random walk with bounds
    double loadChange = uniform(-0.1, 0.1) * timeDelta;  // Small random changes
    cpuLoadFactor += loadChange;
    
    // Keep CPU load within realistic bounds (10% - 90%)
    if (cpuLoadFactor < 0.1) cpuLoadFactor = 0.1;
    if (cpuLoadFactor > 0.9) cpuLoadFactor = 0.9;
    
    // Calculate available CPU capacity based on load
    flocHz_available = flocHz_max * (1.0 - cpuLoadFactor);
    lastCpuUpdateTime = currentTime;
    
    // === BATTERY DRAIN MODEL ===
    double batteryTimeDelta = currentTime - lastBatteryUpdateTime;
    if (batteryTimeDelta > 0) {
        // Calculate power consumption (Watts)
        // CPU power: Higher load = more power (typical: 10-50W for vehicle ECU)
        double cpuPower_W = 10.0 + (cpuLoadFactor * 40.0);  // 10-50W based on load
        
        // Radio transmission power (convert mW to W)
        double radioPower_W = txPower_mW / 1000.0;
        
        // Speed-based power (motor consumption, negative for regenerative braking)
        // Simplified model: higher speed = more drain, deceleration = slight regen
        double speedPower_W = speed * 0.5;  // Rough approximation
        
        // Total power consumption
        double totalPower_W = cpuPower_W + radioPower_W + speedPower_W;
        
        // Convert to mAh drain: P(W) * t(h) / V = Ah, then * 1000 = mAh
        // mAh_drain = (Power_W * time_hours * 1000) / voltage_V
        double batteryDrain_mAh = (totalPower_W * batteryTimeDelta / 3600.0 * 1000.0) / battery_voltage;
        
        battery_mAh_current -= batteryDrain_mAh;
        
        // Prevent negative battery (minimum 1%)
        if (battery_mAh_current < battery_mAh_max * 0.01) {
            battery_mAh_current = battery_mAh_max * 0.01;
        }
        
        lastBatteryUpdateTime = currentTime;
    }
    
    // === MEMORY USAGE MODEL ===
    // Memory usage varies based on active tasks and caching
    double memoryChange = uniform(-0.05, 0.08) * timeDelta;  // Can increase or decrease
    memoryUsageFactor += memoryChange;
    
    // Keep memory usage realistic (20% - 95%)
    if (memoryUsageFactor < 0.2) memoryUsageFactor = 0.2;
    if (memoryUsageFactor > 0.95) memoryUsageFactor = 0.95;
    
    memory_MB_available = memory_MB_max * (1.0 - memoryUsageFactor);
    
    // Calculate battery percentage
    double battery_pct = (battery_mAh_current / battery_mAh_max) * 100.0;
    
}

std::string PayloadVehicleApp::createVehicleDataPayload() {
    // Create structured payload with actual vehicle data (similar to recordHeartbeatScalars format)
    std::ostringstream payload;
    
    // Calculate derived metrics
    double battery_pct = (battery_mAh_current / battery_mAh_max) * 100.0;
    double memory_avail_pct = (memory_MB_available / memory_MB_max) * 100.0;
    
    payload << "VEHICLE_DATA|"
            << "VehID:" << getParentModule()->getIndex() << "|"
            << "Time:" << std::fixed << std::setprecision(3) << simTime().dbl() << "|"
            << "CPU_Max_Hz:" << std::fixed << std::setprecision(2) << flocHz_max << "|"
            << "CPU_Avail_Hz:" << std::fixed << std::setprecision(2) << flocHz_available << "|"
            << "CPU_Load_Pct:" << std::fixed << std::setprecision(2) << (cpuLoadFactor * 100) << "|"
            << "Battery_Pct:" << std::fixed << std::setprecision(2) << battery_pct << "|"
            << "Battery_mAh:" << std::fixed << std::setprecision(2) << battery_mAh_current << "|"
            << "Battery_Max_mAh:" << std::fixed << std::setprecision(2) << battery_mAh_max << "|"
            << "Memory_Avail_MB:" << std::fixed << std::setprecision(2) << memory_MB_available << "|"
            << "Memory_Avail_Pct:" << std::fixed << std::setprecision(2) << memory_avail_pct << "|"
            << "Memory_Max_MB:" << std::fixed << std::setprecision(2) << memory_MB_max << "|"
            << "TxPower_mW:" << std::fixed << std::setprecision(2) << txPower_mW << "|"
            << "Speed:" << std::fixed << std::setprecision(2) << speed << "|"
            << "PosX:" << std::fixed << std::setprecision(2) << pos.x << "|"
            << "PosY:" << std::fixed << std::setprecision(2) << pos.y << "|"
            << "MAC:" << myMacAddress;
    
    return payload.str();
}

void PayloadVehicleApp::applyTaskEnergyDrain(double energy_joules, const std::string& source) {
    if (energy_joules <= 0.0 || battery_voltage <= 0.0) {
        return;
    }

    // Convert energy (J) to battery drain (mAh): mAh = E * 1000 / (V * 3600)
    double drain_mAh = (energy_joules * 1000.0) / (battery_voltage * 3600.0);
    battery_mAh_current -= drain_mAh;

    task_energy_j_last = energy_joules;
    task_energy_j_total += energy_joules;

    // Keep at least 1% reserve to avoid negative battery artifacts.
    if (battery_mAh_current < battery_mAh_max * 0.01) {
        battery_mAh_current = battery_mAh_max * 0.01;
    }

    EV_DEBUG << "Battery drain (" << source << "): "
             << energy_joules << " J (" << drain_mAh << " mAh)" << endl;
}

void PayloadVehicleApp::receiveSignal(cComponent* src, simsignal_t id, cObject* obj, cObject* details) {
    // Handle mobility state changes
    if (mobility && id == mobility->mobilityStateChangedSignal) {
        // Update position and speed when mobility changes
        pos = mobility->getPositionAt(simTime());
        double current_speed = mobility->getSpeed();
        
        // Calculate acceleration
        simtime_t time_delta = simTime() - last_speed_update;
        if (time_delta > 0) {
            acceleration = (current_speed - prev_speed) / time_delta.dbl();
        } else {
            acceleration = 0.0;
        }
        
        prev_speed = speed;
        speed = current_speed;
        last_speed_update = simTime();
    }
    
    DemoBaseApplLayer::receiveSignal(src, id, obj, details);
}

// ============================================================================
// TASK PROCESSING SYSTEM IMPLEMENTATION
// ============================================================================

void PayloadVehicleApp::initializeTaskSystem() {
    if (motionChannelOnly) {
        offloadingEnabled = false;
        serviceVehicleEnabled = false;
        EV_INFO << "Motion/channel-only mode enabled: task generation and offloading disabled" << endl;
        std::cout << "DT_SECONDARY: Motion/channel-only mode active for vehicle "
                  << getParentModule()->getIndex() << std::endl;
        return;
    }

    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║            INITIALIZING TASK PROCESSING SYSTEM                           ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    // Read parameters from omnetpp.ini
    task_arrival_mean = par("task_arrival_mean").doubleValue();
    task_size_min = par("task_size_min").doubleValue();
    task_size_max = par("task_size_max").doubleValue();
    cpu_per_byte_min = par("cpu_per_byte_min").doubleValue();
    cpu_per_byte_max = par("cpu_per_byte_max").doubleValue();
    deadline_min = par("deadline_min").doubleValue();
    deadline_max = par("deadline_max").doubleValue();
    
    // Initialize resource capacities
    cpu_total = par("cpu_total").doubleValue();
    cpu_reservation_ratio = par("cpu_reservation_ratio").doubleValue();
    cpu_allocable = cpu_total * (1.0 - cpu_reservation_ratio);
    cpu_available = cpu_allocable;  // Initialized before threshold-aware updates
    
    memory_total = par("memory_total").doubleValue();
    memory_available = memory_total;  // Initially all memory is available
    
    // Queue parameters
    max_queue_size = par("max_queue_size").intValue();
    max_concurrent_tasks = par("max_concurrent_tasks").intValue();
    min_cpu_guarantee = par("min_cpu_guarantee").doubleValue();
    cpuConcurrencyCompletionThreshold = par("cpuConcurrencyCompletionThreshold").doubleValue();
    cpuConcurrencyCompletionThreshold = std::max(0.0, std::min(1.0, cpuConcurrencyCompletionThreshold));
    cpu_available = calculateReportedCpuAvailable();
    
    // Multi-RSU Candidate Selection & Redirect Parameters
    max_candidate_rsus = par("maxCandidateRsus").intValue();
    max_redirect_hops = par("maxRedirectHops").intValue();
    candidateBlacklistDuration = par("candidateBlacklistDuration").doubleValue();
    candidateBlacklistThreshold = par("candidateBlacklistThreshold").intValue();
    rssiThreshold = par("rssiThreshold_dBm").doubleValue();
    
    EV_INFO << "┌──────────────────────────────────────────────────────────────────────────┐" << endl;
    EV_INFO << "│ TASK GENERATION PARAMETERS                                               │" << endl;
    EV_INFO << "├──────────────────────────────────────────────────────────────────────────┤" << endl;
    EV_INFO << "│ Mean Arrival Time:    " << std::left << std::setw(48) << task_arrival_mean << " sec │" << endl;
    EV_INFO << "│ Task Size Range:      " << std::setw(36) << (task_size_min/1024) << " - " 
            << std::setw(8) << (task_size_max/1024) << " KB  │" << endl;
    EV_INFO << "│ CPU/Byte Range:       " << std::setw(36) << cpu_per_byte_min << " - " 
            << std::setw(8) << cpu_per_byte_max << " cyc│" << endl;
    EV_INFO << "│ Deadline Range:       " << std::setw(36) << deadline_min << " - " 
            << std::setw(8) << deadline_max << " sec│" << endl;
    EV_INFO << "└──────────────────────────────────────────────────────────────────────────┘" << endl;

    // Data sharing freshness window for cooperative perception
    objectDetectionTtlSec = TaskPeriods::LOCAL_OBJECT_DETECTION * 4.0;
    
    EV_INFO << "┌──────────────────────────────────────────────────────────────────────────┐" << endl;
    EV_INFO << "│ RESOURCE CAPACITIES                                                      │" << endl;
    EV_INFO << "├──────────────────────────────────────────────────────────────────────────┤" << endl;
    EV_INFO << "│ CPU Total:            " << std::setw(38) << (cpu_total / 1e9) << " GHz      │" << endl;
    EV_INFO << "│ CPU Reserved (" << std::setw(3) << (int)(cpu_reservation_ratio*100) << "%):  " 
            << std::setw(38) << ((cpu_total - cpu_allocable) / 1e9) << " GHz      │" << endl;
    EV_INFO << "│ CPU Allocable:        " << std::setw(38) << (cpu_allocable / 1e9) << " GHz      │" << endl;
    EV_INFO << "│ Memory Total:         " << std::setw(38) << (memory_total / 1e6) << " MB       │" << endl;
    EV_INFO << "│ Max Queue Size:       " << std::setw(48) << max_queue_size << "      │" << endl;
    EV_INFO << "│ Max Concurrent Tasks: " << std::setw(48) << max_concurrent_tasks << "      │" << endl;
    EV_INFO << "└──────────────────────────────────────────────────────────────────────────┘" << endl;

    EV_INFO << "┌──────────────────────────────────────────────────────────────────────────┐" << endl;
    EV_INFO << "│ TASK PROFILE SUMMARY                                                      │" << endl;
    EV_INFO << "├──────────────────────────────────────────────────────────────────────────┤" << endl;
    for (TaskType type : TaskProfileDatabase::getAllTaskTypes()) {
        const auto& profile = TaskProfileDatabase::getInstance().getProfile(type);
        EV_INFO << "│ " << std::left << std::setw(24) << TaskProfileDatabase::getTaskTypeName(type)
                << " In=" << std::setw(8) << (profile.computation.input_size_bytes / 1024.0)
                << "KB Out=" << std::setw(8) << (profile.computation.output_size_bytes / 1024.0)
                << "KB CPU=" << std::setw(8) << (profile.computation.cpu_cycles / 1e9)
                << "G DL=" << std::setw(6) << profile.timing.deadline_seconds
                << "s │" << endl;
    }
    EV_INFO << "└──────────────────────────────────────────────────────────────────────────┘" << endl;

    
    // Schedule per-task generation based on TaskProfile
    if (localObjDetEvent == nullptr) {
        localObjDetEvent = new cMessage("taskGenLocalObjDet");
    }
    if (coopPercepEvent == nullptr) {
        coopPercepEvent = new cMessage("taskGenCoopPercep");
    }
    if (routeOptEvent == nullptr) {
        routeOptEvent = new cMessage("taskGenRouteOpt");
    }
    if (fleetForecastEvent == nullptr) {
        fleetForecastEvent = new cMessage("taskGenFleetForecast");
    }
    if (voiceCommandEvent == nullptr) {
        voiceCommandEvent = new cMessage("taskGenVoiceCommand");
    }
    if (sensorHealthEvent == nullptr) {
        sensorHealthEvent = new cMessage("taskGenSensorHealth");
    }

    const double startupPhase = 0.02 * static_cast<double>(getParentModule()->getIndex() % 10);
    scheduleNextTaskGeneration(TaskType::LOCAL_OBJECT_DETECTION, localObjDetEvent, startupPhase + 0.000);
    scheduleNextTaskGeneration(TaskType::COOPERATIVE_PERCEPTION, coopPercepEvent, startupPhase + 0.002);
    scheduleNextTaskGeneration(TaskType::ROUTE_OPTIMIZATION, routeOptEvent, startupPhase + 0.004);
    scheduleNextTaskGeneration(TaskType::FLEET_TRAFFIC_FORECAST, fleetForecastEvent, startupPhase + 0.006);
    scheduleNextTaskGeneration(TaskType::VOICE_COMMAND_PROCESSING, voiceCommandEvent, startupPhase + 0.008);
    scheduleNextTaskGeneration(TaskType::SENSOR_HEALTH_CHECK, sensorHealthEvent, startupPhase + 0.010);
    
    // ============================================================================
    // INITIALIZE OFFLOADING DECISION FRAMEWORK
    // ============================================================================
    
    // Check if offloading is enabled
    offloadingEnabled = par("offloadingEnabled").boolValue();
    offloadMode = par("offloadMode").stringValue();
    EV_INFO << "⚡ Offload mode: " << offloadMode << endl;
    std::cout << "OFFLOADING: mode=" << offloadMode << " for vehicle "
              << getParentModule()->getIndex() << std::endl;
    
    if (offloadingEnabled) {
        // Initialize decision maker
        HeuristicDecisionMaker* heuristic = new HeuristicDecisionMaker();
        heuristic->setGateWeights(par("gateAlpha").doubleValue(), par("gateBeta").doubleValue());
        heuristic->setGateSafetyMarginSeconds(par("gateSafetyMarginSec").doubleValue());
        heuristic->setStageThresholds(par("K1").doubleValue(), par("K2").doubleValue());
        decisionMaker = heuristic;
        EV_INFO << "✓ Task offloading decision maker initialized" << endl;
        std::cout << "OFFLOADING: Decision maker initialized for vehicle " 
                  << getParentModule()->getIndex() << std::endl;
        EV_INFO << "  Gate-B weights: alpha=" << par("gateAlpha").doubleValue()
            << ", beta=" << par("gateBeta").doubleValue() << endl;
        EV_INFO << "  Gate-B safety margin: " << par("gateSafetyMarginSec").doubleValue() << "s" << endl;
        EV_INFO << "  Stage thresholds: K1=" << par("K1").doubleValue()
            << ", K2=" << par("K2").doubleValue() << endl;
        
        // Read timeout parameters
        rsuDecisionTimeout = par("rsuDecisionTimeout").doubleValue();
        offloadedTaskTimeout = par("offloadedTaskTimeout").doubleValue();
        EV_INFO << "  RSU decision timeout: " << rsuDecisionTimeout << "s" << endl;
        EV_INFO << "  Offloaded task timeout: " << offloadedTaskTimeout << "s" << endl;
    } else {
        EV_INFO << "⚠ Offloading DISABLED - tasks will execute locally only" << endl;
    }
    
    // Service vehicle parameters
    serviceVehicleEnabled = par("serviceVehicleEnabled").boolValue();
    if (serviceVehicleEnabled) {
        maxConcurrentServiceTasks = par("maxConcurrentServiceTasks").intValue();
        serviceCpuReservation = par("serviceCpuReservation").doubleValue();
        serviceMemoryReservation = par("serviceMemoryReservation").doubleValue();
        try {
            serviceDirectRssiThresholdDbm = par("serviceDirectRssiThresholdDbm").doubleValue();
        } catch (...) {
            serviceDirectRssiThresholdDbm = -85.0;  // IEEE 802.11p typical: -85 to -90 dBm
        }
        EV_INFO << "✓ Service vehicle mode ENABLED (can process tasks for others)" << endl;
        std::cout << "SERVICE_VEHICLE: Vehicle " << getParentModule()->getIndex() 
                  << " can accept " << maxConcurrentServiceTasks << " service tasks" << std::endl;
    }
    
    EV_INFO << "✅ Task offloading system fully initialized" << endl;
    
    EV_INFO << "✓ Task processing system initialized successfully" << endl;
    std::cout << "TASK_SYSTEM: Initialized for Vehicle " << getParentModule()->getIndex() 
              << " with " << (cpu_allocable/1e9) << " GHz allocable CPU" << std::endl;
}

void PayloadVehicleApp::scheduleNextTaskGeneration(TaskType type, cMessage* eventMsg, double extra_offset) {
    const auto& profile = TaskProfileDatabase::getInstance().getProfile(type);
    double next_arrival = 0.0;

    if (profile.generation.pattern == GenerationPattern::PERIODIC) {
        double jitter = profile.generation.period_seconds * profile.generation.period_jitter_ratio;
        next_arrival = profile.generation.period_seconds + uniform(-jitter, jitter);
    } else if (profile.generation.pattern == GenerationPattern::POISSON) {
        if (profile.generation.lambda > 0.0) {
            next_arrival = exponential(1.0 / profile.generation.lambda);
        } else {
            next_arrival = 1.0; // fallback
        }
    } else if (profile.generation.pattern == GenerationPattern::BATCH) {
        next_arrival = profile.generation.batch_interval_seconds;
    }

    if (next_arrival < 0.001) {
        next_arrival = 0.001;
    }

    // extra_offset is used only on the first call (startup delay) so that
    // tasks begin after at least two DT heartbeat cycles, giving the RSU
    // (and any DRL/heuristic decision-maker) valid vehicle state to work with.
    double fire_at = simTime().dbl() + next_arrival + extra_offset;
    scheduleAt(fire_at, eventMsg);

    if (extra_offset > 0.0) {
        EV_INFO << "⏰ First " << TaskProfileDatabase::getTaskTypeName(type)
                << " task scheduled at t=" << fire_at
                << " (startup_delay=" << extra_offset
                << "s + interval=" << next_arrival << "s)" << endl;
    } else {
        EV_INFO << "⏰ Next " << TaskProfileDatabase::getTaskTypeName(type)
                << " task scheduled in " << next_arrival << " seconds (at "
                << fire_at << ")" << endl;
    }
}

void PayloadVehicleApp::generateTask(TaskType type) {
    EV_INFO << "\n" << endl;
    EV_INFO << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
    EV_INFO << "                         GENERATING NEW TASK                                " << endl;
    EV_INFO << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;

    // Create task from TaskProfile (nominal values)
    std::string vehicle_id = std::to_string(getParentModule()->getIndex());
    Task* task = Task::createFromProfile(type, vehicle_id, task_sequence_number++);

    // -------------------------------------------------------------------------
    // Per-instance variation: sample each attribute uniformly from the profile's
    // [min, max] range.  This ensures no two tasks of the same type carry
    // identical metadata, which would be unrealistic in a real IoV network.
    // -------------------------------------------------------------------------
    const auto& profile = TaskProfileDatabase::getInstance().getProfile(type);

    uint64_t sampled_cycles = static_cast<uint64_t>(
        uniform(static_cast<double>(profile.computation.cpu_cycles),
                static_cast<double>(profile.computation.cpu_cycles_max)));
    uint64_t sampled_input = static_cast<uint64_t>(
        uniform(static_cast<double>(profile.computation.input_size_bytes),
                static_cast<double>(profile.computation.input_size_bytes_max)));
    double sampled_deadline = uniform(profile.timing.deadline_seconds,
                                      profile.timing.deadline_seconds_max);

    task->cpu_cycles       = sampled_cycles;
    task->input_size_bytes = sampled_input;
    task->mem_footprint_bytes = sampled_input;   // working-set memory = input size
    task->relative_deadline = sampled_deadline;
    task->deadline = task->created_time + SimTime(sampled_deadline);

    EV_INFO << "  [Sampled] cycles=" << (sampled_cycles/1e9) << "G"
            << " input=" << (sampled_input/1024.0) << "KB"
            << " deadline=" << sampled_deadline << "s" << endl;

    // For COOPERATIVE_PERCEPTION: scale input further by available neighbour data
    if (type == TaskType::COOPERATIVE_PERCEPTION) {
        const auto& localProfile = TaskProfileDatabase::getInstance().getProfile(TaskType::LOCAL_OBJECT_DETECTION);
        uint64_t local_output_size = localProfile.computation.output_size_bytes;
        bool local_fresh = (simTime() - localObjectDetection.timestamp).dbl() <= objectDetectionTtlSec;
        uint32_t neighbor_count = countFreshNeighborDetections();

        uint64_t input_size = (local_fresh ? local_output_size : 0) +
                              (static_cast<uint64_t>(neighbor_count) * local_output_size);
        if (input_size == 0) {
            input_size = local_output_size;
        }

        uint64_t base_input = task->input_size_bytes;
        if (input_size < base_input) {
            input_size = base_input;
        }

        double scale = static_cast<double>(input_size) / static_cast<double>(base_input);
        if (scale < 0.5) scale = 0.5;
        if (scale > 2.0) scale = 2.0;

        task->input_size_bytes = input_size;
        task->mem_footprint_bytes = input_size;
        task->cpu_cycles = static_cast<uint64_t>(task->cpu_cycles * scale);

        EV_INFO << "COOP_PERCEP input size set to " << (input_size / 1024.0) << " KB"
                << " (neighbors=" << neighbor_count << ", local_fresh="
                << (local_fresh ? "YES" : "NO") << ")" << endl;
    }

    tasks_generated++;
    {
        std::string _vid = "VEHICLE_" + std::to_string(getParentModule()->getIndex());
        sendTaskOffloadingEvent(task->task_id, "TASK_CREATED", _vid, _vid,
            std::string("{\"task_type\":\"") + TaskProfileDatabase::getTaskTypeName(type) + "\","
            "\"cpu_cycles\":" + std::to_string(task->cpu_cycles) + ","
            "\"mem_bytes\":" + std::to_string(task->mem_footprint_bytes) + ","
            "\"deadline_s\":" + std::to_string(task->relative_deadline) + ","
            "\"offloadable\":" + (task->is_offloadable ? "true" : "false") + "}");
    }

    EV_INFO << "Task generated: ID=" << task->task_id
            << " Type=" << TaskProfileDatabase::getTaskTypeName(type)
            << " Input=" << (task->input_size_bytes/1024.0) << "KB"
            << " Cycles=" << (task->cpu_cycles/1e9) << "G"
            << " QoS=" << task->qos_value
            << " Deadline=" << task->relative_deadline << "s" << endl;
    
    // ============================================================================
    // OFFLOADING DECISION INTEGRATION
    // ============================================================================
    
    // ── allLocal mode: all tasks execute locally, bypass RSU entirely ──────────
    if (offloadMode == "allLocal") {
        sendTaskOffloadingEvent(task->task_id, "DECISION_LOCAL",
            "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "SELF",
            "{\"reason\":\"ALL_LOCAL_MODE\"}");
        if (!canAcceptTask(task)) {
            EV_INFO << "❌ Task REJECTED - queue full or infeasible" << endl;
            task->state = REJECTED;
            task->failure_reason = "LOCAL_QUEUE_FULL";
            tasks_rejected++;
            sendTaskFailureToRSU(task, "REJECTED");
            cleanupTaskEvents(task);
            delete task;
            logTaskStatistics();
            return;
        }
        if (canStartProcessing(task)) {
            EV_INFO << "✓ allLocal: starting immediately" << endl;
            allocateResourcesAndStart(task);
        } else {
            EV_INFO << "⏸ allLocal: resources busy, queuing" << endl;
            task->state = QUEUED;
            markTaskQueued(task);
            pending_tasks.push(task);
        }
        logResourceState("After task generation (allLocal)");
        return;
    }

    if (!task->is_offloadable) {
        // Task is marked as local-only (e.g. LOCAL_OBJECT_DETECTION).
        // Do NOT send metadata over the air — saves wireless bandwidth.
        EV_INFO << "💻 Task is_offloadable=false → forcing local execution" << endl;
        std::cout << "LOCAL_ONLY: Task " << task->task_id
                  << " is non-offloadable, processing locally" << std::endl;
        sendTaskOffloadingEvent(task->task_id, "DECISION_LOCAL",
            "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "SELF",
            "{\"reason\":\"NON_OFFLOADABLE\"}");
    }

    // ── allOffload mode: offloadable tasks skip Gate-A/B, always go to RSU ──
    if (offloadMode == "allOffload" && task->is_offloadable) {
        sendTaskMetadataToRSU(task);
        task->state = METADATA_SENT;
        sendTaskOffloadingEvent(task->task_id, "METADATA_SENT",
            "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "RSU",
            "{\"reason\":\"ALL_OFFLOAD_MODE\"}");
        GateBDecisionResult gateResult;
        gateResult.decision = OffloadingDecision::OFFLOAD_TO_RSU;
        gateResult.classification = GateBClassification::MUST_OFFLOAD;
        gateResult.reason = "ALL_OFFLOAD_MODE";
        gateResult.t_local_seconds = 0.0;
        sendOffloadingRequestToRSU(task, OffloadingDecision::OFFLOAD_TO_RSU, gateResult);
        cMessage* toMsg = new cMessage("rsuDecisionTimeout");
        toMsg->setContextPointer(task);
        scheduleAt(simTime() + rsuDecisionTimeout.dbl(), toMsg);
        pendingDecisionTimeouts[task->task_id] = toMsg;
        logResourceState("After task generation (allOffload)");
        return;
    }

    if (offloadingEnabled && decisionMaker && task->is_offloadable) {
        // Step 6: metadata transmission is deferred until offload is actually committed.
        EV_INFO << "🤖 Offloading enabled - building decision context..." << endl;
        
        // Build decision context for the decision maker
        DecisionContext context;
        
        // Task characteristics
        context.task_size_kb = task->mem_footprint_bytes / 1024.0;
        context.output_size_kb = task->output_size_bytes / 1024.0;
        context.cpu_cycles_required = task->cpu_cycles;
        context.qos_value = task->qos_value;
        context.deadline_seconds = task->relative_deadline;
        context.must_local_tag = task->must_local_tag;
        context.must_offload_tag = task->must_offload_tag;
        context.gpu_required_tag = task->gpu_required_tag;
        context.cooperation_required_tag = task->cooperation_required_tag;
        
        // Local vehicle resources
        context.local_cpu_available = calculateReportedCpuAvailable() / 1e9;  // Convert Hz to GHz
        context.local_cpu_utilization = calculateTaskCpuUtilization();
        context.local_mem_available = memory_available / 1e6;  // Convert bytes to MB
        context.local_queue_length = pending_tasks.size();
        context.local_processing_count = processing_tasks.size();
        context.local_queue_wait_seconds = estimateLocalQueueWait(task);
        
        // RSU availability and channel conditions
        LAddress::L2Type rsu = selectBestRSU();
        context.rsu_available = (rsu != 0);
        context.rsu_distance = getRSUDistance();
        context.estimated_rsu_rssi = getEstimatedRSSI();
        context.estimated_transmission_time = estimateTransmissionTime(task);
        context.estimated_downlink_bandwidth_mbps = kDefaultV2xLinkBandwidthMbps;
        context.rsu_cpu_available_ghz = EnergyConstants::FREQ_RSU_NOMINAL / 1e9; // fallback when RSU load metrics are unavailable
        
        // RSU edge-compute load — look up the selected RSU in our metrics map
        for (auto& kv : rsuMetrics) {
            if (kv.second.macAddress == rsu) {
                context.rsu_processing_count = kv.second.rsu_processing_count;
                context.rsu_max_concurrent   = kv.second.rsu_max_concurrent;
                context.rsu_cpu_available_ghz = kv.second.rsu_cpu_available_ghz;
                break;
            }
        }
        
        // Current time for deadline calculations
        context.current_time = simTime().dbl();
        
        EV_INFO << "Decision context: LocalCPU=" << context.local_cpu_available << "GHz, "
                << "Utilization=" << (context.local_cpu_utilization*100) << "%, "
                << "Queue=" << context.local_queue_length << ", "
            << "QueueWait=" << context.local_queue_wait_seconds << "s, "
                << "RSU_CPU=" << context.rsu_cpu_available_ghz << "GHz, "
                << "RSU_dist=" << context.rsu_distance << "m, "
                << "RSSI=" << context.estimated_rsu_rssi << "dBm" << endl;
        
        // Gate A: preserve RSSI reliability semantics but fold them into unified
        // Gate B result handling instead of separate OFFLOAD_SKIP branches.
        const double RSU_RELIABLE_RSSI_THRESH = -65.0;  // dBm
        const bool gateA_rssi_ok = (context.estimated_rsu_rssi >= RSU_RELIABLE_RSSI_THRESH);
        if (!gateA_rssi_ok) {
            context.rsu_available = false;
            EV_INFO << "Gate A: RSSI weak (" << context.estimated_rsu_rssi
                    << " dBm < " << RSU_RELIABLE_RSSI_THRESH
                    << " dBm) - disabling offload feasibility" << endl;
        }

        // Make local offloading decision from detailed Gate-B result
        GateBDecisionResult gateResult = decisionMaker->makeDecisionDetailed(context);
        OffloadingDecision localDecision = gateResult.decision;

        // Keep Gate A as a hard reliability guard for this phase.
        if (!gateA_rssi_ok &&
            (localDecision == OffloadingDecision::OFFLOAD_TO_RSU ||
             localDecision == OffloadingDecision::OFFLOAD_TO_SERVICE_VEHICLE)) {
            localDecision = OffloadingDecision::EXECUTE_LOCALLY;
            gateResult.classification = GateBClassification::MUST_LOCAL;
            gateResult.reason = "GATE_A_RSSI_WEAK";
        }
        
        std::string decisionStr;
        switch(localDecision) {
            case OffloadingDecision::EXECUTE_LOCALLY:
                decisionStr = "LOCAL";
                break;
            case OffloadingDecision::OFFLOAD_TO_RSU:
                decisionStr = "OFFLOAD_TO_RSU";
                break;
            case OffloadingDecision::OFFLOAD_TO_SERVICE_VEHICLE:
                decisionStr = "OFFLOAD_TO_SERVICE_VEHICLE";
                break;
            case OffloadingDecision::REJECT_TASK:
                decisionStr = "REJECT";
                break;
        }
        
        EV_INFO << "📊 Local decision: " << decisionStr
            << " reason=" << gateResult.reason << endl;
        std::cout << "OFFLOAD_DECISION: Vehicle " << vehicle_id << " local decision for task " 
              << task->task_id << ": " << decisionStr
              << " (" << gateResult.reason << ")" << std::endl;

        if (localDecision == OffloadingDecision::EXECUTE_LOCALLY) {
            sendTaskOffloadingEvent(task->task_id, "DECISION_LOCAL",
                "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "SELF",
                "{\"decision\":\"LOCAL\",\"reason\":\"" + gateResult.reason + "\"}");
        } else if (localDecision == OffloadingDecision::OFFLOAD_TO_RSU ||
                   localDecision == OffloadingDecision::OFFLOAD_TO_SERVICE_VEHICLE) {
            sendTaskOffloadingEvent(task->task_id, "DECISION_OFFLOAD",
                "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "RSU",
                "{\"decision\":\"" + decisionStr + "\",\"reason\":\"" + gateResult.reason + "\"}");
        }
        
        // ========================================================================
        // DECISION BRANCH: LOCAL vs OFFLOAD
        // ========================================================================
        
        if (localDecision == OffloadingDecision::EXECUTE_LOCALLY) {
            // LOCAL DECISION: Process immediately without consulting RSU
            // Vehicle is confident it can handle this task
            EV_INFO << "💻 Local decision is LOCAL - processing immediately (no RSU consultation)" << endl;
            std::cout << "OFFLOAD_LOCAL: Task " << task->task_id << " processing locally (confident)" << std::endl;
            
            // Track timing for completion report (even though no RSU decision needed)
            TaskTimingInfo timing;
            timing.request_time = simTime().dbl();
            timing.decision_time = simTime().dbl();  // Immediate local decision
            timing.decision_type = "LOCAL";
            timing.processor_id = "VEHICLE_" + std::to_string(getParentModule()->getIndex());
            taskTimings[task->task_id] = timing;
            
            // Check if can accept task
            if (!canAcceptTask(task)) {
                EV_INFO << "❌ Task REJECTED - queue full or infeasible" << endl;
                task->state = REJECTED;
                tasks_rejected++;
                sendTaskFailureToRSU(task, "LOCAL_REJECTED");
                cleanupTaskEvents(task);
                delete task;
                logTaskStatistics();
                return;
            }
            
            // Process locally (existing flow)
            if (canStartProcessing(task)) {
                EV_INFO << "✓ Resources available - starting immediately" << endl;
                allocateResourcesAndStart(task);
            } else {
                EV_INFO << "⏸ Resources busy - queuing task" << endl;
                task->state = QUEUED;
                markTaskQueued(task);
                pending_tasks.push(task);
                sendTaskOffloadingEvent(task->task_id, "QUEUED",
                    "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "SELF",
                    "{\"reason\":\"LOCAL_RESOURCES_BUSY\"}");
                logQueueState("Task queued after LOCAL decision");
            }
            
            logResourceState("After task generation (local processing)");
            return;
        }
        
        // OFFLOAD DECISION: Consult RSU for final decision
        // Local Gate-B result suggests offloading - get RSU placement decision
        EV_INFO << "🌐 Local decision is OFFLOAD - consulting RSU for optimal placement" << endl;

        std::cout << "OFFLOAD_REQUEST: Task " << task->task_id << " requesting RSU decision" << std::endl;

        // Commit point reached: this task will be offloaded, so publish metadata now.
        sendTaskMetadataToRSU(task);
        task->state = METADATA_SENT;
        sendTaskOffloadingEvent(task->task_id, "METADATA_SENT",
            "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "RSU",
            "{\"mem_bytes\":" + std::to_string(task->mem_footprint_bytes) + ","
            "\"cpu_cycles\":" + std::to_string(task->cpu_cycles) + "}");
        
        // Send offloading request to RSU (includes local recommendation)
        sendOffloadingRequestToRSU(task, localDecision, gateResult);
        
        // Mark task as awaiting RSU ML decision
        EV_INFO << "⏳ Task awaiting RSU ML decision..." << endl;
        // Task is already in pendingOffloadingDecisions map from sendOffloadingRequestToRSU()
        
        // Schedule a timeout for the decision
        cMessage* timeoutMsg = new cMessage("rsuDecisionTimeout");
        timeoutMsg->setContextPointer(task);
        double timeoutBudgetSec = rsuDecisionTimeout.dbl();
        auto pendingIt = pendingOffloadingDecisions.find(task->task_id);
        if (pendingIt != pendingOffloadingDecisions.end()) {
            timeoutBudgetSec = std::max(0.001, pendingIt->second.rsu_timeout_budget_seconds);
        }
        scheduleAt(simTime() + timeoutBudgetSec, timeoutMsg);
        
        // Store timeout message so we can cancel it if decision arrives
        pendingDecisionTimeouts[task->task_id] = timeoutMsg;
        
        logResourceState("After task generation (awaiting offload decision)");
        return;  // Don't process yet - wait for RSU decision
    }
    
    // ============================================================================
    // FALLBACK: ORIGINAL LOCAL-ONLY PROCESSING (if offloading disabled)
    // ============================================================================

    if (task->is_offloadable || !offloadingEnabled) {
        sendTaskOffloadingEvent(task->task_id, "DECISION_LOCAL",
            "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "SELF",
            "{\"reason\":\"OFFLOADING_DISABLED\"}");
    }
    
    // Check if can accept task
    if (!canAcceptTask(task)) {
        EV_INFO << "❌ Task REJECTED - queue full or infeasible" << endl;
        task->state = REJECTED;
            task->failure_reason = "LOCAL_QUEUE_FULL";
        tasks_rejected++;
        sendTaskFailureToRSU(task, "REJECTED");
        cleanupTaskEvents(task);
        delete task;
        logTaskStatistics();
        return;
    }

    EV_INFO << "✓ Task accepted" << endl;
    
    // Check if can start immediately
    if (canStartProcessing(task)) {
        EV_INFO << "✓ Resources available - starting immediately" << endl;
        allocateResourcesAndStart(task);
    } else {
        EV_INFO << "⏸ Resources busy - queuing task" << endl;
        task->state = QUEUED;
            markTaskQueued(task);
        pending_tasks.push(task);
        sendTaskOffloadingEvent(task->task_id, "QUEUED",
            "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "SELF",
            "{\"reason\":\"LOCAL_FALLBACK_RESOURCES_BUSY\"}");
        logQueueState("Task queued");
    }
    
    logResourceState("After task generation");
}

void PayloadVehicleApp::finish() {
    // -------------------------------------------------------------------------
    // Message cleanup strategy: do NOT call cancelEvent/cancelAndDelete here.
    // When deleteManagedModule() calls callFinish() then deleteModule(), OMNeT++
    // removes all pending messages from the FES in deleteModule() without
    // delivering them. Calling cancelEvent() in finish() on messages whose
    // internal owner state may have been disturbed by prior signal emissions
    // (traciModuleRemovedSignal etc.) crashes in cSoftOwner::yieldOwnership.
    //
    // Safe approach: null all cMessage* pointers we own (so nothing else tries
    // to use them), delete our own heap objects (Tasks), then let deleteModule()
    // handle the actual FES/message cleanup.
    // -------------------------------------------------------------------------

    // Cancel the heartbeat explicitly — unlike other timers this one is created
    // fresh with new cMessage() on every fire (not from a single pre-allocated
    // cMessage).  When SUMO removes a vehicle mid-simulation, Veins may not call
    // the standard deleteModule() FES cleanup, leaving the timer in the FES and
    // causing a SIGSEGV when it fires against the freed module vtable.
    // cancelAndDelete() is safe here because no Veins signal has touched the
    // ownership of this plain cMessage timer before finish() is called.
    if (heartbeatMsg) {
        cancelAndDelete(heartbeatMsg);
        heartbeatMsg = nullptr;
    }

    // Reused recurring timers must also be canceled explicitly to avoid stale
    // FES entries when a vehicle is removed by TraCI before full module teardown.
    if (sendPayloadEvent) {
        cancelAndDelete(sendPayloadEvent);
        sendPayloadEvent = nullptr;
    }
    if (monitorPositionEvent) {
        cancelAndDelete(monitorPositionEvent);
        monitorPositionEvent = nullptr;
    }

    // Null recurring generation timers (deleteModule will remove them from FES).
    localObjDetEvent  = nullptr;
    coopPercepEvent   = nullptr;
    routeOptEvent     = nullptr;
    fleetForecastEvent = nullptr;
    voiceCommandEvent = nullptr;
    sensorHealthEvent = nullptr;

    // Null pending RSU decision timeout messages (deleteModule handles FES).
    for (auto& kv : pendingDecisionTimeouts) {
        kv.second = nullptr;
    }
    pendingDecisionTimeouts.clear();

    // Null pending offloaded-task timeout messages.
    for (auto& kv : offloadedTaskTimeouts) {
        kv.second = nullptr;
    }
    offloadedTaskTimeouts.clear();

    // Collect unique Task* pointers and delete them (Tasks are our heap objects,
    // not cOwnedObjects, so OMNeT++ does not manage them).
    // Null the task's cMessage pointers first so nothing can follow them.
    std::set<Task*> tasksToDelete;
    while (!pending_tasks.empty()) {
        tasksToDelete.insert(pending_tasks.top());
        pending_tasks.pop();
    }
    for (Task* t : processing_tasks) tasksToDelete.insert(t);
    processing_tasks.clear();

    for (Task* t : completed_tasks) tasksToDelete.insert(t);
    completed_tasks.clear();
    for (Task* t : failed_tasks) tasksToDelete.insert(t);
    failed_tasks.clear();

    while (!serviceTasks.empty()) {
        tasksToDelete.insert(serviceTasks.front());
        serviceTasks.pop();
    }
    for (Task* t : processingServiceTasks) tasksToDelete.insert(t);
    processingServiceTasks.clear();

    for (auto& kv : pendingOffloadingDecisions) {
        if (kv.second.task) {
            tasksToDelete.insert(kv.second.task);
        }
    }
    pendingOffloadingDecisions.clear();
    for (auto& kv : offloadedTasks) tasksToDelete.insert(kv.second);
    offloadedTasks.clear();

    for (Task* task : tasksToDelete) {
        // Null event pointers — the cMessage objects are owned by FES and will
        // be freed by deleteModule(); do NOT cancel/delete them here.
        if (task) {
            task->completion_event = nullptr;
            task->deadline_event   = nullptr;
        }
        delete task;
    }

    // Clear non-owning/index containers.
    offloadedTaskTargets.clear();
    taskTimings.clear();
    serviceTaskOriginVehicles.clear();
    serviceTaskOriginMACs.clear();
    rsuMetrics.clear();
    taskCandidates.clear();
    task_redirect_counts.clear();

    if (decisionMaker) {
        delete decisionMaker;
        decisionMaker = nullptr;
    }

    if (routeProgressRedisClient) {
        delete routeProgressRedisClient;
        routeProgressRedisClient = nullptr;
    }

    DemoBaseApplLayer::finish();
    if (motionChannelOnly) {
        // Secondary run has task generation/offloading disabled, so skip task report output.
        return;
    }
    EV_INFO << "==================== TASK METRICS REPORT ====================" << endl;
    MetricsManager::getInstance().printReport();
}

void PayloadVehicleApp::cleanupTaskEvents(Task* task) {
    if (!task) {
        return;
    }

    if (task->completion_event) {
        cancelAndDelete(task->completion_event);
        task->completion_event = nullptr;
    }

    if (task->deadline_event) {
        cancelAndDelete(task->deadline_event);
        task->deadline_event = nullptr;
    }
}

bool PayloadVehicleApp::canAcceptTask(Task* task) {
    // Check queue capacity
    if (pending_tasks.size() >= max_queue_size) {
        // If task can start immediately, queue size is not a blocker.
        const bool can_start_now = processing_tasks.size() < max_concurrent_tasks
                                   && task->mem_footprint_bytes <= memory_available;
        if (!can_start_now) {
            const simtime_t now = simTime();
            const double incoming_utility = computeQueueUtility(task, now);

            std::priority_queue<Task*, std::vector<Task*>, TaskComparator> snapshot = pending_tasks;
            double lowest_utility = std::numeric_limits<double>::infinity();
            while (!snapshot.empty()) {
                Task* queued = snapshot.top();
                snapshot.pop();
                lowest_utility = std::min(lowest_utility, computeQueueUtility(queued, now));
            }

            if (!(incoming_utility > lowest_utility)) {
                EV_INFO << "❌ Queue full (" << pending_tasks.size() << "/" << max_queue_size
                        << "), incoming utility=" << incoming_utility
                        << " <= lowest queued utility=" << lowest_utility << endl;
                return false;
            }

            EV_INFO << "↻ Queue full but incoming task has higher utility (" << incoming_utility
                    << " > " << lowest_utility << ") - replacing lowest queued task" << endl;
            dropLowestPriorityTask();
        }
    }
    
    // Check if task is too large for vehicle memory
    if (task->mem_footprint_bytes > memory_total) {
        EV_INFO << "❌ Task too large for vehicle memory" << endl;
        return false;
    }
    
    // Optimistic feasibility check
    double avg_cpu = cpu_allocable / (processing_tasks.size() + 1);
    double estimated_time = task->cpu_cycles / avg_cpu;
    if (estimated_time > task->relative_deadline * 1.2) {
        EV_INFO << "❌ Task likely infeasible (est. " << estimated_time << "s > deadline " 
                << task->relative_deadline << "s × 1.2)" << endl;
        return false;
    }
    
    EV_INFO << "✓ Task passes acceptance checks" << endl;
    return true;
}

void PayloadVehicleApp::dropLowestPriorityTask() {
    if (pending_tasks.empty()) {
        return;
    }

    const simtime_t now = simTime();
    std::vector<Task*> queued_tasks;
    queued_tasks.reserve(pending_tasks.size());

    while (!pending_tasks.empty()) {
        queued_tasks.push_back(pending_tasks.top());
        pending_tasks.pop();
    }

    Task* lowest_task = nullptr;
    double lowest_utility = std::numeric_limits<double>::infinity();
    for (Task* t : queued_tasks) {
        const double utility = computeQueueUtility(t, now);
        if (utility < lowest_utility) {
            lowest_utility = utility;
            lowest_task = t;
        }
    }

    for (Task* t : queued_tasks) {
        if (t != lowest_task) {
            pending_tasks.push(t);
        }
    }

    if (!lowest_task) {
        return;
    }

    lowest_task->state = REJECTED;
    lowest_task->failure_reason = "REPLACED_BY_HIGHER_PRIORITY";
    tasks_rejected++;
    sendTaskOffloadingEvent(lowest_task->task_id, "DROPPED_FROM_QUEUE",
        "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "SELF",
        "{\"reason\":\"REPLACED_BY_HIGHER_PRIORITY\",\"utility\":" + std::to_string(lowest_utility) + "}");
    sendTaskFailureToRSU(lowest_task, "REPLACED_BY_HIGHER_PRIORITY");

    EV_WARN << "Dropped queued task " << lowest_task->task_id
            << " (utility=" << lowest_utility << ") to make room for higher-priority work" << endl;

    delete lowest_task;
}

bool PayloadVehicleApp::canStartProcessing(Task* task) {
    // Check concurrent task limit
    if (processing_tasks.size() >= max_concurrent_tasks) {
        EV_INFO << "  ⏸ Max concurrent tasks reached (" << processing_tasks.size() << "/" 
                << max_concurrent_tasks << ")" << endl;
        return false;
    }
    
    // Check memory availability
    if (task->mem_footprint_bytes > memory_available) {
        EV_INFO << "  ⏸ Insufficient memory (" << (memory_available/1e6) << " MB available, " 
                << (task->mem_footprint_bytes/1e6) << " MB needed)" << endl;
        return false;
    }
    
    return true;
}

void PayloadVehicleApp::allocateResourcesAndStart(Task* task) {
    EV_INFO << "┌──────────────────────────────────────────────────────────────────────────┐" << endl;
    EV_INFO << "│           ALLOCATING RESOURCES AND STARTING TASK                         │" << endl;
    EV_INFO << "└──────────────────────────────────────────────────────────────────────────┘" << endl;
    
    
    // Allocate memory
    memory_available -= task->mem_footprint_bytes;
    
    // Add to processing set
    processing_tasks.insert(task);
    task->state = PROCESSING;
    task->processing_start_time = simTime();
    
    // Track start time for latency calculation
    if (taskTimings.find(task->task_id) != taskTimings.end()) {
        taskTimings[task->task_id].start_time = simTime().dbl();
        if (taskTimings[task->task_id].predicted_service_time <= 0.0) {
            taskTimings[task->task_id].predicted_service_time =
                (task->predicted_service_time_sec > 0.0)
                    ? task->predicted_service_time_sec
                    : estimateLocalServiceTime(task);
        }
        EV_INFO << "Start time tracked for task " << task->task_id << endl;
    }
    
    // Calculate CPU allocation
    task->cpu_allocated = calculateCPUAllocation(task);
    cpu_available = calculateReportedCpuAvailable();
    
    EV_INFO << "Resource allocation:" << endl;
    EV_INFO << "  • Memory allocated: " << (task->mem_footprint_bytes / 1e6) << " MB" << endl;
    EV_INFO << "  • CPU allocated: " << (task->cpu_allocated / 1e9) << " GHz" << endl;
    EV_INFO << "  • Remaining CPU: " << (cpu_available / 1e9) << " GHz" << endl;
    EV_INFO << "  • Remaining Memory: " << (memory_available / 1e6) << " MB" << endl;
    
    // Calculate execution time
    double exec_time = task->cpu_cycles / task->cpu_allocated;
    
    EV_INFO << "Execution plan:" << endl;
    EV_INFO << "  • Estimated execution time: " << exec_time << " seconds" << endl;
    EV_INFO << "  • Deadline: " << task->relative_deadline << " seconds" << endl;
    EV_INFO << "  • Will complete at: " << (simTime() + exec_time).dbl() << endl;
    EV_INFO << "  • Deadline expires at: " << task->deadline.dbl() << endl;
    
    // Schedule completion event
    cMessage* completionMsg = new cMessage("taskCompletion");
    completionMsg->setContextPointer(task);
    task->completion_event = completionMsg;
    scheduleAt(simTime() + exec_time, completionMsg);
    sendTaskOffloadingEvent(task->task_id, "PROCESSING_STARTED",
        "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "SELF",
        "{\"cpu_ghz\":" + std::to_string(task->cpu_allocated/1e9) + ","
        "\"est_exec_s\":" + std::to_string(exec_time) + "}");

    // Schedule deadline check (guard against deadline already passed)
    cMessage* deadlineMsg = new cMessage("taskDeadline");
    deadlineMsg->setContextPointer(task);
    task->deadline_event = deadlineMsg;
    scheduleAt(task->deadline <= simTime() ? simTime() : task->deadline, deadlineMsg);
    
    task->logTaskInfo("Task started processing");
    
    std::cout << "TASK_EXEC: Vehicle " << getParentModule()->getIndex() << " started task " 
              << task->task_id << " with " << (task->cpu_allocated/1e9) << " GHz" << std::endl;
}

void PayloadVehicleApp::markTaskQueued(Task* task) {
    task->queue_entry_time = simTime();
    if (task->predicted_service_time_sec <= 0.0) {
        task->predicted_service_time_sec = estimateLocalServiceTime(task);
    }

    TaskTimingInfo& timing = taskTimings[task->task_id];
    if (timing.predicted_service_time <= 0.0) {
        timing.predicted_service_time = task->predicted_service_time_sec;
    }
}

double PayloadVehicleApp::estimateLocalServiceTime(Task* task) const {
    return estimateQueuedTaskServiceTime(task, local_service_time_default_sec,
                                         local_service_time_ewma,
                                         cpu_allocable, max_concurrent_tasks);
}

double PayloadVehicleApp::estimateLocalQueueWait(Task* task) const {
    if (pending_tasks.empty()) {
        return 0.0;
    }

    std::priority_queue<Task*, std::vector<Task*>, TaskComparator> snapshot = pending_tasks;
    double queue_wait = 0.0;

    const double task_deadline = std::max((task->deadline - task->created_time).dbl(), 1e-6);
    const double task_urgency = std::min(1.0, TaskComparator::DEADLINE_MIN_REF_SEC / task_deadline);
    const double task_qos = std::max(0.0, std::min(task->qos_value / TaskComparator::QOS_MAX, 1.0));
    const double task_score = TaskComparator::ALPHA_QOS * task_qos
                            + TaskComparator::BETA_DEADLINE * task_urgency;

    while (!snapshot.empty()) {
        Task* queued = snapshot.top();
        snapshot.pop();

        const double queued_deadline = std::max((queued->deadline - queued->created_time).dbl(), 1e-6);
        const double queued_urgency = std::min(1.0, TaskComparator::DEADLINE_MIN_REF_SEC / queued_deadline);
        const double queued_qos = std::max(0.0, std::min(queued->qos_value / TaskComparator::QOS_MAX, 1.0));
        const double queued_base_score = TaskComparator::ALPHA_QOS * queued_qos
                                       + TaskComparator::BETA_DEADLINE * queued_urgency;
        const double queued_wait = std::max(0.0, (simTime() - queued->queue_entry_time).dbl());
        const double queued_score = queued_base_score
                                  + std::min(TaskComparator::MAX_AGING_BOOST,
                                             TaskComparator::AGING_FACTOR_PER_SEC * queued_wait);

        if (queued_score > task_score ||
            (std::abs(queued_score - task_score) <= 1e-12 && queued->task_id < task->task_id)) {
            queue_wait += estimateQueuedTaskServiceTime(queued, local_service_time_default_sec,
                                                        local_service_time_ewma,
                                                        cpu_allocable, max_concurrent_tasks);
        }
    }

    return queue_wait;
}

void PayloadVehicleApp::updateLocalServiceTimeEstimate(Task* task, double actual_service_time_sec) {
    if (!(actual_service_time_sec > 0.0) || !std::isfinite(actual_service_time_sec)) {
        return;
    }

    double& ewma = local_service_time_ewma[task->type];
    if (!(ewma > 0.0) || !std::isfinite(ewma)) {
        ewma = actual_service_time_sec;
    } else {
        ewma = local_service_time_ewma_alpha * actual_service_time_sec
             + (1.0 - local_service_time_ewma_alpha) * ewma;
    }
}

double PayloadVehicleApp::calculateTaskCompletionFraction(const Task* task) const {
    if (!task || task->processing_start_time <= SimTime(0)) {
        return 0.0;
    }

    const double elapsed = std::max(0.0, (simTime() - task->processing_start_time).dbl());

    if (task->completion_event && task->completion_event->isScheduled()) {
        const double time_left = std::max(0.0,
            task->completion_event->getArrivalTime().dbl() - simTime().dbl());
        const double estimated_total = elapsed + time_left;
        if (estimated_total > 1e-9) {
            return std::max(0.0, std::min(1.0, elapsed / estimated_total));
        }
    }

    if (task->cpu_cycles > 0 && task->cpu_cycles_executed > 0) {
        return std::max(0.0, std::min(1.0,
            static_cast<double>(task->cpu_cycles_executed) / static_cast<double>(task->cpu_cycles)));
    }

    return 0.0;
}

size_t PayloadVehicleApp::countCpuAvailableConcurrency() const {
    size_t concurrent = 0;
    for (Task* task : processing_tasks) {
        if (!task || task->state != PROCESSING) {
            continue;
        }
        const double progress = calculateTaskCompletionFraction(task);
        if (progress < cpuConcurrencyCompletionThreshold) {
            concurrent++;
        }
    }
    return concurrent;
}

double PayloadVehicleApp::calculateReportedCpuAvailable() const {
    if (cpu_allocable <= 0.0) {
        return 0.0;
    }
    const size_t concurrent = countCpuAvailableConcurrency();
    return cpu_allocable / (static_cast<double>(concurrent) + 1.0);
}

double PayloadVehicleApp::calculateTaskCpuUtilization() const {
    if (cpu_allocable <= 0.0) {
        return 0.0;
    }

    double allocated_sum = 0.0;
    for (Task* task : processing_tasks) {
        if (task && task->state == PROCESSING) {
            allocated_sum += std::max(0.0, task->cpu_allocated);
        }
    }
    return std::max(0.0, std::min(1.0, allocated_sum / cpu_allocable));
}

double PayloadVehicleApp::getServiceCpuPoolHz() const {
    if (cpu_allocable <= 0.0) {
        return 0.0;
    }

    // Base reserved pool for service work.
    const double reservation = std::max(0.0, std::min(1.0, serviceCpuReservation));
    const double base_pool_hz = cpu_total * reservation;

    // Borrow part of currently idle allocable CPU (after local processing allocations).
    double local_allocated_hz = 0.0;
    for (Task* task : processing_tasks) {
        if (task && task->state == PROCESSING) {
            local_allocated_hz += std::max(0.0, task->cpu_allocated);
        }
    }

    const double idle_allocable_hz = std::max(0.0, cpu_allocable - local_allocated_hz);
    const double borrow_hz = 0.7 * idle_allocable_hz;

    // Keep headroom for own tasks and control traffic bursts.
    const double hard_cap_hz = 0.9 * cpu_allocable;
    const double adaptive_pool_hz = std::min(hard_cap_hz, base_pool_hz + borrow_hz);

    return std::max(base_pool_hz, adaptive_pool_hz);
}

double PayloadVehicleApp::calculateCPUAllocation(Task* task) {
    if (processing_tasks.empty()) {
        // First task gets full allocable CPU
        return cpu_allocable;
    }
    
    // Calculate minimum guarantee
    double F_min = std::min(cpu_allocable * min_cpu_guarantee, 
                           cpu_allocable / max_concurrent_tasks);
    
    // Calculate weights based on urgency
    double total_weight = 0.0;
    std::map<Task*, double> weights;
    
    for (Task* t : processing_tasks) {
        double remaining_cycles = t->getRemainingCycles();
        double remaining_deadline = t->getRemainingDeadline(simTime());
        double weight = std::min(remaining_cycles / std::max(remaining_deadline, 0.001), 
                                cpu_allocable);
        weights[t] = weight;
        total_weight += weight;
    }
    
    // Add current task
    double remaining_deadline = task->getRemainingDeadline(simTime());
    double task_weight = std::min((double)task->cpu_cycles / std::max(remaining_deadline, 0.001),
                                  cpu_allocable);
    weights[task] = task_weight;
    total_weight += task_weight;
    
    // Calculate proportional allocation
    size_t n_tasks = processing_tasks.size() + 1;  // +1 for current task
    double leftover_cpu = cpu_allocable - (n_tasks * F_min);
    
    if (leftover_cpu > 0 && total_weight > 0) {
        double allocated = F_min + (leftover_cpu * task_weight / total_weight);
        EV_INFO << "CPU Allocation (Proportional Fair): F_min=" << (F_min/1e9) 
                << " GHz + proportional=" << ((allocated - F_min)/1e9) 
                << " GHz = " << (allocated/1e9) << " GHz" << endl;
        return allocated;
    }
    
    EV_INFO << "CPU Allocation (Minimum Guarantee): " << (F_min/1e9) << " GHz" << endl;
    return F_min;
}

void PayloadVehicleApp::reallocateCPUResources() {
    if (processing_tasks.empty()) {
        cpu_available = calculateReportedCpuAvailable();
        return;
    }
    
    EV_INFO << "┌──────────────────────────────────────────────────────────────────────────┐" << endl;
    EV_INFO << "│               REALLOCATING CPU RESOURCES                                 │" << endl;
    EV_INFO << "├──────────────────────────────────────────────────────────────────────────┤" << endl;
    EV_INFO << "│ Processing tasks: " << std::setw(52) << processing_tasks.size() << " │" << endl;
    EV_INFO << "└──────────────────────────────────────────────────────────────────────────┘" << endl;
    
    // Recalculate allocation for all processing tasks.
    // Remaining cycles are derived from the currently scheduled completion time:
    //   remaining_cycles = cpu_allocated * (arrival_time - now)
    // This is always correct regardless of how many prior reallocations occurred
    // because cpu_allocated * remaining_time = outstanding compute work (Hz * s = cycles).
    for (Task* task : processing_tasks) {
        double old_allocation = task->cpu_allocated;
        double new_allocation = calculateCPUAllocation(task);

        if (std::abs(new_allocation - old_allocation) > 1e6) {  // Significant change (> 1 MHz)
            EV_INFO << "Task " << task->task_id << ": " << (old_allocation/1e9)
                    << " → " << (new_allocation/1e9) << " GHz" << endl;

            // Derive remaining cycles from the currently scheduled completion time
            double remaining_cycles;
            if (task->completion_event && task->completion_event->isScheduled()) {
                double time_left = std::max(0.0,
                    task->completion_event->getArrivalTime().dbl() - simTime().dbl());
                remaining_cycles = old_allocation * time_left;
            } else {
                remaining_cycles = static_cast<double>(task->cpu_cycles);  // fallback
            }

            task->cpu_allocated = new_allocation;

            if (task->completion_event && task->completion_event->isScheduled()) {
                cancelEvent(task->completion_event);
            }
            double new_exec_time = (new_allocation > 0.0)
                                   ? remaining_cycles / new_allocation : 0.0;
            scheduleAt(simTime() + new_exec_time, task->completion_event);

            EV_INFO << "  Remaining cycles: " << (remaining_cycles/1e9) << " G" << endl;
            EV_INFO << "  New completion time: " << new_exec_time << " seconds" << endl;
        }
    }
    
    cpu_available = calculateReportedCpuAvailable();
    
    EV_INFO << "CPU available after reallocation: " << (cpu_available/1e9) << " GHz" << endl;
}

void PayloadVehicleApp::processQueuedTasks() {
    EV_INFO << "┌──────────────────────────────────────────────────────────────────────────┐" << endl;
    EV_INFO << "│            PROCESSING QUEUED TASKS                                       │" << endl;
    EV_INFO << "├──────────────────────────────────────────────────────────────────────────┤" << endl;
    EV_INFO << "│ Queue size: " << std::setw(58) << pending_tasks.size() << " │" << endl;
    EV_INFO << "└──────────────────────────────────────────────────────────────────────────┘" << endl;
    
    std::vector<Task*> deferred_tasks;

    // Try-next scheduling:
    // scan the whole pending queue once, start any task that is currently feasible,
    // and defer blocked tasks back into the queue afterward.
    while (!pending_tasks.empty()) {
        Task* task = pending_tasks.top();
        pending_tasks.pop();

        // Check if task deadline already passed
        if (task->isDeadlineMissed(simTime())) {
            EV_INFO << "❌ Task " << task->task_id << " deadline expired in queue" << endl;
            task->state = FAILED;
            task->failure_reason = "DEADLINE_MISSED_IN_QUEUE";
            tasks_failed++;
            if (task->is_profile_task) {
                double latency = (simTime() - task->created_time).dbl();
                MetricsManager::getInstance().recordTaskFailed(task->type, latency);
            }
            sendTaskFailureToRSU(task, "DEADLINE_MISSED_IN_QUEUE");
            cleanupTaskEvents(task);
            delete task;
            continue;
        }

        if (canStartProcessing(task)) {
            EV_INFO << "✓ Starting queued task " << task->task_id << endl;
            allocateResourcesAndStart(task);
        } else {
            deferred_tasks.push_back(task);
        }
    }

    for (Task* task : deferred_tasks) {
        markTaskQueued(task);
        pending_tasks.push(task);
    }

    if (!deferred_tasks.empty()) {
        EV_INFO << "⏸ Deferred " << deferred_tasks.size() << " task(s) that are still blocked" << endl;
    }
    
    logQueueState("After processing queue");
}

void PayloadVehicleApp::handleTaskCompletion(Task* task) {
    EV_INFO << "\n" << endl;
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║                       TASK COMPLETED                                     ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    task->completion_time = simTime();
    double completion_time_elapsed = (task->completion_time - task->created_time).dbl();
    
    // Check if completed on time or late
    bool on_time = task->completion_time <= task->deadline;
    
    if (on_time) {
        task->state = COMPLETED_ON_TIME;
        tasks_completed_on_time++;
        EV_INFO << "✓ Task completed ON TIME" << endl;
        std::cout << "TASK_SUCCESS: Vehicle " << getParentModule()->getIndex() << " completed task " 
                  << task->task_id << " ON TIME (" << completion_time_elapsed << "s / " 
                  << task->relative_deadline << "s)" << std::endl;
    } else {
        task->state = COMPLETED_LATE;
        tasks_completed_late++;
        double lateness = (task->completion_time - task->deadline).dbl();
        EV_INFO << "⚠ Task completed LATE (by " << lateness << " seconds)" << endl;
        std::cout << "TASK_LATE: Vehicle " << getParentModule()->getIndex() << " completed task " 
                  << task->task_id << " LATE (by " << lateness << "s)" << std::endl;
    }

    if (task->is_profile_task && task->type == TaskType::LOCAL_OBJECT_DETECTION) {
        localObjectDetection.timestamp = simTime();
        localObjectDetection.size_bytes = task->output_size_bytes;
        sendObjectDetectionData(task);
    }
    
    // Release resources
    processing_tasks.erase(task);
    cpu_available = calculateReportedCpuAvailable();
    memory_available += task->mem_footprint_bytes;
    
    cleanupTaskEvents(task);
    
    // Record statistics
    total_completion_time += completion_time_elapsed;

    double exec_time = (task->completion_time - task->processing_start_time).dbl();
    if (taskTimings.find(task->task_id) != taskTimings.end()) {
        taskTimings[task->task_id].finish_time = task->completion_time.dbl();
        taskTimings[task->task_id].actual_service_time = exec_time;
    }
    updateLocalServiceTimeEstimate(task, exec_time);
    double energy_joules = energyCalculator.calcLocalExecutionEnergy(
            task->cpu_cycles,
            static_cast<uint32_t>(task->input_size_bytes),
            exec_time
    );
    applyTaskEnergyDrain(energy_joules, "LOCAL_EXECUTION");

    if (task->is_profile_task) {
        MetricsManager::getInstance().recordTaskCompletion(
            task->type,
            MetricsManager::LOCAL_EXECUTION,
            exec_time,
            completion_time_elapsed,
            energy_joules,
            task->deadline.dbl(),
            task->completion_time.dbl()
        );
    }
    
    sendTaskOffloadingEvent(task->task_id, "PROCESSING_COMPLETED",
        "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "SELF",
        "{\"success\":true,\"elapsed_s\":" + std::to_string(completion_time_elapsed) + "}");

    {
        std::string _status = on_time ? "COMPLETE_ON_TIME" : "FAILED";
        sendTaskOffloadingEvent(task->task_id, _status,
            "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "SELF",
            "{\"on_time\":" + std::string(on_time ? "true" : "false") + ","
            "\"elapsed_s\":" + std::to_string(completion_time_elapsed) + "}");
    }
    task->logTaskInfo("Task completed");

    // Send local completion notification to RSU (writes local_result to Redis)
    sendTaskCompletionToRSU(task);

    // Reallocate CPU to remaining tasks
    reallocateCPUResources();
    
    // Try to start queued tasks
    processQueuedTasks();
    
    // Keep limited history
    completed_tasks.push_back(task);
    if (completed_tasks.size() > 100) {
        delete completed_tasks.front();
        completed_tasks.pop_front();
    }
    
    logResourceState("After task completion");
    logTaskStatistics();
}

void PayloadVehicleApp::handleTaskDeadline(Task* task) {
    // Check if task is still processing or queued
    if (task->state != PROCESSING && task->state != QUEUED) {
        // Task already completed or failed
        return;
    }
    
    EV_INFO << "\n" << endl;
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║                    TASK DEADLINE EXPIRED                                 ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    task->state = FAILED;
    task->failure_reason = "DEADLINE_MISSED";
    task->completion_time = simTime();
    double wasted_time = (task->completion_time - task->created_time).dbl();
    bool was_processing = (processing_tasks.find(task) != processing_tasks.end());
    
    EV_INFO << "❌ Task FAILED - deadline missed" << endl;
    EV_INFO << "  Wasted time: " << wasted_time << " seconds" << endl;
    
    std::cout << "TASK_FAILED: Vehicle " << getParentModule()->getIndex() << " task " 
              << task->task_id << " DEADLINE MISSED (wasted " << wasted_time << "s)" << std::endl;
    
    // Release resources if was processing
    if (was_processing) {
        processing_tasks.erase(task);
        cpu_available = calculateReportedCpuAvailable();
        memory_available += task->mem_footprint_bytes;
        
        reallocateCPUResources();
    }

    cleanupTaskEvents(task);

    if (was_processing) {
        sendTaskOffloadingEvent(task->task_id, "PROCESSING_COMPLETED",
            "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "SELF",
            "{\"success\":false,\"reason\":\"DEADLINE_MISSED\"}");
    }
    sendTaskOffloadingEvent(task->task_id, "FAILED",
        "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "SELF",
        "{\"reason\":\"DEADLINE_MISSED\",\"wasted_s\":" + std::to_string(wasted_time) + "}");
    
    tasks_failed++;

    if (task->is_profile_task) {
        MetricsManager::getInstance().recordTaskFailed(
            task->type,
            wasted_time
        );
    }
    
    task->logTaskInfo("Task failed - deadline expired");
    
    // Send local completion report for failed task (writes local_result to Redis)
    sendTaskCompletionToRSU(task);

    // Send failure notification to RSU
    sendTaskFailureToRSU(task, "DEADLINE_MISSED");
    
    // Try to start queued tasks with freed resources
    processQueuedTasks();
    
    // Keep limited history
    failed_tasks.push_back(task);
    if (failed_tasks.size() > 100) {
        delete failed_tasks.front();
        failed_tasks.pop_front();
    }
    
    logResourceState("After task failure");
    logTaskStatistics();
}

void PayloadVehicleApp::sendTaskMetadataToRSU(Task* task) {
    EV_INFO << "📤 [TASK EVENT] Sending task metadata to RSU (triggered by task generation)" << endl;
    
    TaskMetadataMessage* msg = new TaskMetadataMessage();
    msg->setTask_id(task->task_id.c_str());
    msg->setVehicle_id(task->vehicle_id.c_str());
    msg->setMem_footprint_bytes(task->mem_footprint_bytes);
    msg->setCpu_cycles(task->cpu_cycles);
    msg->setCreated_time(task->created_time.dbl());
    msg->setDeadline_seconds(task->relative_deadline);
    msg->setQos_value(task->qos_value);
    
    // Populate task profile fields so RSU/Redis DT know task type
    if (task->is_profile_task) {
        msg->setIs_profile_task(true);
        msg->setTask_type_id(static_cast<int>(task->type));
        msg->setTask_type_name(TaskProfileDatabase::getInstance().getTaskTypeName(task->type).c_str());
        msg->setInput_size_bytes(task->input_size_bytes);
        msg->setOutput_size_bytes(task->output_size_bytes);
        msg->setIs_offloadable(task->is_offloadable);
        msg->setIs_safety_critical(task->is_safety_critical);
        msg->setPriority_level(static_cast<int>(task->priority));
    }
    
    // Find RSU and send
    LAddress::L2Type rsuMacAddress = selectBestRSU();
    if (rsuMacAddress != 0) {
        populateWSM((BaseFrame1609_4*)msg, rsuMacAddress);
        sendDown(msg);
        EV_INFO << "✓ Task metadata sent to RSU (MAC: " << rsuMacAddress << ")" << endl;
        std::cout << "TASK_METADATA: Vehicle " << task->vehicle_id << " sent task " 
                  << task->task_id << " metadata to RSU" << std::endl;
    } else {
        EV_INFO << "⚠ RSU not found, broadcasting metadata" << endl;
        populateWSM((BaseFrame1609_4*)msg);
        sendDown(msg);
    }
}

void PayloadVehicleApp::sendTaskCompletionToRSU(Task* task) {
    EV_INFO << "📤 Sending local task completion to RSU: " << task->task_id << endl;

    bool on_time = (task->state == COMPLETED_ON_TIME);
    bool success = (task->state == COMPLETED_ON_TIME || task->state == COMPLETED_LATE);
    // Use on_time (not success) so COMPLETED_LATE correctly reports "DEADLINE_MISSED"
    // instead of "NONE". The RSU uses this to set the Redis reason field.
    std::string fail_reason = on_time ? "NONE" : "DEADLINE_MISSED";

    TaskCompletionMessage* msg = new TaskCompletionMessage();
    msg->setTask_id(task->task_id.c_str());
    msg->setVehicle_id(task->vehicle_id.c_str());
    msg->setCompletion_time(task->completion_time.dbl());
    msg->setProcessing_time((task->completion_time - task->processing_start_time).dbl());
    msg->setCompleted_on_time(on_time);
    msg->setCpu_allocated(task->cpu_allocated);

    // Local-execution metadata so RSU can write task:{id}:local_result to Redis
    msg->setIs_local_execution(true);
    msg->setTask_type_name(TaskProfileDatabase::getTaskTypeName(task->type).c_str());
    msg->setQos_value(task->qos_value);
    msg->setDeadline_seconds(task->relative_deadline);
    msg->setFailure_reason(fail_reason.c_str());

    // Clean up any timing entry that was created for this task
    taskTimings.erase(task->task_id);

    LAddress::L2Type rsuMacAddress = selectBestRSU();
    if (rsuMacAddress != 0) {
        populateWSM((BaseFrame1609_4*)msg, rsuMacAddress);
        sendDown(msg);
    } else {
        populateWSM((BaseFrame1609_4*)msg);
        sendDown(msg);
    }
}

void PayloadVehicleApp::sendTaskFailureToRSU(Task* task, const std::string& reason) {
    EV_INFO << "📤 Sending task failure notification to RSU (reason: " << reason << ")" << endl;

    sendTaskOffloadingEvent(task->task_id, "FAILED",
        "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "SELF",
        "{\"reason\":\"" + reason + "\"}");
    
    TaskFailureMessage* msg = new TaskFailureMessage();
    msg->setTask_id(task->task_id.c_str());
    msg->setVehicle_id(task->vehicle_id.c_str());
    msg->setFailure_time(simTime().dbl());
    msg->setFailure_reason(reason.c_str());
    msg->setWasted_time((simTime() - task->created_time).dbl());
    
    LAddress::L2Type rsuMacAddress = selectBestRSU();
    if (rsuMacAddress != 0) {
        populateWSM((BaseFrame1609_4*)msg, rsuMacAddress);
        sendDown(msg);
    } else {
        populateWSM((BaseFrame1609_4*)msg);
        sendDown(msg);
    }
}

// ============================================================================
// SERVICE VEHICLE TASK COMPLETION HANDLERS
// ============================================================================

void PayloadVehicleApp::handleServiceTaskCompletion(Task* task) {
    EV_INFO << "\n" << endl;
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║              SERVICE TASK COMPLETED                                      ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    task->completion_time = simTime();
    double processing_time = (task->completion_time - task->processing_start_time).dbl();

    // Service vehicle pays compute energy for the offloaded task it executes.
    double service_energy_j = energyCalculator.calcLocalExecutionEnergy(
        task->cpu_cycles,
        static_cast<uint32_t>(task->input_size_bytes),
        processing_time
    );
    applyTaskEnergyDrain(service_energy_j, "SERVICE_VEHICLE_EXECUTION");
    
    // Determine if task met its deadline
    if (task->completion_time <= task->deadline) {
        task->state = COMPLETED_ON_TIME;
        EV_INFO << "✅ Service task COMPLETED ON TIME" << endl;
        std::cout << "SERVICE_COMPLETE_ON_TIME: Vehicle " << getParentModule()->getIndex() 
                  << " completed service task " << task->task_id 
                  << " in " << processing_time << "s" << std::endl;
    } else {
        task->state = COMPLETED_LATE;
        double lateness = (task->completion_time - task->deadline).dbl();
        EV_INFO << "⚠️ Service task COMPLETED LATE (+" << lateness << "s)" << endl;
        std::cout << "SERVICE_COMPLETE_LATE: Vehicle " << getParentModule()->getIndex() 
                  << " completed service task " << task->task_id 
                  << " LATE by " << lateness << "s" << std::endl;
    }
    
    EV_INFO << "  Processing time: " << processing_time << " seconds" << endl;

    // Lifecycle: SV task completion status
    {
        std::string sv_id = "VEHICLE_" + std::to_string(getParentModule()->getIndex());
        auto orig_it = serviceTaskOriginVehicles.find(task->task_id);
        std::string orig_id = (orig_it != serviceTaskOriginVehicles.end()) ? orig_it->second : "UNKNOWN";
        std::string ev_type = (task->state == COMPLETED_ON_TIME) ? "SV_COMPLETED_ON_TIME" : "SV_COMPLETED_LATE";
        sendTaskOffloadingEvent(task->task_id, ev_type, sv_id, orig_id,
            "{\"on_time\":" + std::string(task->state == COMPLETED_ON_TIME ? "true" : "false") +
            ",\"processing_time_s\":" + std::to_string(processing_time) + "}");
    }
    
    // Remove from processing set
    processingServiceTasks.erase(task);

    // Release resources
    memory_available += task->mem_footprint_bytes;

    // Task completed → remaining service tasks get a larger CPU share.
    // Reallocate so they finish sooner.
    reallocateServiceCPUResources();
    
    cleanupTaskEvents(task);
    
    // Get origin vehicle and send result
    auto it = serviceTaskOriginVehicles.find(task->task_id);
    if (it != serviceTaskOriginVehicles.end()) {
        std::string origin_vehicle_id = it->second;
        sendServiceTaskResult(task, origin_vehicle_id);
    } else {
        EV_ERROR << "Origin vehicle not found for service task " << task->task_id << endl;
    }
    
    // Try to process next queued service task
    if (!serviceTasks.empty() && processingServiceTasks.size() < maxConcurrentServiceTasks) {
        Task* nextTask = serviceTasks.front();
        serviceTasks.pop();
        processServiceTask(nextTask);
    }
    
    // Clean up
    cleanupTaskEvents(task);
    delete task;
    
    EV_INFO << "Service vehicle queue: " << serviceTasks.size() 
            << ", processing: " << processingServiceTasks.size() << endl;
}

void PayloadVehicleApp::handleServiceTaskDeadline(Task* task) {
    // Check if task is still processing
    if (task->state != PROCESSING) {
        // Task already completed
        return;
    }
    
    EV_INFO << "\n" << endl;
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║           SERVICE TASK DEADLINE EXPIRED                                  ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    task->state = FAILED;
    task->failure_reason = "SERVICE_VEHICLE_DEADLINE_MISSED";
    task->completion_time = simTime();
    double wasted_time = (task->completion_time - task->processing_start_time).dbl();
    
    EV_INFO << "❌ Service task FAILED - deadline missed" << endl;
    EV_INFO << "  Wasted time: " << wasted_time << " seconds" << endl;
    
    std::cout << "SERVICE_FAILED: Vehicle " << getParentModule()->getIndex() 
              << " service task " << task->task_id 
              << " DEADLINE MISSED (wasted " << wasted_time << "s)" << std::endl;

    // Lifecycle: SV deadline missed
    {
        std::string sv_id = "VEHICLE_" + std::to_string(getParentModule()->getIndex());
        auto orig_it = serviceTaskOriginVehicles.find(task->task_id);
        std::string orig_id = (orig_it != serviceTaskOriginVehicles.end()) ? orig_it->second : "UNKNOWN";
        sendTaskOffloadingEvent(task->task_id, "SV_DEADLINE_MISSED", sv_id, orig_id,
            "{\"wasted_time_s\":" + std::to_string(wasted_time) + "}");
    }
    
    // Remove from processing set
    if (processingServiceTasks.find(task) != processingServiceTasks.end()) {
        processingServiceTasks.erase(task);
        memory_available += task->mem_footprint_bytes;
        
    }

    cleanupTaskEvents(task);
    
    // Send failure result back via normal relay path (SV → TV's RSU → TV).
    // For agent sub-tasks ("::"), TV's RSU intercepts and writes FAILED to Redis.
    // For regular tasks, TV receives the failure notification.
    auto it = serviceTaskOriginVehicles.find(task->task_id);
    if (it != serviceTaskOriginVehicles.end()) {
        std::string origin_vehicle_id = it->second;
        // Reuse sendServiceTaskResult which handles relay routing and map cleanup
        sendServiceTaskResult(task, origin_vehicle_id);
    }
    
    // Try to process next queued service task
    if (!serviceTasks.empty() && processingServiceTasks.size() < maxConcurrentServiceTasks) {
        Task* nextTask = serviceTasks.front();
        serviceTasks.pop();
        processServiceTask(nextTask);
    }
    
    cleanupTaskEvents(task);
    
    delete task;
    
    EV_INFO << "Service vehicle queue: " << serviceTasks.size() 
            << ", processing: " << processingServiceTasks.size() << endl;
}

// ============================================================================

void PayloadVehicleApp::logResourceState(const std::string& context) {
    EV_INFO << "┌──────────────────────────────────────────────────────────────────────────┐" << endl;
    EV_INFO << "│ RESOURCE STATE: " << std::left << std::setw(56) << context << " │" << endl;
    EV_INFO << "├──────────────────────────────────────────────────────────────────────────┤" << endl;
    EV_INFO << "│ CPU Total:       " << std::setw(38) << (cpu_total / 1e9) << " GHz      │" << endl;
    EV_INFO << "│ CPU Allocable:   " << std::setw(38) << (cpu_allocable / 1e9) << " GHz      │" << endl;
    EV_INFO << "│ CPU Available:   " << std::setw(38) << (cpu_available / 1e9) << " GHz      │" << endl;
    double cpu_util = calculateTaskCpuUtilization() * 100.0;
    EV_INFO << "│ CPU Utilization: " << std::setw(38) << cpu_util << " %        │" << endl;
    EV_INFO << "│ Memory Total:    " << std::setw(38) << (memory_total / 1e6) << " MB       │" << endl;
    EV_INFO << "│ Memory Available:" << std::setw(38) << (memory_available / 1e6) << " MB       │" << endl;
    double mem_util = ((memory_total - memory_available) / memory_total) * 100.0;
    EV_INFO << "│ Memory Util:     " << std::setw(38) << mem_util << " %        │" << endl;
    EV_INFO << "└──────────────────────────────────────────────────────────────────────────┘" << endl;
}

void PayloadVehicleApp::logQueueState(const std::string& context) {
    EV_INFO << "┌──────────────────────────────────────────────────────────────────────────┐" << endl;
    EV_INFO << "│ QUEUE STATE: " << std::left << std::setw(60) << context << " │" << endl;
    EV_INFO << "├──────────────────────────────────────────────────────────────────────────┤" << endl;
    EV_INFO << "│ Pending Tasks:    " << std::setw(38) << pending_tasks.size() << " / " 
            << std::setw(12) << max_queue_size << "│" << endl;
    EV_INFO << "│ Processing Tasks: " << std::setw(38) << processing_tasks.size() << " / " 
            << std::setw(12) << max_concurrent_tasks << "│" << endl;
    EV_INFO << "│ Completed Tasks:  " << std::setw(52) << completed_tasks.size() << "│" << endl;
    EV_INFO << "│ Failed Tasks:     " << std::setw(52) << failed_tasks.size() << "│" << endl;
    EV_INFO << "└──────────────────────────────────────────────────────────────────────────┘" << endl;
}

void PayloadVehicleApp::logTaskStatistics() {
    uint32_t total_tasks = tasks_completed_on_time + tasks_completed_late + tasks_failed + tasks_rejected;
    
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║                      TASK STATISTICS                                     ║" << endl;
    EV_INFO << "╠══════════════════════════════════════════════════════════════════════════╣" << endl;
    EV_INFO << "║ Tasks Generated:      " << std::setw(48) << tasks_generated << "║" << endl;
    EV_INFO << "║ Tasks Accepted:       " << std::setw(48) << (tasks_generated - tasks_rejected) << "║" << endl;
    EV_INFO << "║ Tasks Rejected:       " << std::setw(48) << tasks_rejected << "║" << endl;
    EV_INFO << "║ ────────────────────────────────────────────────────────────────────────║" << endl;
    EV_INFO << "║ Completed On Time:    " << std::setw(48) << tasks_completed_on_time << "║" << endl;
    EV_INFO << "║ Completed Late:       " << std::setw(48) << tasks_completed_late << "║" << endl;
    EV_INFO << "║ Failed (Deadline):    " << std::setw(48) << tasks_failed << "║" << endl;
    EV_INFO << "║ ────────────────────────────────────────────────────────────────────────║" << endl;
    
    if (total_tasks > 0) {
        double success_rate = (double)tasks_completed_on_time / total_tasks * 100.0;
        double late_rate = (double)tasks_completed_late / total_tasks * 100.0;
        double fail_rate = (double)tasks_failed / total_tasks * 100.0;
        double reject_rate = (double)tasks_rejected / total_tasks * 100.0;
        
        EV_INFO << "║ Success Rate:         " << std::setw(38) << success_rate << " %        ║" << endl;
        EV_INFO << "║ Late Completion Rate: " << std::setw(38) << late_rate << " %        ║" << endl;
        EV_INFO << "║ Failure Rate:         " << std::setw(38) << fail_rate << " %        ║" << endl;
        EV_INFO << "║ Rejection Rate:       " << std::setw(38) << reject_rate << " %        ║" << endl;
    }
    
    if (tasks_completed_on_time + tasks_completed_late > 0) {
        double avg_time = total_completion_time / (tasks_completed_on_time + tasks_completed_late);
        EV_INFO << "║ Avg Completion Time:  " << std::setw(38) << avg_time << " sec      ║" << endl;
    }
    
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    std::cout << "TASK_STATS: Vehicle " << getParentModule()->getIndex() 
              << " - Generated:" << tasks_generated 
              << " OnTime:" << tasks_completed_on_time 
              << " Late:" << tasks_completed_late 
              << " Failed:" << tasks_failed 
              << " Rejected:" << tasks_rejected << std::endl;
}

void PayloadVehicleApp::sendResourceStatusToRSU() {
    EV_INFO << "📡 Sending vehicle resource status to RSU..." << endl;
    
    // Update position and speed
    if (mobility) {
        pos = mobility->getPositionAt(simTime());
        double current_speed = mobility->getSpeed();
        
        // Calculate acceleration
        simtime_t time_delta = simTime() - last_speed_update;
        if (time_delta > 0) {
            acceleration = (current_speed - prev_speed) / time_delta.dbl();
        } else {
            acceleration = 0.0;
        }
        
        prev_speed = speed;
        speed = current_speed;
        last_speed_update = simTime();
    }
    
    const double reported_cpu_available = calculateReportedCpuAvailable();
    cpu_available = reported_cpu_available;

    // Create VehicleResourceStatusMessage
    VehicleResourceStatusMessage* statusMsg = new VehicleResourceStatusMessage();
    
    // Vehicle identification
    std::ostringstream veh_id;
    veh_id << getParentModule()->getIndex();
    statusMsg->setVehicle_id(veh_id.str().c_str());
    
    // Position and speed
    statusMsg->setPos_x(pos.x);
    statusMsg->setPos_y(pos.y);
    statusMsg->setSpeed(speed);
    statusMsg->setAcceleration(acceleration);
    double heading = mobility ? mobility->getHeading().getRad() * 180.0 / M_PI : 0.0;
    statusMsg->setHeading(heading);
    
    // Resource information
    statusMsg->setCpu_total(cpu_total);
    statusMsg->setCpu_allocable(cpu_allocable);
    statusMsg->setCpu_available(reported_cpu_available);
    // Task-allocation utilization (fraction of allocable CPU assigned to tasks)
    double task_util = calculateTaskCpuUtilization();
    // Also factor in the background vehicle CPU load (driving/safety systems modelled by
    // cpuLoadFactor).  When all tasks are force-offloaded, task_util is always 0 even
    // though the vehicle CPU is genuinely busy with on-board workloads.  We take the
    // maximum so the Digital Twin is never incorrectly shown as idle.
    double cpu_util = std::max(task_util, cpuLoadFactor);
    statusMsg->setCpu_utilization(cpu_util);
    
    statusMsg->setMem_total(memory_total);
    // Blend task memory usage with ambient OS/cache pressure for realistic DT state.
    double task_mem_util = (memory_total > 0) ? ((memory_total - memory_available) / memory_total) : 0.0;
    double mem_util = std::max(task_mem_util, memoryUsageFactor);
    mem_util = std::max(0.0, std::min(1.0, mem_util));
    double reported_mem_available = memory_total * (1.0 - mem_util);
    statusMsg->setMem_available(reported_mem_available);
    statusMsg->setMem_utilization(mem_util);
    
    // Task statistics
    statusMsg->setTasks_generated(tasks_generated);
    statusMsg->setTasks_completed_on_time(tasks_completed_on_time);
    statusMsg->setTasks_completed_late(tasks_completed_late);
    statusMsg->setTasks_failed(tasks_failed);
    statusMsg->setTasks_rejected(tasks_rejected);
    statusMsg->setCurrent_queue_length(pending_tasks.size());
    statusMsg->setCurrent_processing_count(processing_tasks.size());
    
    // Calculate average completion time and deadline miss ratio
    if (tasks_completed_on_time + tasks_completed_late > 0) {
        statusMsg->setAvg_completion_time(total_completion_time / (tasks_completed_on_time + tasks_completed_late));
    } else {
        statusMsg->setAvg_completion_time(0.0);
    }
    
    uint32_t total_completed = tasks_completed_on_time + tasks_completed_late;
    if (total_completed > 0) {
        statusMsg->setDeadline_miss_ratio((double)tasks_completed_late / total_completed);
    } else {
        statusMsg->setDeadline_miss_ratio(0.0);
    }

    double battery_pct = (battery_mAh_max > 0.0) ? (battery_mAh_current / battery_mAh_max) * 100.0 : 0.0;
    statusMsg->setBattery_level_pct(battery_pct);
    statusMsg->setBattery_current_mAh(battery_mAh_current);
    statusMsg->setBattery_capacity_mAh(battery_mAh_max);
    statusMsg->setEnergy_task_j_total(task_energy_j_total);
    statusMsg->setEnergy_task_j_last(task_energy_j_last);
    statusMsg->setVehicle_type(vehicle_type.c_str());
    statusMsg->setTx_power_mw(txPower_mW);
    statusMsg->setStorage_capacity_gb(storage_capacity_gb);
    statusMsg->setMax_queue_size(static_cast<uint32_t>(max_queue_size));
    statusMsg->setMax_concurrent_tasks(static_cast<uint32_t>(max_concurrent_tasks));

    // Set sender MAC so RSU can register this vehicle's L2 address from heartbeats alone.
    statusMsg->setSenderAddress(myId);

    // Broadcast to all RSUs so each RSU maintains its own synchronized DT copy.
    populateWSM(statusMsg);
    sendDown(statusMsg);
    EV_INFO << "✓ Vehicle resource status broadcast to all RSUs" << endl;
    std::cout << "RESOURCE_BROADCAST: Vehicle " << getParentModule()->getIndex()
              << " broadcast status - CPU:" << (cpu_util * 100.0)
              << "% Mem:" << (mem_util * 100.0) << "% Queue:" << pending_tasks.size() << std::endl;
    exportRouteProgressToRedis();
}

void PayloadVehicleApp::sendVehicleResourceStatus() {
    sendResourceStatusToRSU();
}

void PayloadVehicleApp::exportRouteProgressToRedis() {
    if (!routeProgressRedisEnabled || !routeProgressRedisClient || !routeProgressRedisClient->isConnected() || !mobility) {
        return;
    }

    std::string edge_id;
    double lane_pos_m = 0.0;

    try {
        edge_id = mobility->getRoadId();
        auto* vehicle_if = mobility->getVehicleCommandInterface();
        if (vehicle_if) {
            lane_pos_m = vehicle_if->getLanePosition();
        }
    }
    catch (const cRuntimeError&) {
        return;
    }

    if (edge_id.empty()) {
        return;
    }

    const std::string vehicle_id = std::to_string(getParentModule()->getIndex());
    routeProgressRedisClient->updateVehicleRouteProgress(vehicle_id, simTime().dbl(), edge_id, lane_pos_m);
}

// ============================================================================
// COOPERATIVE PERCEPTION DATA SHARING (VEHICLE-TO-VEHICLE)
// ============================================================================

void PayloadVehicleApp::sendObjectDetectionData(const Task* task) {
    if (task->output_size_bytes == 0) {
        return;
    }

    auto* msg = new veins::ObjectDetectionDataMessage();
    msg->setVehicle_id(task->vehicle_id.c_str());
    msg->setTimestamp(simTime().dbl());
    msg->setData_size_bytes(task->output_size_bytes);
    msg->setPayload_tag("LOCAL_OBJECT_DETECTION");

    populateWSM(msg);
    sendDown(msg);

    EV_INFO << "📤 Broadcasted object detection data (" << (task->output_size_bytes / 1024.0)
            << " KB) from vehicle " << task->vehicle_id << endl;
}

void PayloadVehicleApp::handleObjectDetectionDataMessage(veins::ObjectDetectionDataMessage* msg) {
    std::string sender_id = msg->getVehicle_id();
    std::string self_id = std::to_string(getParentModule()->getIndex());

    if (sender_id == self_id) {
        delete msg;
        return;
    }

    ObjectDetectionSnapshot snapshot;
    snapshot.timestamp = SimTime(msg->getTimestamp());
    snapshot.size_bytes = msg->getData_size_bytes();
    neighborObjectDetections[sender_id] = snapshot;

    EV_INFO << "📥 Received object detection data from vehicle " << sender_id
            << " (" << (snapshot.size_bytes / 1024.0) << " KB)" << endl;

    delete msg;
}

uint32_t PayloadVehicleApp::countFreshNeighborDetections() {
    uint32_t fresh_count = 0;
    simtime_t now = simTime();

    for (auto it = neighborObjectDetections.begin(); it != neighborObjectDetections.end(); ) {
        double age = (now - it->second.timestamp).dbl();
        if (age <= objectDetectionTtlSec) {
            fresh_count++;
            ++it;
        } else {
            it = neighborObjectDetections.erase(it);
        }
    }

    return fresh_count;
}

// ============================================================================
// MODERN MULTI-CRITERIA RSU SELECTION IMPLEMENTATION
// ============================================================================

LAddress::L2Type PayloadVehicleApp::selectBestRSU() {
    // Get vehicle position
    Coord vehiclePos = mobility ? mobility->getPositionAt(simTime()) : Coord(0, 0, 0);
    cModule* networkModule = getModuleByPath("^.^");
    if (!networkModule) {
        return 0;
    }
    
    // Update metrics for all RSUs
    for (int i = 0; i < 3; i++) {
        cModule* rsuModule = networkModule->getSubmodule("rsu", i);
        if (rsuModule) {
            cModule* rsuMobilityModule = rsuModule->getSubmodule("mobility");
            if (rsuMobilityModule) {
                double rsuX = rsuMobilityModule->par("x").doubleValue();
                double rsuY = rsuMobilityModule->par("y").doubleValue();
                double dx = vehiclePos.x - rsuX;
                double dy = vehiclePos.y - rsuY;
                double distance = sqrt(dx * dx + dy * dy);
                
                // Initialize or update RSU metrics
                if (rsuMetrics.find(i) == rsuMetrics.end()) {
                    rsuMetrics[i] = RSUMetrics();
                    // Get MAC address from RSU's MAC interface
                    DemoBaseApplLayerToMac1609_4Interface* rsuMacInterface =
                        FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(rsuModule);
                    if (rsuMacInterface) {
                        rsuMetrics[i].macAddress = rsuMacInterface->getMACAddress();
                    }
                }
                
                rsuMetrics[i].distance = distance;

                // Derive available RSU CPU from current load estimate.
                // When load telemetry is unavailable, defaults keep this at nominal RSU CPU (from EnergyModel).
                const double nominal_rsu_cpu_ghz = EnergyConstants::FREQ_RSU_NOMINAL / 1e9;
                double load_ratio = 0.0;
                if (rsuMetrics[i].rsu_max_concurrent > 0) {
                    load_ratio = static_cast<double>(rsuMetrics[i].rsu_processing_count) /
                                 static_cast<double>(rsuMetrics[i].rsu_max_concurrent);
                }
                load_ratio = std::max(0.0, std::min(1.0, load_ratio));
                rsuMetrics[i].rsu_cpu_available_ghz = std::max(0.1, nominal_rsu_cpu_ghz * (1.0 - load_ratio));

                // RSSI estimation: TwoRay Ground Reflection (matches omnetpp.ini pathLoss config)
                // RSU TX = 2000mW = 33 dBm; antenna heights ht=hr=1.5m; carrier=5.89GHz
                const double RSU_TX_dBm = 33.0;
                const double freq = 5.89e9, c = 3e8;
                const double ht = 1.5, hr = 1.5;
                const double lambda_est = c / freq;
                const double d_break = 4.0 * ht * hr / lambda_est;  // ~706m breakpoint
                double estimatedRSSI;
                if (distance < d_break) {
                    // FSPL regime (short range, < ~706m)
                    double fspl = 20*log10(distance) + 20*log10(freq) + 20*log10(4*M_PI/c);
                    estimatedRSSI = RSU_TX_dBm - fspl;
                } else {
                    // TwoRay regime (beyond breakpoint): ~40*log10(d)
                    double loss = 40*log10(distance) - 20*log10(ht) - 20*log10(hr);
                    estimatedRSSI = RSU_TX_dBm - loss;
                }
                
                // 70/30 blend: prevents stale RSSI misleading the quality gate as vehicle moves
                if (rsuMetrics[i].lastRSSI < -100) {
                    rsuMetrics[i].lastRSSI = estimatedRSSI;  // first observation
                } else {
                    rsuMetrics[i].lastRSSI = 0.7 * rsuMetrics[i].lastRSSI + 0.3 * estimatedRSSI;
                }
            }
        }
    }
    
    // Filter viable RSUs and calculate scores
    LAddress::L2Type bestRSU = 0;
    double bestScore = -999;
    int bestIndex = -1;
    
    for (auto& pair : rsuMetrics) {
        int rsuIndex = pair.first;
        RSUMetrics& metrics = pair.second;
        
        // Check blacklist condition
        if (metrics.consecutiveFailures >= failure_blacklist_threshold) {
            simtime_t timeSinceLastFailure = simTime() - metrics.lastContactTime;
            if (timeSinceLastFailure < blacklist_duration) {
                continue;  // Skip blacklisted RSU
            } else {
                // Reset blacklist after timeout
                metrics.consecutiveFailures = 0;
            }
        }

        // Check minimum RSSI threshold
        if (metrics.lastRSSI < rssi_threshold) {
            continue;  // Skip RSUs with too weak signal
        }

        // Calculate multi-criteria score
        metrics.score = calculateRSUScore(metrics);

        // Apply hysteresis: favor currently selected RSU
        if (metrics.macAddress == currentRSU && currentRSU != 0) {
            metrics.score += 0.1;  // 10% bonus for staying with current RSU
        }

        // Track best candidate
        if (metrics.score > bestScore) {
            bestScore = metrics.score;
            bestRSU = metrics.macAddress;
            bestIndex = rsuIndex;
        }
    }
    
    // Check if we should switch RSUs (hysteresis)
    if (bestRSU != 0 && bestRSU != currentRSU && currentRSU != 0) {
        // Switching to new RSU - check if improvement is significant enough
        if (bestScore - rsuMetrics[bestIndex].score < 0.15) {  // Need >15% improvement
            return currentRSU;  // Stay with current RSU
        }
    }

    if (bestRSU != 0) {
        currentRSU = bestRSU;
        return bestRSU;
    }

    return findRSUMacAddress();  // Fallback to old method
}

void PayloadVehicleApp::updateRSUMetrics(int rsuIndex, bool messageSuccess, double rssi) {
    if (rsuMetrics.find(rsuIndex) == rsuMetrics.end()) {
        rsuMetrics[rsuIndex] = RSUMetrics();
    }
    
    RSUMetrics& metrics = rsuMetrics[rsuIndex];
    
    // Update RSSI if provided
    if (rssi > -999) {
        metrics.lastRSSI = rssi;
    }
    
    // Update reception history for PRR calculation
    metrics.receptionHistory.push_back(messageSuccess);
    if (metrics.receptionHistory.size() > (size_t)prr_window_size) {
        metrics.receptionHistory.pop_front();  // Keep only recent history
    }
    
    // Update failure tracking
    if (messageSuccess) {
        metrics.consecutiveFailures = 0;  // Reset on success
        metrics.lastContactTime = simTime();
    } else {
        metrics.consecutiveFailures++;
    }
    
    std::cout << "RSU_METRICS: RSU[" << rsuIndex << "] updated - success=" << messageSuccess 
              << ", PRR=" << metrics.getPRR() << ", failures=" << metrics.consecutiveFailures << std::endl;
}

double PayloadVehicleApp::calculateRSUScore(const RSUMetrics& metrics) {
    // Multi-criteria weighted scoring (weights sum to 1.0)
    double score = 0.0;
    
    // 1. RSSI component (40% weight) - normalized to 0-1
    double rssiScore = normalizeValue(metrics.lastRSSI, rssi_threshold, -40.0);
    score += rssiScore * 0.4;
    
    // 2. PRR component (30% weight)
    double prrScore = metrics.getPRR();
    score += prrScore * 0.3;
    
    // 3. Distance component (20% weight) - closer is better
    double distScore = normalizeValue(2000.0 - metrics.distance, 0, 2000.0);  // Invert
    score += distScore * 0.2;
    
    // 4. Recency component (10% weight) - recent contact is better
    double timeSinceContact = (simTime() - metrics.lastContactTime).dbl();
    double recencyScore = normalizeValue(10.0 - timeSinceContact, 0, 10.0);  // Penalize old contacts
    score += recencyScore * 0.1;
    
    return score;
}

double PayloadVehicleApp::normalizeValue(double value, double min, double max) {
    if (max <= min) return 0.0;
    double normalized = (value - min) / (max - min);
    // Clamp to [0, 1]
    if (normalized < 0.0) normalized = 0.0;
    if (normalized > 1.0) normalized = 1.0;
    return normalized;
}

std::vector<veins::LAddress::L2Type> PayloadVehicleApp::buildRankedRSUCandidates() {
    EV_DEBUG << "🎯 Building ranked RSU candidate list..." << endl;
    
    // Get vehicle position
    Coord vehiclePos = mobility ? mobility->getPositionAt(simTime()) : Coord(0, 0, 0);
    cModule* networkModule = getModuleByPath("^.^");
    if (!networkModule) {
        EV_WARN << "Network module not found!" << endl;
        return std::vector<veins::LAddress::L2Type>();
    }
    
    // Update metrics for all RSUs and collect viable candidates
    struct RSUCandidate {
        int index;
        LAddress::L2Type mac;
        double score;
        double distance;
        double rssi;
    };
    
    std::vector<RSUCandidate> candidates;
    
    for (int i = 0; i < 3; i++) {  // Assuming 3 RSUs (make configurable if needed)
        cModule* rsuModule = networkModule->getSubmodule("rsu", i);
        if (!rsuModule) continue;
        
        cModule* rsuMobilityModule = rsuModule->getSubmodule("mobility");
        if (!rsuMobilityModule) continue;
        
        double rsuX = rsuMobilityModule->par("x").doubleValue();
        double rsuY = rsuMobilityModule->par("y").doubleValue();
        double dx = vehiclePos.x - rsuX;
        double dy = vehiclePos.y - rsuY;
        double distance = sqrt(dx * dx + dy * dy);
        
        // Initialize or update RSU metrics
        if (rsuMetrics.find(i) == rsuMetrics.end()) {
            rsuMetrics[i] = RSUMetrics();
            DemoBaseApplLayerToMac1609_4Interface* rsuMacInterface =
                FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(rsuModule);
            if (rsuMacInterface) {
                rsuMetrics[i].macAddress = rsuMacInterface->getMACAddress();
            }
        }
        
        rsuMetrics[i].distance = distance;
        
        // Estimate RSSI based on distance
        double lambda = 3e8 / 5.89e9;
        double fspl = 20 * log10(distance) + 20 * log10(5.89e9) + 20 * log10(4 * M_PI / 3e8);
        double estimatedRSSI = 13.0 - fspl;
        
        if (rsuMetrics[i].lastRSSI < -100) {
            rsuMetrics[i].lastRSSI = estimatedRSSI;
        }
        
        // Check if RSU is viable (not blacklisted, meets RSSI threshold)
        bool viable = true;
        
        // Check blacklist
        if (rsuMetrics[i].consecutiveFailures >= failure_blacklist_threshold) {
            simtime_t timeSinceLastFailure = simTime() - rsuMetrics[i].lastContactTime;
            if (timeSinceLastFailure < blacklist_duration) {
                viable = false;
            } else {
                rsuMetrics[i].consecutiveFailures = 0;  // Reset blacklist
            }
        }
        
        // Check RSSI threshold
        if (rsuMetrics[i].lastRSSI < rssi_threshold) {
            viable = false;
        }
        
        if (viable) {
            RSUCandidate candidate;
            candidate.index = i;
            candidate.mac = rsuMetrics[i].macAddress;
            candidate.score = calculateRSUScore(rsuMetrics[i]);
            candidate.distance = distance;
            candidate.rssi = rsuMetrics[i].lastRSSI;
            candidates.push_back(candidate);
        }
    }
    
    // Sort candidates by score (descending)
    std::sort(candidates.begin(), candidates.end(),
              [](const RSUCandidate& a, const RSUCandidate& b) {
                  return a.score > b.score;  // Higher score is better
              });
    
    // Build result vector (limit to max_candidate_rsus)
    std::vector<veins::LAddress::L2Type> result;
    for (size_t i = 0; i < candidates.size() && i < (size_t)max_candidate_rsus; i++) {
        result.push_back(candidates[i].mac);
        EV_DEBUG << "  Candidate " << i << ": RSU[" << candidates[i].index 
                 << "] MAC=" << candidates[i].mac 
                 << " score=" << candidates[i].score
                 << " dist=" << candidates[i].distance << "m"
                 << " rssi=" << candidates[i].rssi << "dBm" << endl;
        std::cout << "RSU_CANDIDATE[" << i << "]: RSU[" << candidates[i].index 
                  << "] score=" << candidates[i].score << " dist=" << candidates[i].distance << "m" << std::endl;
    }
    
    EV_DEBUG << "Built candidate list with " << result.size() << " RSUs" << endl;
    return result;
}

// ============================================================================
// TASK OFFLOADING HELPER METHODS
// ============================================================================

double PayloadVehicleApp::getRSUDistance() {
    // Get distance to currently selected RSU
    if (currentRSU == 0) {
        EV_WARN << "No RSU currently selected, selecting best RSU" << endl;
        currentRSU = selectBestRSU();
        if (currentRSU == 0) {
            EV_WARN << "No RSU available" << endl;
            return 999999.0;  // Very large distance if no RSU
        }
    }
    
    // Find the RSU index for currentRSU MAC address
    for (const auto& pair : rsuMetrics) {
        if (pair.second.macAddress == currentRSU) {
            EV_DEBUG << "Distance to RSU: " << pair.second.distance << " meters" << endl;
            return pair.second.distance;
        }
    }
    
    // If not found in metrics, return a default high distance
    EV_WARN << "Current RSU not found in metrics" << endl;
    return 1000.0;
}

double PayloadVehicleApp::getEstimatedRSSI() {
    // Get RSSI from currently selected RSU
    if (currentRSU == 0) {
        EV_WARN << "No RSU currently selected, selecting best RSU" << endl;
        currentRSU = selectBestRSU();
        if (currentRSU == 0) {
            EV_WARN << "No RSU available" << endl;
            return -999.0;  // Very weak signal if no RSU
        }
    }
    
    // Find the RSU index for currentRSU MAC address
    for (const auto& pair : rsuMetrics) {
        if (pair.second.macAddress == currentRSU) {
            double rssi = pair.second.lastRSSI;
            EV_DEBUG << "RSSI from RSU: " << rssi << " dBm" << endl;
            return rssi;
        }
    }
    
    // If not found in metrics, return a default weak RSSI
    EV_WARN << "Current RSU not found in metrics" << endl;
    return -90.0;
}

double PayloadVehicleApp::estimateTransmissionTime(Task* task) {
    // IEEE 802.11p DSRC Channel Rate: 3-27 Mbps (typically 6 Mbps for reliability)
    double bandwidth_mbps = kDefaultV2xLinkBandwidthMbps;  // Conservative estimate for reliable transmission
    
    
    // Calculate transmission time based on input bytes sent over the air
    double transmission_time = (task->input_size_bytes * 8.0) / (bandwidth_mbps * 1e6);
    
    // Add propagation delay (distance / speed of light)
    double distance = getRSUDistance();
    double propagation_delay = distance / 3e8;  // Speed of light in m/s
    
    // Add processing overhead (~1ms for MAC layer)
    double overhead = 0.001;
    
    double total_time = transmission_time + propagation_delay + overhead;
    
    EV_DEBUG << "Transmission estimate: " << (transmission_time * 1000) << "ms data + "
             << (propagation_delay * 1e6) << "us propagation + 1ms overhead = "
             << (total_time * 1000) << "ms total" << endl;
    
    return total_time;
}

// ============================================================================
// OFFLOADING REQUEST/RESPONSE HANDLERS
// ============================================================================

void PayloadVehicleApp::sendOffloadingRequestToRSU(Task* task, OffloadingDecision localDecision, const GateBDecisionResult& gateResult) {
    EV_INFO << "📤 Sending offloading request to RSU for task " << task->task_id << endl;
    std::cout << "OFFLOAD_Decision_REQUEST: Vehicle " << task->vehicle_id << " requesting offloading decision for task " 
              << task->task_id << std::endl;
    
    // Create offloading request message
    veins::OffloadingRequestMessage* msg = new veins::OffloadingRequestMessage();
    
    // Task identification and characteristics
    msg->setTask_id(task->task_id.c_str());
    msg->setVehicle_id(task->vehicle_id.c_str());
    msg->setRequest_time(simTime().dbl());
    msg->setMem_footprint_bytes(task->mem_footprint_bytes);
    msg->setCpu_cycles(task->cpu_cycles);
    msg->setDeadline_seconds(std::max(0.001, task->relative_deadline - (simTime() - task->created_time).dbl()));
    msg->setQos_value(task->qos_value);
    
    // Vehicle resource state
    msg->setLocal_cpu_available_ghz(calculateReportedCpuAvailable() / 1e9);
    msg->setLocal_cpu_utilization(calculateTaskCpuUtilization());
    msg->setLocal_mem_available_mb(memory_available / 1e6);
    msg->setLocal_queue_length(pending_tasks.size());
    msg->setLocal_processing_count(processing_tasks.size());
    
    // Vehicle location and mobility
    Coord pos = mobility->getPositionAt(simTime());
    msg->setPos_x(pos.x);
    msg->setPos_y(pos.y);
    msg->setSpeed(mobility->getSpeed());
    
    // Local decision recommendation
    std::string decisionStr;
    switch(localDecision) {
        case OffloadingDecision::EXECUTE_LOCALLY:
            decisionStr = "LOCAL";
            break;
        case OffloadingDecision::OFFLOAD_TO_RSU:
            decisionStr = "OFFLOAD_TO_RSU";
            break;
        case OffloadingDecision::OFFLOAD_TO_SERVICE_VEHICLE:
            decisionStr = "OFFLOAD_TO_SERVICE_VEHICLE";
            break;
        case OffloadingDecision::REJECT_TASK:
            decisionStr = "REJECT";
            break;
        default:
            decisionStr = "UNKNOWN";
    }
    msg->setLocal_decision(decisionStr.c_str());

    std::string gateClassification;
    switch (gateResult.classification) {
        case GateBClassification::MUST_OFFLOAD:
            gateClassification = "MUST_OFFLOAD";
            break;
        case GateBClassification::MUST_LOCAL:
            gateClassification = "MUST_LOCAL";
            break;
        case GateBClassification::BOTH_FEASIBLE:
            gateClassification = "BOTH_FEASIBLE";
            break;
        case GateBClassification::INFEASIBLE:
        default:
            gateClassification = "INFEASIBLE";
            break;
    }
    msg->setInitial_gate_classification(gateClassification.c_str());
    msg->setInitial_gate_reason(gateResult.reason.c_str());
    
    // Set sender address (our MAC)
    msg->setSenderAddress(myId);
    
    // Build ranked RSU candidate list for redirect support
    std::vector<veins::LAddress::L2Type> candidates = buildRankedRSUCandidates();
    
    if (candidates.empty()) {
        EV_ERROR << "No viable RSU candidates available" << endl;
        delete msg;
        return;
    }
    
    // Populate candidate array in message
    msg->setCandidate_rsu_macsArraySize(candidates.size());
    for (size_t i = 0; i < candidates.size(); i++) {
        msg->setCandidate_rsu_macs(i, candidates[i]);
    }
    
    // Initialize redirect tracking
    msg->setCurrent_candidate_index(0);  // Start with best candidate
    msg->setMax_redirect_hops(max_redirect_hops);
    
    // Send to first candidate (best RSU)
    LAddress::L2Type rsuMac = candidates[0];

    populateWSM(msg, rsuMac);
    sendDown(msg);
    
    // Mark task as awaiting decision
    PendingOffloadDecision pending;
    pending.task = task;
    pending.classification = gateResult.classification;
    pending.must_offload = (gateResult.classification == GateBClassification::MUST_OFFLOAD);
    pending.t_local_seconds = gateResult.t_local_seconds;
    pending.remaining_deadline_seconds = gateResult.remaining_deadline_seconds;
    const double safety_margin = std::max(0.0, par("gateSafetyMarginSec").doubleValue());
    const double remaining = std::max(0.0, gateResult.remaining_deadline_seconds);
    if (pending.must_offload) {
        // Step 9: MUST_OFFLOAD waits as long as remaining deadline allows.
        pending.rsu_timeout_budget_seconds = std::max(0.001, remaining);
    } else {
        // Step 9: BOTH_FEASIBLE keeps room for local fallback execution.
        double both_budget = std::min(remaining,
            std::max(0.001, remaining - gateResult.t_local_seconds - safety_margin));
        pending.rsu_timeout_budget_seconds = std::max(0.001, both_budget);
    }
    pending.reason = gateResult.reason;
    pendingOffloadingDecisions[task->task_id] = pending;
    
    // Initialize redirect counter for this task
    task_redirect_counts[task->task_id] = 0;
    
    // Store candidate list for potential redirects
    taskCandidates[task->task_id] = candidates;
    
    // Record timing information for latency tracking
    TaskTimingInfo timing;
    timing.request_time = simTime().dbl();
    taskTimings[task->task_id] = timing;
    
    EV_INFO << "Offloading request sent to RSU[0] (MAC: " << rsuMac << ") with " 
            << candidates.size() << " candidate RSUs" << endl;
    std::cout << "INFO: Offloading request sent to primary RSU with " << candidates.size() 
              << " fallback candidates" << std::endl;
}

void PayloadVehicleApp::handleOffloadingDecisionFromRSU(veins::OffloadingDecisionMessage* msg) {
    EV_INFO << "📥 Received offloading decision from RSU" << endl;
    std::string taskId = msg->getTask_id();
    std::string decisionType = msg->getDecision_type();
    
    std::cout << "INFO: 📥 Received offloading decision for task " << taskId 
              << ": " << decisionType << std::endl;
    
    // Find the task in pending decisions
    auto it = pendingOffloadingDecisions.find(taskId);
    if (it == pendingOffloadingDecisions.end()) {
        EV_WARN << "Task " << taskId << " not found in pending decisions (may have timed out)" << endl;
        delete msg;
        return;
    }
    
    Task* task = it->second.task;
    pendingOffloadingDecisions.erase(it);  // Remove from pending
    
    // Record decision time for latency tracking
    if (taskTimings.find(taskId) != taskTimings.end()) {
        taskTimings[taskId].decision_time = simTime().dbl();
        taskTimings[taskId].decision_type = decisionType;
        std::cout << "INFO: 📊 Decision latency: " << (taskTimings[taskId].decision_time - taskTimings[taskId].request_time) << "s" << std::endl;
    }
    
    // Cancel the timeout message for this task
    auto timeoutIt = pendingDecisionTimeouts.find(taskId);
    if (timeoutIt != pendingDecisionTimeouts.end()) {
        cMessage* timeoutMsg = timeoutIt->second;
        if (timeoutMsg->isScheduled()) {
            cancelAndDelete(timeoutMsg);
        } else {
            delete timeoutMsg;
        }
        pendingDecisionTimeouts.erase(timeoutIt);
        EV_INFO << "✓ Cancelled timeout for task " << taskId << endl;
    }
    
    EV_INFO << "✓ Task found: " << task->task_id << endl;
    EV_INFO << "  Decision: " << decisionType << endl;
    EV_INFO << "  Confidence: " << msg->getConfidence_score() << endl;
    EV_INFO << "  Est. completion: " << msg->getEstimated_completion_time() << "s" << endl;
    EV_INFO << "  Reason: " << msg->getDecision_reason() << endl;
    
    // Send lifecycle event with explicit wait and sender context for timeout analysis.
    double decision_wait_s = 0.0;
    auto timingIt = taskTimings.find(taskId);
    if (timingIt != taskTimings.end()) {
        decision_wait_s = std::max(0.0, simTime().dbl() - timingIt->second.request_time);
    } else {
        decision_wait_s = std::max(0.0, simTime().dbl() - task->created_time.dbl());
    }
    sendTaskOffloadingEvent(taskId, "DECISION_RECEIVED", "RSU", task->vehicle_id,
        std::string("{\"decision_wait_s\":") + std::to_string(decision_wait_s) + ","
        "\"decision_type\":\"" + decisionType + "\","
        "\"rsu_sender_mac\":\"" + std::to_string(msg->getSenderAddress()) + "\"}");
    
    // Cache any shadow-evaluation payload before executing the real decision
    // because some execution branches may consume or delete the task.
    const std::string agentDec = msg->getAgentDecisions();
    const LAddress::L2Type dispatchRsuMac = msg->getSenderAddress();
    if (!agentDec.empty()) {
        std::cout << "TV_BASELINE_DISPATCH: task=" << taskId
                  << " agentDec=[" << agentDec << "]" << std::endl;
        dispatchBaselineSubTasks(task, agentDec, dispatchRsuMac);
    }

    // Execute the real DDQN decision
    executeOffloadingDecision(task, msg);

    delete msg;
}

// ---------------------------------------------------------------------------
// dispatchBaselineSubTasks — fire-and-forget shadow evaluations for DRL
// ---------------------------------------------------------------------------
// The agentDecisions string is CSV: "agent:TYPE:target_id:target_mac,..."
// For each NON-ddqn agent, we create a TaskOffloadPacket with sub-task ID
// "{orig_task_id}::{agent_name}" and send it to the target.
// Results flow back to TV's RSU via normal relay → RSU writes metrics to Redis.
// ---------------------------------------------------------------------------
void PayloadVehicleApp::dispatchBaselineSubTasks(Task* task,
                                                  const std::string& agentDecisions,
                                                  veins::LAddress::L2Type dispatchRsuMac) {
    // Parse CSV
    std::vector<std::string> entries;
    {
        std::stringstream ss(agentDecisions);
        std::string tok;
        while (std::getline(ss, tok, ',')) {
            if (!tok.empty()) entries.push_back(tok);
        }
    }

    for (const auto& entry : entries) {
        // Parse "agent:TYPE:target_id:target_mac"
        std::vector<std::string> parts;
        {
            std::stringstream ss(entry);
            std::string p;
            while (std::getline(ss, p, ':')) parts.push_back(p);
        }
        if (parts.size() < 4) continue;
        std::string agent_name = parts[0];
        std::string target_type = parts[1];
        std::string target_id = parts[2];
        LAddress::L2Type target_mac = 0;
        try { target_mac = static_cast<LAddress::L2Type>(std::stoull(parts[3])); }
        catch (...) {}

        // Skip DDQN — its metrics come from the real task execution
        if (agent_name == "ddqn") continue;

        std::string sub_id = task->task_id + "::" + agent_name;

        // Pass remaining deadline so the target node uses accurate timing.
        // Using original relative_deadline would give the SV/RSU a full 0.5s
        // even though 0.1-0.3s has already elapsed — corrupting deadline checks.
        double elapsed_s = (simTime() - task->created_time).dbl();
        double remaining_deadline_s = std::max(0.001, task->relative_deadline - elapsed_s);

        if (target_type == "RSU") {
            // Send to the dispatch RSU (TV's RSU) for local processing.
            // If target_id != this RSU, the route hint will forward it.
            veins::TaskOffloadPacket* pkt = new veins::TaskOffloadPacket();
            pkt->setTask_id(sub_id.c_str());
            pkt->setOrigin_vehicle_id(task->vehicle_id.c_str());
            pkt->setOrigin_vehicle_mac(myId);
            pkt->setOffload_time(simTime().dbl());
            pkt->setMem_footprint_bytes(task->mem_footprint_bytes);
            pkt->setCpu_cycles(task->cpu_cycles);
            pkt->setDeadline_seconds(remaining_deadline_s);
            pkt->setQos_value(task->qos_value);
            pkt->setTask_input_data("{}");  // no route hint — goes to TV's nearest RSU

            LAddress::L2Type rsuMac = (dispatchRsuMac != 0) ? dispatchRsuMac : selectBestRSU();
            if (rsuMac != 0) {
                populateWSM(pkt, rsuMac);
                sendDown(pkt);
                std::cout << "TV_AGENT_SUBTASK: " << sub_id << " → RSU" << std::endl;
            } else {
                delete pkt;
                std::cout << "TV_AGENT_SUBTASK: " << sub_id << " DROPPED (no RSU)" << std::endl;
            }

        } else if (target_type == "SERVICE_VEHICLE") {
            if (target_mac == 0) {
                // SV MAC unknown — fall back to RSU processing for this sub-task
                std::cout << "TV_AGENT_SUBTASK: " << sub_id
                          << " SV " << target_id << " MAC=0 → RSU fallback" << std::endl;
                veins::TaskOffloadPacket* pkt = new veins::TaskOffloadPacket();
                pkt->setTask_id(sub_id.c_str());
                pkt->setOrigin_vehicle_id(task->vehicle_id.c_str());
                pkt->setOrigin_vehicle_mac(myId);
                pkt->setOffload_time(simTime().dbl());
                pkt->setMem_footprint_bytes(task->mem_footprint_bytes);
                pkt->setCpu_cycles(task->cpu_cycles);
                pkt->setDeadline_seconds(remaining_deadline_s);
                pkt->setQos_value(task->qos_value);
                pkt->setTask_input_data("{}");
                LAddress::L2Type rsuMac = (dispatchRsuMac != 0) ? dispatchRsuMac : selectBestRSU();
                if (rsuMac != 0) {
                    populateWSM(pkt, rsuMac);
                    sendDown(pkt);
                } else {
                    delete pkt;
                }
                continue;
            }

            veins::TaskOffloadPacket* pkt = new veins::TaskOffloadPacket();
            pkt->setTask_id(sub_id.c_str());
            pkt->setOrigin_vehicle_id(task->vehicle_id.c_str());
            pkt->setOrigin_vehicle_mac(myId);
            pkt->setOffload_time(simTime().dbl());
            pkt->setMem_footprint_bytes(task->mem_footprint_bytes);
            pkt->setCpu_cycles(task->cpu_cycles);
            pkt->setDeadline_seconds(remaining_deadline_s);
            pkt->setQos_value(task->qos_value);

            // Try direct V2V first; fall back to infrastructure relay via TV's RSU
            Coord myPos = mobility ? mobility->getPositionAt(simTime()) : Coord(0,0,0);
            Coord svPos;
            bool svPosKnown = lookupVehiclePositionById(target_id, svPos);
            bool directOk = false;
            if (svPosKnown) {
                double rssi = estimateV2vRssiDbm(myPos, svPos);
                directOk = (rssi >= serviceDirectRssiThresholdDbm);
            }

            if (directOk) {
                pkt->setTask_input_data("{}");
                populateWSM(pkt, target_mac);
                sendDown(pkt);
                std::cout << "TV_AGENT_SUBTASK: " << sub_id
                          << " → SV " << target_id << " (direct)" << std::endl;
            } else {
                // Route via RSU infrastructure: TV → TV's RSU → (anchor) → SV
                LAddress::L2Type anchorRsu = (dispatchRsuMac != 0) ? dispatchRsuMac : selectBestRSU();
                std::string hint = buildServiceRouteHint(target_mac, anchorRsu, myId);
                pkt->setTask_input_data(hint.c_str());
                LAddress::L2Type firstHop = (dispatchRsuMac != 0) ? dispatchRsuMac : selectBestRSU();
                if (firstHop != 0) {
                    populateWSM(pkt, firstHop);
                    sendDown(pkt);
                    std::cout << "TV_AGENT_SUBTASK: " << sub_id
                              << " → SV " << target_id << " (via RSU)" << std::endl;
                } else {
                    delete pkt;
                    std::cout << "TV_AGENT_SUBTASK: " << sub_id
                              << " DROPPED (no RSU for relay)" << std::endl;
                }
            }
        }
    }
}

void PayloadVehicleApp::executeOffloadingDecision(Task* task, veins::OffloadingDecisionMessage* decision) {
    EV_INFO << "⚙️ Executing offloading decision for task " << task->task_id << endl;
    
    std::string decisionType = decision->getDecision_type();
    
    // ========================================================================
    // DECISION: EXECUTE LOCALLY
    // ========================================================================
    if (decisionType == "LOCAL") {
                sendTaskOffloadingEvent(task->task_id, "DECISION_LOCAL",
                    "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "SELF",
                    "{\"reason\":\"RSU_DECISION_LOCAL\"}");

        EV_INFO << "💻 Decision: EXECUTE LOCALLY" << endl;
        std::cout << "OFFLOAD_EXEC: Task " << task->task_id << " executing LOCALLY" << std::endl;
        
        // Track processor and decision time for completion report
        if (taskTimings.find(task->task_id) != taskTimings.end()) {
            taskTimings[task->task_id].processor_id = "VEHICLE_" + std::to_string(getParentModule()->getIndex());
            taskTimings[task->task_id].decision_time = simTime().dbl();
            taskTimings[task->task_id].decision_type = "LOCAL";
        } else {
            // Create timing entry if it wasn't created yet (e.g. rsuDecisionTimeout fallback)
            TaskTimingInfo timing;
            timing.request_time = simTime().dbl();
            timing.decision_time = simTime().dbl();
            timing.decision_type = "LOCAL";
            timing.processor_id = "VEHICLE_" + std::to_string(getParentModule()->getIndex());
            taskTimings[task->task_id] = timing;
        }
        
        // Check if can accept task locally
        if (!canAcceptTask(task)) {
            EV_WARN << "Cannot accept task locally (queue full) - rejecting" << endl;
            task->state = REJECTED;
            task->failure_reason = "LOCAL_QUEUE_FULL";
            tasks_rejected++;
            sendTaskFailureToRSU(task, "LOCAL_QUEUE_FULL");
            cleanupTaskEvents(task);
            delete task;
            return;
        }
        
        // Try to start immediately or queue
        if (canStartProcessing(task)) {
            EV_INFO << "✓ Starting task immediately" << endl;
            allocateResourcesAndStart(task);
        } else {
            EV_INFO << "⏸ Queuing task for local execution" << endl;
            task->state = QUEUED;
            markTaskQueued(task);
            pending_tasks.push(task);
            sendTaskOffloadingEvent(task->task_id, "QUEUED",
                "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "SELF",
                "{\"reason\":\"RSU_LOCAL_RESOURCES_BUSY\"}");
            logQueueState("Task queued after LOCAL decision");
        }
        
        logResourceState("After LOCAL execution decision");
    }
    
    // ========================================================================
    // DECISION: OFFLOAD TO RSU
    // ========================================================================
    else if (decisionType == "RSU") {
                sendTaskOffloadingEvent(task->task_id, "DECISION_OFFLOAD",
                    "RSU", "VEHICLE_" + std::to_string(getParentModule()->getIndex()),
                    "{\"target\":\"RSU\"}");

        EV_INFO << "🏢 Decision: OFFLOAD TO RSU" << endl;
        std::cout << "OFFLOAD_EXEC: Task " << task->task_id << " offloading to RSU" << std::endl;
        
        // Track processor for completion report
        if (taskTimings.find(task->task_id) != taskTimings.end()) {
            taskTimings[task->task_id].processor_id = "RSU";
        }

        // Phase 1 criteria: upload must go to the RSU that made the decision.
        // Do not re-run best-RSU selection during task upload.
        LAddress::L2Type decisionRsuMac = decision->getSenderAddress();
        LAddress::L2Type remoteProcessorMac = decision->getRedirect_target_rsu_mac();
        sendTaskToRSU(task, decisionRsuMac, remoteProcessorMac);
        
        // Track offloaded task
        offloadedTasks[task->task_id] = task;
        offloadedTaskTargets[task->task_id] = "RSU";
        
        // Schedule timeout for result
        cMessage* timeoutMsg = new cMessage("offloadedTaskTimeout");
        timeoutMsg->setContextPointer(task);
        scheduleAt(simTime() + offloadedTaskTimeout, timeoutMsg);
        offloadedTaskTimeouts[task->task_id] = timeoutMsg;

        EV_INFO << "✓ Task offloaded to RSU, awaiting result" << endl;
    }

    // ========================================================================
    // DECISION: OFFLOAD TO SERVICE VEHICLE
    // ========================================================================
    else if (decisionType == "SERVICE_VEHICLE") {
        EV_INFO << "🚗 Decision: OFFLOAD TO SERVICE VEHICLE" << endl;
        
        std::string serviceVehicleId = canonicalServiceVehicleId(decision->getTarget_service_vehicle_id());
        LAddress::L2Type serviceVehicleMac = decision->getTarget_service_vehicle_mac();
        LAddress::L2Type anchorRsuMac = decision->getRedirect_target_rsu_mac();
        LAddress::L2Type ingressRsuMac = decision->getSenderAddress();

        // Guardrail: keep SV as the preferred execution target when RSU already
        // selected it, and only fallback on clear high-risk deadline misses.
        // NOTE: this vehicle cannot observe the target SV queue/CPU directly;
        // using local service queue here causes false SV->RSU fallback.
        const double elapsed_s = std::max(0.0, (simTime() - task->created_time).dbl());
        const double remaining_deadline_s = std::max(0.001, task->relative_deadline - elapsed_s);

        // RSU predicts the chosen SV path using global DT state and candidate scoring.
        const double rsu_pred_sv_s = std::max(0.0, decision->getEstimated_completion_time());

        // Keep a lightweight local proxy only as a corroboration signal when RSU
        // prediction is unavailable. This proxy avoids target-SV queue assumptions.
        // Use nominal shared service capacity (not this vehicle's active service queue)
        // to keep a conservative but stable execution-time estimate.
        const double est_uplink_s = (static_cast<double>(task->input_size_bytes) * 8.0) / 6e6;
        const double est_downlink_s = (static_cast<double>(task->output_size_bytes) * 8.0) / 6e6;
        const double nominal_service_hz = std::max(1.0, cpu_total * std::max(0.25, serviceCpuReservation));
        const double est_exec_proxy_s = static_cast<double>(task->cpu_cycles) / nominal_service_hz;
        const double est_total_proxy_sv_s = est_uplink_s + est_exec_proxy_s + est_downlink_s + 0.02;

        // Promote executed SV share by requiring stronger evidence before fallback.
        // Higher QoS still applies tighter deadlines than medium/low QoS.
        double risk_threshold = 1.0;
        if (task->qos_value >= 0.8) {
            risk_threshold = 1.18;
        } else if (task->qos_value >= 0.6) {
            risk_threshold = 1.40;
        } else {
            risk_threshold = 1.75;
        }

        // High-confidence SV selections get slightly more slack before demotion.
        if (decision->getConfidence_score() >= 0.85) {
            risk_threshold += 0.12;
        }

        const bool rsu_high_risk = (rsu_pred_sv_s > 0.0)
            ? (rsu_pred_sv_s > (remaining_deadline_s * risk_threshold))
            : false;
        const bool proxy_high_risk = est_total_proxy_sv_s > (remaining_deadline_s * risk_threshold * 0.95);

        bool shouldFallbackToRsu = false;
        if (rsu_pred_sv_s > 0.0) {
            // Only fallback when RSU says SV is high-risk and local proxy does not disagree.
            shouldFallbackToRsu = rsu_high_risk && proxy_high_risk;
        } else {
            // No RSU prediction available: use local proxy with strict high-risk test only.
            shouldFallbackToRsu = est_total_proxy_sv_s > (remaining_deadline_s * (risk_threshold + 0.25));
        }

        if (shouldFallbackToRsu) {
            EV_WARN << "SV infeasible for task " << task->task_id
                    << ": proxy_est_total=" << est_total_proxy_sv_s
                    << "s, rsu_pred=" << rsu_pred_sv_s
                    << "s, remaining_deadline=" << remaining_deadline_s
                    << "s. Falling back to RSU." << endl;

            sendTaskOffloadingEvent(task->task_id, "DECISION_OFFLOAD_FALLBACK",
                "RSU", "VEHICLE_" + std::to_string(getParentModule()->getIndex()),
                "{\"from\":\"SERVICE_VEHICLE\",\"to\":\"RSU\","
                "\"est_sv_total_s\":" + std::to_string(est_total_proxy_sv_s) + ","
                "\"rsu_pred_sv_s\":" + std::to_string(rsu_pred_sv_s) + ","
                "\"remaining_deadline_s\":" + std::to_string(remaining_deadline_s) + "}");

            std::cout << "OFFLOAD_FALLBACK: Task " << task->task_id
                      << " SV->RSU (proxy_est_sv=" << est_total_proxy_sv_s
                      << "s, rsu_pred=" << rsu_pred_sv_s
                      << "s, remaining=" << remaining_deadline_s << "s)" << std::endl;

            if (taskTimings.find(task->task_id) != taskTimings.end()) {
                taskTimings[task->task_id].decision_type = "RSU";
                taskTimings[task->task_id].processor_id = "RSU";
            }

            sendTaskToRSU(task, ingressRsuMac, anchorRsuMac);

            offloadedTasks[task->task_id] = task;
            offloadedTaskTargets[task->task_id] = "RSU";

            cMessage* timeoutMsg = new cMessage("offloadedTaskTimeout");
            timeoutMsg->setContextPointer(task);
            scheduleAt(simTime() + offloadedTaskTimeout, timeoutMsg);
            offloadedTaskTimeouts[task->task_id] = timeoutMsg;

            EV_INFO << "✓ Infeasible SV offload redirected to RSU, awaiting result" << endl;
            return;
        }

        sendTaskOffloadingEvent(task->task_id, "DECISION_OFFLOAD",
            "RSU", "VEHICLE_" + std::to_string(getParentModule()->getIndex()),
            "{\"target\":\"SERVICE_VEHICLE\",\"sv_id\":\"" + serviceVehicleId + "\"}");
        
        std::cout << "OFFLOAD_EXEC: Task " << task->task_id 
                  << " offloading to service vehicle " << serviceVehicleId << std::endl;
        
        // Track processor for completion report
        if (taskTimings.find(task->task_id) != taskTimings.end()) {
            taskTimings[task->task_id].processor_id = serviceVehicleId;
        }
        
        if (serviceVehicleMac == 0) {
            EV_ERROR << "Invalid service vehicle MAC address" << endl;
            // Fallback to local execution
            EV_INFO << "Falling back to LOCAL execution" << endl;
            if (canAcceptTask(task)) {
                if (canStartProcessing(task)) {
                    allocateResourcesAndStart(task);
                } else {
                    task->state = QUEUED;
                    markTaskQueued(task);
                    pending_tasks.push(task);
                    sendTaskOffloadingEvent(task->task_id, "QUEUED",
                        "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "SELF",
                        "{\"reason\":\"SV_INVALID_FALLBACK\"}");
                }
            } else {
                task->state = REJECTED;
                task->failure_reason = "SERVICE_VEHICLE_INVALID";
                tasks_rejected++;
                sendTaskFailureToRSU(task, "SERVICE_VEHICLE_INVALID");
                cleanupTaskEvents(task);
                delete task;
            }
            return;
        }
        
        if (anchorRsuMac != 0) {
            sendTaskToServiceVehicleViaRSU(task, serviceVehicleId, serviceVehicleMac, ingressRsuMac, anchorRsuMac);
        } else {
            sendTaskToServiceVehicle(task, serviceVehicleId, serviceVehicleMac);
        }
        
        // Track offloaded task
        offloadedTasks[task->task_id] = task;
        offloadedTaskTargets[task->task_id] = serviceVehicleId;
        
        // Schedule timeout for result
        cMessage* timeoutMsg = new cMessage("offloadedTaskTimeout");
        timeoutMsg->setContextPointer(task);
        scheduleAt(simTime() + offloadedTaskTimeout, timeoutMsg);
        offloadedTaskTimeouts[task->task_id] = timeoutMsg;

        EV_INFO << "✓ Task offloaded to service vehicle, awaiting result" << endl;
    }

    // ========================================================================
    // DECISION: REDIRECT TO ANOTHER RSU
    // ========================================================================
    else if (decisionType == "REDIRECT") {
        EV_INFO << "🔄 Decision: REDIRECT TO ANOTHER RSU" << endl;
        std::cout << "OFFLOAD_REDIRECT: Task " << task->task_id << " redirected by RSU" << std::endl;
        
        // Extract redirect information from decision message
        LAddress::L2Type redirectTargetMac = decision->getRedirect_target_rsu_mac();
        int nextCandidateIndex = decision->getNext_candidate_index();
        std::string redirectTargetId = decision->getRedirect_target_rsu_id();
        
        // Handle redirect attempt
        auto it = task_redirect_counts.find(task->task_id);
        int redirectAttempts = (it == task_redirect_counts.end()) ? 0 : it->second;
        
        EV_INFO << "RSU redirects to target: " << redirectTargetId 
                << " (MAC: " << redirectTargetMac << "), attempt " 
                << (redirectAttempts + 1) << "/" << max_redirect_hops << endl;
        
        // Check if we can still retry
        if (redirectAttempts >= max_redirect_hops) {
            EV_WARN << "Max redirect hops (" << max_redirect_hops << ") exceeded for task " 
                    << task->task_id << " - falling back to LOCAL" << endl;
            std::cout << "OFFLOAD_REDIRECT_EXHAUSTED: Task " << task->task_id 
                      << " exceeds max redirects, executing locally" << std::endl;
            
            // Fallback to local execution
            if (canAcceptTask(task)) {
                if (canStartProcessing(task)) {
                    allocateResourcesAndStart(task);
                } else {
                    task->state = QUEUED;
                    markTaskQueued(task);
                    pending_tasks.push(task);
                    sendTaskOffloadingEvent(task->task_id, "QUEUED",
                        "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "SELF",
                        "{\"reason\":\"REDIRECT_EXHAUSTED_FALLBACK\"}");
                }
                
                // Track fallback
                if (taskTimings.find(task->task_id) != taskTimings.end()) {
                    taskTimings[task->task_id].processor_id = "VEHICLE_FALLBACK_" + std::to_string(getParentModule()->getIndex());
                    taskTimings[task->task_id].decision_type = "LOCAL_FALLBACK";
                }
            } else {
                // Queue full - reject
                EV_WARN << "Cannot accept task locally (queue full) - rejecting" << endl;
                task->state = REJECTED;
                task->failure_reason = "REDIRECT_EXHAUSTED_QUEUE_FULL";
                tasks_rejected++;
                sendTaskFailureToRSU(task, "REDIRECT_EXHAUSTED_QUEUE_FULL");
                cleanupTaskEvents(task);
                delete task;
            }
        }
        else if (redirectTargetMac == 0 || redirectTargetMac == myId) {
            EV_ERROR << "Invalid redirect target MAC or self-reference - rejecting" << endl;
            task->state = REJECTED;
            task->failure_reason = "INVALID_REDIRECT_TARGET";
            tasks_rejected++;
            sendTaskFailureToRSU(task, "INVALID_REDIRECT_TARGET");
            cleanupTaskEvents(task);
            delete task;
        }
        else {
            // Valid redirect - resend request to next RSU
            task_redirect_counts[task->task_id]++;
            
            EV_INFO << "Sending redirect request to " << redirectTargetId 
                    << " (MAC: " << redirectTargetMac << ")" << endl;
            
            // Create new offloading request with updated candidate index
            veins::OffloadingRequestMessage* redirectRequest = new veins::OffloadingRequestMessage();
            redirectRequest->setTask_id(task->task_id.c_str());
            redirectRequest->setVehicle_id(task->vehicle_id.c_str());
            redirectRequest->setRequest_time(simTime().dbl());
            redirectRequest->setMem_footprint_bytes(task->input_size_bytes);
            redirectRequest->setCpu_cycles(task->cpu_cycles);
            redirectRequest->setDeadline_seconds(std::max(0.001, task->relative_deadline - (simTime() - task->created_time).dbl()));
            redirectRequest->setQos_value(task->qos_value);
            redirectRequest->setLocal_cpu_available_ghz(calculateReportedCpuAvailable() / 1e9);
            redirectRequest->setLocal_cpu_utilization(calculateTaskCpuUtilization());
            redirectRequest->setLocal_mem_available_mb(memory_available / 1e6);
            redirectRequest->setLocal_queue_length(pending_tasks.size());
            redirectRequest->setLocal_processing_count(processing_tasks.size());
            Coord redirectPos = mobility->getPositionAt(simTime());
            redirectRequest->setPos_x(redirectPos.x);
            redirectRequest->setPos_y(redirectPos.y);
            redirectRequest->setSpeed(mobility->getSpeed());
            redirectRequest->setLocal_decision("OFFLOAD_TO_RSU");
            redirectRequest->setSenderAddress(myId);
            
            // Copy the candidate list from the original decision message
            // Note: The decision message should have the same candidate MACs
            // Get candidates for this task from the tracking data
            auto candidateIt = taskCandidates.find(task->task_id);
            if (candidateIt != taskCandidates.end()) {
                const auto& candidates = candidateIt->second;
                
                // Set candidate array
                redirectRequest->setCandidate_rsu_macsArraySize(candidates.size());
                for (size_t i = 0; i < candidates.size(); i++) {
                    redirectRequest->setCandidate_rsu_macs(i, candidates[i]);
                }
                
                // Update to point to the redirect target
                redirectRequest->setCurrent_candidate_index(nextCandidateIndex);
                redirectRequest->setMax_redirect_hops(max_redirect_hops);
                
                // Send to redirect target using standard WSM protocol
                populateWSM(redirectRequest, redirectTargetMac);
                sendDown(redirectRequest);
                
                EV_INFO << "✓ Redirect request sent to RSU candidate index " << nextCandidateIndex << endl;
                std::cout << "INFO: Redirect request sent to " << redirectTargetId 
                          << " (redirect attempt " << task_redirect_counts[task->task_id] << ")" << std::endl;
            }
            else {
                EV_ERROR << "Cannot find candidate list for task " << task->task_id << endl;
                task->state = REJECTED;
                task->failure_reason = "REDIRECT_CANDIDATE_LIST_NOT_FOUND";
                tasks_rejected++;
                sendTaskFailureToRSU(task, "REDIRECT_CANDIDATE_LIST_NOT_FOUND");
                cleanupTaskEvents(task);
                delete task;
            }
        }
    }
    
    // ========================================================================
    // DECISION: REJECT TASK
    // ========================================================================
    else if (decisionType == "REJECT") {
        EV_INFO << "❌ Decision: REJECT TASK" << endl;
        std::cout << "OFFLOAD_EXEC: Task " << task->task_id << " REJECTED by RSU" << std::endl;

        std::string reason = decision->getDecision_reason();
        
        task->state = REJECTED;
        task->failure_reason = std::string("RSU_REJECTED: ") + reason;
        tasks_rejected++;

        sendTaskFailureToRSU(task, "RSU_REJECTED: " + reason);
        
        EV_INFO << "Task rejected - reason: " << reason << endl;
        cleanupTaskEvents(task);
        delete task;
    }
    
    // ========================================================================
    // UNKNOWN DECISION
    // ========================================================================
    else {
        EV_ERROR << "Unknown decision type: " << decisionType << endl;
        std::cout << "OFFLOAD_EXEC: ERROR - Unknown decision type for task " 
                  << task->task_id << std::endl;
        
        // Fallback to local execution if possible
        if (canAcceptTask(task)) {
            EV_INFO << "Falling back to local execution" << endl;
            if (canStartProcessing(task)) {
                allocateResourcesAndStart(task);
            } else {
                task->state = QUEUED;
                markTaskQueued(task);
                pending_tasks.push(task);
            }
        } else {
            task->state = REJECTED;
            task->failure_reason = "UNKNOWN_DECISION";
            tasks_rejected++;
            sendTaskFailureToRSU(task, "UNKNOWN_DECISION");
            cleanupTaskEvents(task);
            delete task;
        }
    }
}

// ============================================================================
// TASK EXECUTION METHODS
// ============================================================================

void PayloadVehicleApp::sendTaskToRSU(Task* task) {
    sendTaskToRSU(task, 0, 0);
}

void PayloadVehicleApp::sendTaskToRSU(Task* task, LAddress::L2Type targetRsuMac) {
    sendTaskToRSU(task, targetRsuMac, 0);
}

void PayloadVehicleApp::sendTaskToRSU(Task* task, LAddress::L2Type ingressRsuMac, LAddress::L2Type processorRsuMac) {
    EV_INFO << "📤 Sending task " << task->task_id << " to RSU for processing" << endl;
    std::cout << "OFFLOAD_TO_RSU: Task " << task->task_id << " sent to RSU" << std::endl;
    
    // Create TaskOffloadPacket
    veins::TaskOffloadPacket* packet = new veins::TaskOffloadPacket();
    packet->setTask_id(task->task_id.c_str());
    packet->setOrigin_vehicle_id(task->vehicle_id.c_str());
    packet->setOrigin_vehicle_mac(myId);  // Set our MAC address for return routing
    packet->setOffload_time(simTime().dbl());
    packet->setMem_footprint_bytes(task->mem_footprint_bytes);
    packet->setCpu_cycles(task->cpu_cycles);
    packet->setDeadline_seconds(std::max(0.001, task->relative_deadline - (simTime() - task->created_time).dbl()));
    packet->setQos_value(task->qos_value);
    std::string routeHint = buildRouteHint(ingressRsuMac, processorRsuMac);
    packet->setTask_input_data(routeHint.c_str());
    setOffloadPacketSize(packet, task->input_size_bytes > 0 ? task->input_size_bytes : task->mem_footprint_bytes);
    
    // Send to RSU. If caller provided a target RSU (decision-origin), use it.
    // Otherwise keep legacy behavior and choose current best RSU.
    LAddress::L2Type rsuMac = (ingressRsuMac != 0) ? ingressRsuMac : selectBestRSU();
    if (rsuMac != 0) {
        populateWSM(packet, rsuMac);
        sendDown(packet);
        uint32_t tx_bytes = static_cast<uint32_t>(task->input_size_bytes > 0
            ? task->input_size_bytes
            : task->mem_footprint_bytes);
        double tx_energy_j = energyCalculator.calcTransmissionEnergy(tx_bytes, 6e6);
        applyTaskEnergyDrain(tx_energy_j, "OFFLOAD_TX_RSU");
        EV_INFO << "✓ Task offload packet sent to RSU MAC: " << rsuMac << endl;
        std::cout << "OFFLOAD_TARGET_RSU_MAC: " << rsuMac << std::endl;
        if (processorRsuMac != 0 && processorRsuMac != rsuMac) {
            std::cout << "OFFLOAD_REMOTE_PROCESSOR_RSU_MAC: " << processorRsuMac << std::endl;
        }
    } else {
        EV_ERROR << "No RSU available to send task" << endl;
        delete packet;
        return;
    }
    
    offloadedTasks[task->task_id] = task;
    offloadedTaskTargets[task->task_id] = "RSU";
    sendTaskOffloadingEvent(task->task_id, "TASK_OFFLOADING",
        "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "RSU",
        "{\"target\":\"RSU\",\"mem_bytes\":" + std::to_string(task->mem_footprint_bytes) + ","
        "\"cpu_cycles\":" + std::to_string(task->cpu_cycles) + "}");
}

void PayloadVehicleApp::sendTaskToServiceVehicle(Task* task, const std::string& serviceVehicleId, veins::LAddress::L2Type serviceMac) {
    EV_INFO << "📤 Sending task " << task->task_id << " to service vehicle " << serviceVehicleId << endl;
    std::cout << "OFFLOAD_TO_SV: Task " << task->task_id << " sent to service vehicle " 
              << serviceVehicleId << std::endl;
    
    // Create TaskOffloadPacket
    veins::TaskOffloadPacket* packet = new veins::TaskOffloadPacket();
    packet->setTask_id(task->task_id.c_str());
    packet->setOrigin_vehicle_id(task->vehicle_id.c_str());
    packet->setOrigin_vehicle_mac(myId);  // Set our MAC address for return routing
    packet->setOffload_time(simTime().dbl());
    packet->setMem_footprint_bytes(task->mem_footprint_bytes);
    packet->setCpu_cycles(task->cpu_cycles);
    packet->setDeadline_seconds(std::max(0.001, task->relative_deadline - (simTime() - task->created_time).dbl()));
    packet->setQos_value(task->qos_value);
    packet->setTask_input_data("{\"input\":\"task_data\"}");  // Placeholder
    setOffloadPacketSize(packet, task->input_size_bytes > 0 ? task->input_size_bytes : task->mem_footprint_bytes);
    
    // Send to service vehicle
    populateWSM(packet, serviceMac);
    sendDown(packet);
    uint32_t tx_bytes = static_cast<uint32_t>(task->input_size_bytes > 0
        ? task->input_size_bytes
        : task->mem_footprint_bytes);
    double tx_energy_j = energyCalculator.calcTransmissionEnergy(tx_bytes, 6e6);
    applyTaskEnergyDrain(tx_energy_j, "OFFLOAD_TX_SERVICE_VEHICLE");
    
    EV_INFO << "✓ Task offload packet sent to service vehicle MAC: " << serviceMac << endl;
    
    offloadedTasks[task->task_id] = task;
    offloadedTaskTargets[task->task_id] = serviceVehicleId;
    sendTaskOffloadingEvent(task->task_id, "TASK_OFFLOADING",
        "VEHICLE_" + std::to_string(getParentModule()->getIndex()), serviceVehicleId,
        "{\"target\":\"SERVICE_VEHICLE\",\"sv_id\":\"" + serviceVehicleId + "\","
        "\"mem_bytes\":" + std::to_string(task->mem_footprint_bytes) + "}");
}

void PayloadVehicleApp::sendTaskToServiceVehicleViaRSU(Task* task, const std::string& serviceVehicleId,
                                                        veins::LAddress::L2Type serviceMac,
                                                        veins::LAddress::L2Type ingressRsuMac,
                                                        veins::LAddress::L2Type anchorRsuMac) {
    EV_INFO << "📤 Sending service task " << task->task_id << " via RSU path" << endl;

    veins::TaskOffloadPacket* packet = new veins::TaskOffloadPacket();
    packet->setTask_id(task->task_id.c_str());
    packet->setOrigin_vehicle_id(task->vehicle_id.c_str());
    packet->setOrigin_vehicle_mac(myId);
    packet->setOffload_time(simTime().dbl());
    packet->setMem_footprint_bytes(task->mem_footprint_bytes);
    packet->setCpu_cycles(task->cpu_cycles);
    packet->setDeadline_seconds(std::max(0.001, task->relative_deadline - (simTime() - task->created_time).dbl()));
    packet->setQos_value(task->qos_value);
    std::string hint = buildServiceRouteHint(serviceMac, anchorRsuMac, myId);
    packet->setTask_input_data(hint.c_str());
    setOffloadPacketSize(packet, task->input_size_bytes > 0 ? task->input_size_bytes : task->mem_footprint_bytes);

    LAddress::L2Type firstHopRsu = (ingressRsuMac != 0) ? ingressRsuMac : selectBestRSU();
    if (firstHopRsu == 0) {
        EV_WARN << "No RSU available for service-task infrastructure route, falling back to direct V2V" << endl;
        delete packet;
        sendTaskToServiceVehicle(task, serviceVehicleId, serviceMac);
        return;
    }

    populateWSM(packet, firstHopRsu);
    sendDown(packet);
    uint32_t tx_bytes = static_cast<uint32_t>(task->input_size_bytes > 0
        ? task->input_size_bytes
        : task->mem_footprint_bytes);
    double tx_energy_j = energyCalculator.calcTransmissionEnergy(tx_bytes, 6e6);
    applyTaskEnergyDrain(tx_energy_j, "OFFLOAD_TX_SERVICE_VEHICLE_VIA_RSU");

    offloadedTasks[task->task_id] = task;
    offloadedTaskTargets[task->task_id] = serviceVehicleId;
    sendTaskOffloadingEvent(task->task_id, "TASK_OFFLOADING",
        "VEHICLE_" + std::to_string(getParentModule()->getIndex()), "RSU",
        "{\"target\":\"SERVICE_VEHICLE_VIA_RSU\",\"sv_id\":\"" + serviceVehicleId + "\","
        "\"ingress_rsu_mac\":" + std::to_string(firstHopRsu) + ","
        "\"anchor_rsu_mac\":" + std::to_string(anchorRsuMac) + "}");
}

bool PayloadVehicleApp::lookupVehiclePositionById(const std::string& vehicleId, Coord& outPos) {
    cModule* networkModule = getModuleByPath("^.^");
    if (!networkModule) return false;

    int idx = -1;
    try {
        idx = std::stoi(vehicleId);
    } catch (...) {
        return false;
    }
    if (idx < 0) return false;

    cModule* node = networkModule->getSubmodule("node", idx);
    if (!node) return false;

    TraCIMobility* targetMobility = FindModule<TraCIMobility*>::findSubModule(node);
    if (targetMobility) {
        outPos = targetMobility->getPositionAt(simTime());
        return true;
    }

    cModule* mob = node->getSubmodule("mobility");
    if (!mob) return false;
    outPos.x = mob->par("x").doubleValue();
    outPos.y = mob->par("y").doubleValue();
    outPos.z = mob->hasPar("z") ? mob->par("z").doubleValue() : 0.0;
    return true;
}

veins::LAddress::L2Type PayloadVehicleApp::selectBestRSUForPosition(const Coord& position, int* outRsuIndex) {
    cModule* networkModule = getModuleByPath("^.^");
    if (!networkModule) return 0;

    double bestRssi = -1e9;
    LAddress::L2Type bestMac = 0;
    int bestIdx = -1;

    for (int i = 0; i < 3; i++) {
        cModule* rsu = networkModule->getSubmodule("rsu", i);
        if (!rsu) continue;
        cModule* rmob = rsu->getSubmodule("mobility");
        if (!rmob) continue;

        Coord rsuPos(rmob->par("x").doubleValue(), rmob->par("y").doubleValue(),
                     rmob->hasPar("z") ? rmob->par("z").doubleValue() : 0.0);
        double rssi = estimateV2vRssiDbm(position, rsuPos);
        if (rssi > bestRssi) {
            bestRssi = rssi;
            bestIdx = i;
            DemoBaseApplLayerToMac1609_4Interface* rsuMacInterface =
                FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(rsu);
            bestMac = rsuMacInterface ? rsuMacInterface->getMACAddress() : 0;
        }
    }

    if (outRsuIndex) *outRsuIndex = bestIdx;
    return bestMac;
}

double PayloadVehicleApp::estimateV2vRssiDbm(const Coord& a, const Coord& b) {
    double d = std::max(1.0, a.distance(b));
    return -40.0 - 20.0 * std::log10(d);
}

void PayloadVehicleApp::handleTaskResult(veins::TaskResultMessage* msg) {
    std::string task_id = msg->getTask_id();
    EV_INFO << "📥 Received task result for " << task_id << endl;
    std::cout << "TASK_RESULT: Received result for task " << task_id 
              << " from " << msg->getProcessor_id() << std::endl;
    
    // Process result and send completion report to RSU
    auto it = offloadedTasks.find(task_id);
    if (it != offloadedTasks.end()) {
        Task* task = it->second;
        bool success = msg->getSuccess();
        double completion_time = msg->getCompletion_time();
        double receive_time = simTime().dbl();
        bool on_time = completion_time <= task->deadline.dbl();

        double decision_latency = 0.0;
        double total_offload_latency = 0.0;
        if (taskTimings.find(task_id) != taskTimings.end()) {
            const TaskTimingInfo& timing = taskTimings[task_id];
            decision_latency = std::max(0.0, timing.decision_time - timing.request_time);
            total_offload_latency = std::max(0.0, receive_time - timing.request_time);
        }
        double downlink_latency = std::max(0.0, receive_time - completion_time);
        double estimated_uplink_and_queue = std::max(0.0,
            total_offload_latency - msg->getProcessing_time() - downlink_latency);

        sendTaskOffloadingEvent(task_id, "OFFLOAD_TIMING_MEASURED",
            msg->getProcessor_id(), "VEHICLE_" + std::to_string(getParentModule()->getIndex()),
            "{\"decision_latency_s\":" + std::to_string(decision_latency) +
            ",\"uplink_plus_queue_s\":" + std::to_string(estimated_uplink_and_queue) +
            ",\"processing_s\":" + std::to_string(msg->getProcessing_time()) +
            ",\"downlink_s\":" + std::to_string(downlink_latency) +
            ",\"total_offload_s\":" + std::to_string(total_offload_latency) + "}");

        std::cout << "OFFLOAD_TIMING: task=" << task_id
                  << " decision=" << decision_latency
                  << " uplink+queue=" << estimated_uplink_and_queue
                  << " processing=" << msg->getProcessing_time()
                  << " downlink=" << downlink_latency
                  << " total=" << total_offload_latency << std::endl;
        sendTaskOffloadingEvent(task_id, "TASK_RESULTS_RECEIVED",
            msg->getProcessor_id(), "VEHICLE_" + std::to_string(getParentModule()->getIndex()),
            "{\"success\":" + std::string(success ? "true" : "false") + ","
            "\"on_time\":" + std::string(on_time ? "true" : "false") + "}");
        sendTaskOffloadingEvent(task_id, "PROCESSING_COMPLETED",
            msg->getProcessor_id(), "VEHICLE_" + std::to_string(getParentModule()->getIndex()),
            "{\"success\":" + std::string(success ? "true" : "false") + "}");
        sendTaskOffloadingEvent(task_id, (success && on_time) ? "COMPLETE_ON_TIME" : "FAILED",
            msg->getProcessor_id(), "VEHICLE_" + std::to_string(getParentModule()->getIndex()),
            "{\"success\":" + std::string(success ? "true" : "false") + ","
            "\"on_time\":" + std::string(on_time ? "true" : "false") + "}");
        
        // Send completion report with timing data
        sendTaskCompletionToRSU(task_id, completion_time, 
                                success, on_time, 
                                task->mem_footprint_bytes, task->cpu_cycles, 
                                task->qos_value, msg->getTask_output_data());
        
        if (on_time) {
            tasks_completed_on_time++;
        } else {
            tasks_completed_late++;
        }

        if (task->is_profile_task) {
            double e2e_latency = completion_time - task->created_time.dbl();
            double energy_joules = energyCalculator.calcOffloadEnergy(
                static_cast<uint32_t>(task->input_size_bytes),
                6e6,
                task->cpu_cycles,
                msg->getProcessing_time()
            );

            if (success) {
                MetricsManager::getInstance().recordTaskCompletion(
                    task->type,
                    MetricsManager::RSU_OFFLOAD,
                    msg->getProcessing_time(),
                    e2e_latency,
                    energy_joules,
                    task->deadline.dbl(),
                    completion_time
                );
            } else {
                MetricsManager::getInstance().recordTaskFailed(task->type, e2e_latency);
            }
        }
        
        // Cancel the offloadedTaskTimeout so it doesn't fire on freed memory
        auto tmIt = offloadedTaskTimeouts.find(task_id);
        if (tmIt != offloadedTaskTimeouts.end()) {
            if (tmIt->second->isScheduled()) cancelAndDelete(tmIt->second);
            else delete tmIt->second;
            offloadedTaskTimeouts.erase(tmIt);
        }

        // Clean up
        cleanupTaskEvents(task);
        delete task;
        offloadedTasks.erase(it);
        offloadedTaskTargets.erase(task_id);
    }

    delete msg;
}

// ============================================================================
// SERVICE VEHICLE METHODS
// ============================================================================

void PayloadVehicleApp::handleServiceTaskRequest(veins::TaskOffloadPacket* msg) {
    if (!serviceVehicleEnabled) {
        EV_WARN << "⚠️ Service vehicle mode disabled, rejecting task request" << endl;
        delete msg;
        return;
    }
    
    std::string task_id = msg->getTask_id();
    std::string origin_vehicle_id = msg->getOrigin_vehicle_id();
    veins::LAddress::L2Type origin_mac = msg->getOrigin_vehicle_mac();
    
    EV_INFO << "📥 SERVICE VEHICLE: Received task request from vehicle " << origin_vehicle_id << endl;
    std::cout << "SERVICE_REQUEST: Vehicle " << getParentModule()->getIndex() 
              << " received task " << task_id << " from vehicle " << origin_vehicle_id << std::endl;
    
    // Helper: send a FAILED result back to the dispatch RSU for agent sub-tasks so
    // the RSU can write the Redis result instead of leaving the DRL waiting forever.
    // For agent sub-tasks (task_id contains "::"), send a FAILED result via
    // the normal SV→RSU relay path so TV's RSU can write metrics to Redis.
    auto sendAgentRejection = [&](const std::string& reason) {
        if (task_id.find("::") != std::string::npos) {
            veins::TaskResultMessage* rej = new veins::TaskResultMessage();
            rej->setTask_id(task_id.c_str());
            rej->setOrigin_vehicle_id(origin_vehicle_id.c_str());
            rej->setProcessor_id(std::to_string(getParentModule()->getIndex()).c_str());
            rej->setSuccess(false);
            rej->setCompletion_time(simTime().dbl());
            rej->setProcessing_time(0.0);
            rej->setFailure_reason(reason.c_str());

            // Route via SV's nearest RSU → TV's RSU (using relay hint)
            Coord myPos = mobility ? mobility->getPositionAt(simTime()) : Coord(0,0,0);
            int svRsuIdx = -1;
            LAddress::L2Type svRsuMac = selectBestRSUForPosition(myPos, &svRsuIdx);

            // Estimate TV's best RSU from origin_vehicle_id position
            Coord tvPos;
            bool tvPosKnown = lookupVehiclePositionById(origin_vehicle_id, tvPos);
            LAddress::L2Type tvRsuMac = tvPosKnown ? selectBestRSUForPosition(tvPos, nullptr) : 0;

            if (svRsuMac != 0) {
                std::string relayPayload = buildServiceResultRelayHint(tvRsuMac, "{\"status\":\"rejected\"}");
                rej->setTask_output_data(relayPayload.c_str());
                populateWSM(rej, svRsuMac);
                sendDown(rej);
            } else if (origin_mac != 0) {
                rej->setTask_output_data("{\"status\":\"rejected\"}");
                populateWSM(rej, origin_mac);
                sendDown(rej);
            } else {
                delete rej;
                return;
            }
            std::cout << "SERVICE_REJECT_NOTIFY: agent subtask " << task_id
                      << " rejection sent via relay (reason=" << reason << ")" << std::endl;
        }
    };

    auto sendServiceFailureToOrigin = [&](const std::string& reason) {
        TaskFailureMessage* failure = new TaskFailureMessage();
        failure->setTask_id(task_id.c_str());
        failure->setVehicle_id(origin_vehicle_id.c_str());
        failure->setFailure_time(simTime().dbl());
        failure->setFailure_reason(reason.c_str());
        failure->setWasted_time(0.0);

        if (origin_mac != 0) {
            populateWSM((BaseFrame1609_4*)failure, origin_mac);
            sendDown(failure);
        } else {
            delete failure;
        }
    };

    // Check service vehicle capacity
    int total_service_tasks = serviceTasks.size() + processingServiceTasks.size();
    if (total_service_tasks >= maxConcurrentServiceTasks) {
        EV_WARN << "Service vehicle at capacity (" << total_service_tasks << "/"
                << maxConcurrentServiceTasks << "), rejecting task" << endl;
        std::cout << "SERVICE_REJECT: Task " << task_id << " rejected - capacity full" << std::endl;
        sendTaskOffloadingEvent(task_id, "SV_TASK_REJECTED",
            "VEHICLE_" + std::to_string(getParentModule()->getIndex()),
            origin_vehicle_id, "{\"reason\":\"capacity_full\"}");
        sendAgentRejection("RSU_QUEUE_FULL");
        sendServiceFailureToOrigin("SERVICE_VEHICLE_QUEUE_FULL");
        delete msg;
        return;
    }

    // Calculate reserved resources for service processing
    double service_cpu_hz = cpu_total * serviceCpuReservation;
    double service_mem_bytes = serviceMemoryReservation * 1e6;  // MB to bytes

    // Check if we have sufficient reserved resources
    if (memory_available < msg->getMem_footprint_bytes()) {
        EV_WARN << "Insufficient memory for service task (need "
                << (msg->getMem_footprint_bytes()/1e6) << "MB, have "
                << (memory_available/1e6) << "MB)" << endl;
        std::cout << "SERVICE_REJECT: Task " << task_id << " rejected - insufficient memory" << std::endl;
        sendTaskOffloadingEvent(task_id, "SV_TASK_REJECTED",
            "VEHICLE_" + std::to_string(getParentModule()->getIndex()),
            origin_vehicle_id, "{\"reason\":\"insufficient_memory\"}");
        sendAgentRejection("RSU_QUEUE_FULL");
        sendServiceFailureToOrigin("SERVICE_VEHICLE_INSUFFICIENT_MEMORY");
        delete msg;
        return;
    }
    
    // Create Task object from packet
    std::string vehicle_id = std::to_string(getParentModule()->getIndex());
    Task* task = new Task(origin_vehicle_id, task_sequence_number++, 
                          msg->getMem_footprint_bytes(), msg->getCpu_cycles(),
                          msg->getDeadline_seconds(), msg->getQos_value());
    
    // Override task_id with original task_id from packet
    task->task_id = task_id;
    
    // Store origin information for result sending
    serviceTaskOriginVehicles[task_id] = origin_vehicle_id;
    serviceTaskOriginMACs[task_id] = origin_mac;

    // Lifecycle: SV received task from origin vehicle
    sendTaskOffloadingEvent(task_id, "SV_TASK_RECEIVED",
        "VEHICLE_" + std::to_string(getParentModule()->getIndex()),
        origin_vehicle_id,
        "{\"mem_bytes\":" + std::to_string(msg->getMem_footprint_bytes()) +
        ",\"cpu_cycles\":" + std::to_string(msg->getCpu_cycles()) +
        ",\"deadline_s\":" + std::to_string(msg->getDeadline_seconds()) + "}");
    
    EV_INFO << "  Task ID: " << task_id << endl;
    EV_INFO << "  Origin Vehicle: " << origin_vehicle_id << endl;
    EV_INFO << "  Task Size: " << (msg->getMem_footprint_bytes() / 1024.0) << " KB" << endl;
    EV_INFO << "  CPU Cycles: " << (msg->getCpu_cycles() / 1e9) << " G" << endl;
    EV_INFO << "  Deadline: " << msg->getDeadline_seconds() << " s" << endl;
    EV_INFO << "  Service Queue: " << serviceTasks.size() << ", Processing: " 
            << processingServiceTasks.size() << endl;
    
    // Queue task for service processing
    task->state = QUEUED;
    serviceTasks.push(task);
    
    std::cout << "SERVICE_QUEUED: Task " << task_id << " queued for service processing" << std::endl;
    
    // Try to start processing immediately if we have capacity
    if (processingServiceTasks.size() < maxConcurrentServiceTasks) {
        // Dequeue and start processing
        Task* nextTask = serviceTasks.front();
        serviceTasks.pop();
        processServiceTask(nextTask);
    }
    
    delete msg;
}

void PayloadVehicleApp::processServiceTask(Task* task) {
    EV_INFO << "⚙️ SERVICE VEHICLE: Starting service task " << task->task_id << endl;
    std::cout << "SERVICE_PROCESS: Vehicle " << getParentModule()->getIndex() 
              << " processing service task " << task->task_id << std::endl;

    // ======================================================================
    // PHYSICS: Service vehicle reserves serviceCpuReservation fraction of
    // total CPU for serving others' tasks. Concurrent service tasks share
    // that pool by QoS-derived priority weights (same 3-tier mapping as RSU).
    //
    //   cpu_task_i = (cpu_total * reservation) * weight_i / sum(weights)
    //   exec_time  = cpu_cycles / cpu_task_i
    //
    // Arrival of a new service task triggers reallocateServiceCPUResources(),
    // which burns down already-executed cycles and reschedules all in-flight
    // service tasks with updated weighted shares.
    // ======================================================================
    double total_service_hz = getServiceCpuPoolHz();
    int n_concurrent = static_cast<int>(processingServiceTasks.size()) + 1; // +1 for this task

    double existing_weight_sum = 0.0;
    for (Task* t : processingServiceTasks) {
        existing_weight_sum += getServiceTaskPriorityWeight(t->qos_value);
    }
    double this_weight = getServiceTaskPriorityWeight(task->qos_value);
    double total_weight = std::max(1e-9, existing_weight_sum + this_weight);
    double per_task_hz = total_service_hz * (this_weight / total_weight);
    double processing_time = static_cast<double>(task->cpu_cycles) / per_task_hz;

    EV_INFO << "  Service CPU pool: " << (total_service_hz/1e9) << " GHz across "
            << n_concurrent << " task(s), this weight=" << this_weight
            << ", alloc=" << (per_task_hz/1e9) << " GHz" << endl;
    EV_INFO << "  Task CPU Cycles: " << (task->cpu_cycles / 1e9) << " G" << endl;
    EV_INFO << "  Estimated Processing Time: " << processing_time << " s" << endl;
    EV_INFO << "  Deadline: " << task->relative_deadline << " s" << endl;

    // Update task state
    task->state = PROCESSING;
    task->processing_start_time = simTime();
    task->cpu_allocated = per_task_hz;
    processingServiceTasks.insert(task);

    // Allocate memory from reserved service pool
    if (task->mem_footprint_bytes <= memory_available) {
        memory_available -= task->mem_footprint_bytes;
        EV_INFO << "  Memory allocated: " << (task->mem_footprint_bytes / 1e6) << " MB" << endl;
    } else {
        EV_WARN << "  Insufficient memory, processing anyway with degraded performance" << endl;
    }

    // Schedule completion event
    cMessage* completionMsg = new cMessage("serviceTaskCompletion");
    completionMsg->setContextPointer(task);
    task->completion_event = completionMsg;
    scheduleAt(simTime() + processing_time, completionMsg);

    // Schedule deadline guard
    cMessage* deadlineMsg = new cMessage("serviceTaskDeadline");
    deadlineMsg->setContextPointer(task);
    task->deadline_event = deadlineMsg;
    scheduleAt(task->deadline <= simTime() ? simTime() : task->deadline, deadlineMsg);

    // Lifecycle: SV started processing
    {
        std::string sv_id = "VEHICLE_" + std::to_string(getParentModule()->getIndex());
        auto orig_it = serviceTaskOriginVehicles.find(task->task_id);
        std::string orig_id = (orig_it != serviceTaskOriginVehicles.end()) ? orig_it->second : "UNKNOWN";
        sendTaskOffloadingEvent(task->task_id, "SV_PROCESSING_STARTED", sv_id, orig_id,
            "{\"cpu_ghz\":" + std::to_string(per_task_hz / 1e9) +
            ",\"est_exec_s\":" + std::to_string(processing_time) +
            ",\"concurrent\":" + std::to_string(n_concurrent) +
            ",\"priority_weight\":" + std::to_string(this_weight) + "}");
    }

    // Slow down all other concurrent service tasks now that one more shares the pool
    reallocateServiceCPUResources();

    EV_INFO << "✓ Service task processing started, completion expected at "
            << (simTime() + processing_time).dbl() << endl;
}

// ============================================================================
// SERVICE VEHICLE CPU REALLOCATION
// Divides the reserved CPU pool by QoS-derived priority weights
// and reschedules each completion event accordingly, mirroring
// reallocateCPUResources() for own-vehicle tasks.
// ============================================================================
void PayloadVehicleApp::reallocateServiceCPUResources() {
    if (processingServiceTasks.empty()) return;

    double total_service_hz = getServiceCpuPoolHz();
    double total_weight = 0.0;
    for (Task* t : processingServiceTasks) {
        total_weight += getServiceTaskPriorityWeight(t->qos_value);
    }
    if (total_weight <= 0.0) {
        total_weight = static_cast<double>(processingServiceTasks.size());
    }

    for (Task* t : processingServiceTasks) {
        double task_weight = getServiceTaskPriorityWeight(t->qos_value);
        double per_task_hz = total_service_hz * (task_weight / total_weight);
        if (std::abs(per_task_hz - t->cpu_allocated) < 1e6) continue;  // no meaningful change

        // Derive remaining cycles from the currently scheduled completion time:
        //   remaining = old_cpu_hz * (arrival_time - now)
        // Using arrival-time is correct across any number of prior reallocations.
        double remaining;
        if (t->completion_event && t->completion_event->isScheduled()) {
            double time_left = std::max(0.0,
                t->completion_event->getArrivalTime().dbl() - simTime().dbl());
            remaining = t->cpu_allocated * time_left;
        } else {
            remaining = static_cast<double>(t->cpu_cycles);  // fallback
        }

        t->cpu_allocated = per_task_hz;

        if (t->completion_event && t->completion_event->isScheduled()) {
            cancelEvent(t->completion_event);
        }
        double new_exec = (per_task_hz > 0.0) ? remaining / per_task_hz : 0.0;
        if (t->completion_event) {
            scheduleAt(simTime() + new_exec, t->completion_event);
        }

        EV_INFO << "  SV realloc: task " << t->task_id
            << " (w=" << task_weight << ") → " << (per_task_hz/1e9) << " GHz, "
                << (remaining/1e9) << "G cycles left, completes in " << new_exec << "s" << endl;
    }
}

void PayloadVehicleApp::sendServiceTaskResult(Task* task, const std::string& originalVehicleId) {
    EV_INFO << "📤 SERVICE VEHICLE: Sending result for task " << task->task_id 
            << " back to vehicle " << originalVehicleId << endl;
    std::cout << "SERVICE_RESULT: Vehicle " << getParentModule()->getIndex() 
              << " returning task " << task->task_id 
              << " result to vehicle " << originalVehicleId << std::endl;
    
    // Create TaskResultMessage
    veins::TaskResultMessage* result = new veins::TaskResultMessage();
    result->setTask_id(task->task_id.c_str());
    result->setOrigin_vehicle_id(originalVehicleId.c_str());
    result->setProcessor_id(std::to_string(getParentModule()->getIndex()).c_str());
    result->setSuccess(task->state == COMPLETED_ON_TIME || task->state == COMPLETED_LATE);
    result->setCompletion_time(task->completion_time.dbl());
    result->setProcessing_time((task->completion_time - task->processing_start_time).dbl());
    setResultPacketSize(result, task->output_size_bytes);
    
    // Set result data (placeholder - in real system would include actual computation output)
    result->setTask_output_data("{\"status\":\"completed_by_service_vehicle\"}");
    
    // Set failure reason if task failed
    if (task->state != COMPLETED_ON_TIME && task->state != COMPLETED_LATE) {
        if (!task->failure_reason.empty()) {
            result->setFailure_reason(task->failure_reason.c_str());
        } else if (task->state == FAILED) {
            double deadline_limit = task->relative_deadline;
            double elapsed = (task->completion_time - task->processing_start_time).dbl();
            if (elapsed > deadline_limit) {
                result->setFailure_reason("SERVICE_VEHICLE_DEADLINE_MISSED");
            } else {
                result->setFailure_reason("SERVICE_VEHICLE_PROCESSING_FAILED");
            }
        } else if (task->state == REJECTED) {
            result->setFailure_reason("SERVICE_VEHICLE_REJECTED");
        } else {
            result->setFailure_reason("SERVICE_VEHICLE_PROCESSING_FAILED");
        }
    } else {
        result->setFailure_reason("");
    }
    
    // Get origin vehicle MAC address
    auto mac_it = serviceTaskOriginMACs.find(task->task_id);
    if (mac_it != serviceTaskOriginMACs.end()) {
        veins::LAddress::L2Type origin_mac = mac_it->second;

        Coord myPos = mobility ? mobility->getPositionAt(simTime()) : Coord(0, 0, 0);
        Coord tvPos;
        bool tvPosKnown = lookupVehiclePositionById(originalVehicleId, tvPos);
        bool directV2v = false;
        if (tvPosKnown) {
            double tvSvRssi = estimateV2vRssiDbm(myPos, tvPos);
            directV2v = (tvSvRssi >= serviceDirectRssiThresholdDbm);
        }

        // Agent sub-tasks ("::" in task_id) follow the same relay path as regular tasks.
        // origin_mac is the task vehicle's MAC — the relay routes via RSUs to TV's RSU,
        // which intercepts sub-task results and writes metrics to Redis.
        if (directV2v) {
            // Direct V2V — within range of origin vehicle
            EV_INFO << "  Sending result directly to origin MAC: " << origin_mac << endl;
            populateWSM(result, origin_mac);
            sendDown(result);
        } else {
            // Infrastructure relay path for regular tasks:
            // SV -> best RSU near SV -> best RSSI RSU near TV -> TV
            int svRsuIdx = -1;
            LAddress::L2Type svRsuMac = selectBestRSUForPosition(myPos, &svRsuIdx);
            int tvRsuIdx = -1;
            LAddress::L2Type tvBestRsuMac = tvPosKnown ? selectBestRSUForPosition(tvPos, &tvRsuIdx) : 0;

            if (svRsuMac != 0) {
                std::string relayPayload = buildServiceResultRelayHint(tvBestRsuMac, result->getTask_output_data());
                result->setTask_output_data(relayPayload.c_str());
                populateWSM(result, svRsuMac);
                sendDown(result);
                EV_INFO << "  Result sent via RSU relay path (SV->RSU->...->TV)" << endl;
            } else {
                // Last-resort fallback to direct MAC delivery
                EV_WARN << "  No RSU available for relay, fallback to direct MAC" << endl;
                populateWSM(result, origin_mac);
                sendDown(result);
            }
        }

        // Lifecycle: SV result sent
        {
            std::string sv_id = "VEHICLE_" + std::to_string(getParentModule()->getIndex());
            bool success = (task->state == COMPLETED_ON_TIME || task->state == COMPLETED_LATE);
            sendTaskOffloadingEvent(task->task_id, "SV_RESULT_SENT", sv_id, originalVehicleId,
                "{\"success\":" + std::string(success ? "true" : "false") +
                ",\"on_time\":" + std::string(task->state == COMPLETED_ON_TIME ? "true" : "false") + "}");
        }
        
        EV_INFO << "✓ Service task result sent successfully" << endl;
    } else {
        EV_ERROR << "Origin vehicle MAC not found for task " << task->task_id << endl;
        
        // Fallback: send via RSU
        LAddress::L2Type rsuMac = selectBestRSU();
        if (rsuMac != 0) {
            EV_INFO << "  Fallback: Sending result via RSU" << endl;
            populateWSM(result, rsuMac);
            sendDown(result);
        } else {
            EV_ERROR << "Cannot send result - no RSU available" << endl;
            delete result;
        }
    }
    
    // Clean up origin tracking
    serviceTaskOriginVehicles.erase(task->task_id);
    serviceTaskOriginMACs.erase(task->task_id);
}

// ============================================================================
// TASK OFFLOADING LIFECYCLE EVENT TRACKING
// ============================================================================

// Overload: with explicit details JSON
void PayloadVehicleApp::sendTaskOffloadingEvent(const std::string& taskId, const std::string& eventType,
                                                 const std::string& sourceEntity, const std::string& targetEntity,
                                                 const std::string& details) {
    EV_DEBUG << "📊 Lifecycle event: " << eventType << " for task " << taskId << endl;
    std::cout << "LIFECYCLE: " << eventType << " task=" << taskId
              << " src=" << sourceEntity << " dst=" << targetEntity << std::endl;

    veins::TaskOffloadingEvent* event = new veins::TaskOffloadingEvent();
    event->setTask_id(taskId.c_str());
    event->setEvent_type(eventType.c_str());
    event->setEvent_time(simTime().dbl());
    event->setSource_entity_id(sourceEntity.c_str());
    event->setTarget_entity_id(targetEntity.c_str());
    event->setEvent_details(details.c_str());

    LAddress::L2Type rsuMac = selectBestRSU();
    if (rsuMac != 0) {
        populateWSM(event, rsuMac);
        sendDown(event);
    } else {
        EV_WARN << "No RSU in range — lifecycle event dropped: " << eventType << endl;
        delete event;
    }
}

// Overload: no details (backward compat)
void PayloadVehicleApp::sendTaskOffloadingEvent(const std::string& taskId, const std::string& eventType,
                                                 const std::string& sourceEntity, const std::string& targetEntity) {
    sendTaskOffloadingEvent(taskId, eventType, sourceEntity, targetEntity, "{}");
}

void PayloadVehicleApp::sendTaskCompletionToRSU(const std::string& taskId, double completionTime, 
                                                  bool success, bool onTime, 
                                                  uint64_t taskSizeBytes, uint64_t cpuCycles, 
                                                  double qosValue, const std::string& resultData) {
    EV_INFO << "📤 Sending task completion report to RSU for task: " << taskId << endl;
    
    // Find timing info for this task
    auto it = taskTimings.find(taskId);
    if (it == taskTimings.end()) {
        EV_WARN << "No timing info found for task " << taskId << ", cannot send completion report" << endl;
        return;
    }
    
    const TaskTimingInfo& timing = it->second;
    
    // Create TaskResultMessage and repurpose it to carry completion data
    veins::TaskResultMessage* resultMsg = new veins::TaskResultMessage();
    resultMsg->setTask_id(taskId.c_str());
    resultMsg->setSuccess(success);
    resultMsg->setCompletion_time(completionTime);
    
    // Pack timing data into result_data field as JSON
    std::ostringstream timingJson;
    timingJson << "{";
    timingJson << "\"request_time\":" << timing.request_time << ",";
    timingJson << "\"decision_time\":" << timing.decision_time << ",";
    timingJson << "\"start_time\":" << timing.start_time << ",";
    timingJson << "\"completion_time\":" << completionTime << ",";
    timingJson << "\"decision_type\":\"" << timing.decision_type << "\",";
    timingJson << "\"processor_id\":\"" << timing.processor_id << "\",";
    timingJson << "\"on_time\":" << (onTime ? "true" : "false") << ",";
    timingJson << "\"mem_footprint_bytes\":" << taskSizeBytes << ",";
    timingJson << "\"cpu_cycles\":" << cpuCycles << ",";
    timingJson << "\"qos_value\":" << qosValue << ",";
    timingJson << "\"result\":\"" << resultData << "\"";
    timingJson << "}";
    
    resultMsg->setTask_output_data(timingJson.str().c_str());
    resultMsg->setOrigin_vehicle_id(("VEHICLE_" + std::to_string(getParentModule()->getIndex())).c_str());
    resultMsg->setProcessor_id(timing.processor_id.c_str());
    
    // Send to RSU
    LAddress::L2Type rsuMac = selectBestRSU();
    if (rsuMac != 0) {
        populateWSM(resultMsg, rsuMac);
        sendDown(resultMsg);
        
        // Calculate and log latencies
        double decisionLatency = timing.decision_time - timing.request_time;
        double processingLatency = completionTime - timing.start_time;
        double totalLatency = completionTime - timing.request_time;
        
        EV_INFO << "Task completion report sent:" << endl;
        EV_INFO << "  • Decision latency: " << decisionLatency << "s" << endl;
        EV_INFO << "  • Processing latency: " << processingLatency << "s" << endl;
        EV_INFO << "  • Total latency: " << totalLatency << "s" << endl;
        EV_INFO << "  • Success: " << (success ? "Yes" : "No") << endl;
        EV_INFO << "  • On-time: " << (onTime ? "Yes" : "No") << endl;
        
        std::cout << "COMPLETION_REPORT: Task " << taskId << " - Total latency: " << totalLatency 
                  << "s, Success: " << success << ", On-time: " << onTime << std::endl;
    } else {
        EV_WARN << "No RSU available to send completion report" << endl;
        delete resultMsg;
    }
    
    // Clean up task tracking data
    taskTimings.erase(taskId);
    taskCandidates.erase(taskId);
    task_redirect_counts.erase(taskId);
}

} // namespace complex_network
