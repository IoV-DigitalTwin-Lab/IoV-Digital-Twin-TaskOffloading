#include "MyRSUApp.h"
#include "veins/modules/mobility/traci/TraCICommandInterface.h"
#include "veins/modules/mobility/traci/TraCIScenarioManager.h"
#include <sstream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <cmath>
#include <limits>
#include <algorithm>
#include <random>
#include <cstdint>
#include <cctype>
#include <unordered_set>

using namespace veins;

namespace complex_network {

namespace {
constexpr const char* kRoutePrefix = "ROUTE|";
constexpr const char* kRelayPayloadPrefix = "RSU_RELAY_RESULT|";
constexpr const char* kServiceRoutePrefix = "SVROUTE|";
constexpr const char* kServiceResultRelayPrefix = "SV_RESULT_RELAY|";
constexpr int64_t kPacketHeaderBytes = 256;
constexpr int64_t kResultMinPayloadBytes = 512;

int64_t clampPacketBytes(uint64_t payloadBytes) {
    const int64_t maxSafe = static_cast<int64_t>(std::numeric_limits<int32_t>::max() - kPacketHeaderBytes);
    const int64_t payload = static_cast<int64_t>(std::min<uint64_t>(payloadBytes, static_cast<uint64_t>(maxSafe)));
    return std::max<int64_t>(1, kPacketHeaderBytes + payload);
}

void setResultPacketSize(veins::TaskResultMessage* packet, uint64_t outputBytes) {
    const uint64_t payload = std::max<uint64_t>(outputBytes, static_cast<uint64_t>(kResultMinPayloadBytes));
    packet->setByteLength(clampPacketBytes(payload));
}

bool parseRouteHint(const std::string& routeHint, LAddress::L2Type& ingressRsuMac, LAddress::L2Type& processorRsuMac) {
    ingressRsuMac = 0;
    processorRsuMac = 0;
    if (routeHint.rfind(kRoutePrefix, 0) != 0) {
        return false;
    }

    size_t ingressPos = routeHint.find("ingress=");
    size_t processorPos = routeHint.find("processor=");
    if (ingressPos == std::string::npos || processorPos == std::string::npos) {
        return false;
    }

    try {
        size_t ingressStart = ingressPos + 8;
        size_t ingressEnd = routeHint.find('|', ingressStart);
        size_t processorStart = processorPos + 10;
        size_t processorEnd = routeHint.find('|', processorStart);

        std::string ingressStr = routeHint.substr(ingressStart, ingressEnd - ingressStart);
        std::string processorStr = routeHint.substr(processorStart, processorEnd - processorStart);

        ingressRsuMac = static_cast<LAddress::L2Type>(std::stoull(ingressStr));
        processorRsuMac = static_cast<LAddress::L2Type>(std::stoull(processorStr));
    }
    catch (...) {
        ingressRsuMac = 0;
        processorRsuMac = 0;
        return false;
    }
    return true;
}

bool isRelayTaskResult(const TaskResultMessage* msg) {
    std::string payload = msg->getTask_output_data();
    return payload.rfind(kRelayPayloadPrefix, 0) == 0;
}

std::string stripRelayPrefix(const std::string& payload) {
    if (payload.rfind(kRelayPayloadPrefix, 0) == 0) {
        return payload.substr(std::char_traits<char>::length(kRelayPayloadPrefix));
    }
    return payload;
}

bool parseServiceRouteHint(const std::string& hint, LAddress::L2Type& serviceVehicleMac, LAddress::L2Type& anchorRsuMac) {
    serviceVehicleMac = 0;
    anchorRsuMac = 0;
    if (hint.rfind(kServiceRoutePrefix, 0) != 0) return false;

    size_t svPos = hint.find("sv=");
    size_t anchorPos = hint.find("anchor=");
    if (svPos == std::string::npos || anchorPos == std::string::npos) return false;

    try {
        size_t svStart = svPos + 3;
        size_t svEnd = hint.find('|', svStart);
        size_t anchorStart = anchorPos + 7;
        size_t anchorEnd = hint.find('|', anchorStart);
        serviceVehicleMac = static_cast<LAddress::L2Type>(std::stoull(hint.substr(svStart, svEnd - svStart)));
        anchorRsuMac = static_cast<LAddress::L2Type>(std::stoull(hint.substr(anchorStart, anchorEnd - anchorStart)));
    } catch (...) {
        serviceVehicleMac = 0;
        anchorRsuMac = 0;
        return false;
    }
    return true;
}

bool parseServiceResultRelayHint(const std::string& payload, LAddress::L2Type& targetRsuMac, std::string& cleanPayload) {
    targetRsuMac = 0;
    cleanPayload = payload;
    if (payload.rfind(kServiceResultRelayPrefix, 0) != 0) return false;

    size_t targetPos = payload.find("target=");
    if (targetPos == std::string::npos) return false;

    try {
        size_t valueStart = targetPos + 7;
        size_t valueEnd = payload.find('|', valueStart);
        targetRsuMac = static_cast<LAddress::L2Type>(std::stoull(payload.substr(valueStart, valueEnd - valueStart)));
        cleanPayload = (valueEnd == std::string::npos) ? std::string() : payload.substr(valueEnd + 1);
    } catch (...) {
        return false;
    }
    return true;
}

bool shouldAlwaysPrintLifecycleEvent(const std::string& eventType) {
    return eventType.find("FAIL") != std::string::npos
        || eventType.find("TIMEOUT") != std::string::npos
        || eventType.find("REJECT") != std::string::npos
        || eventType.find("DROPPED") != std::string::npos
        || eventType.find("ERROR") != std::string::npos
        || eventType.find("LOST") != std::string::npos;
}

std::string extractCanonicalVehicleId(const std::string& id) {
    if (id.rfind("SV_", 0) == 0) {
        return id.substr(3);
    }
    if (id.rfind("VEHICLE_", 0) == 0) {
        return id.substr(8);
    }
    if (id.rfind("RSU_", 0) == 0) {
        return std::string();
    }
    return id;
}

std::string canonicalServiceVehicleId(const std::string& id) {
    const std::string raw = extractCanonicalVehicleId(id);
    if (raw.empty()) {
        return std::string();
    }
    return "SV_" + raw;
}

double estimateRssiFromDistance(double distanceMeters) {
    // Lightweight monotonic RSSI proxy for ranking RSUs by radio proximity.
    // Absolute calibration is not required here; ordering by distance is the goal.
    double d = std::max(1.0, distanceMeters);
    return -40.0 - 20.0 * std::log10(d);
}

double estimateSinrDbFromDistance(double distanceMeters) {
    const double tx_power_dbm = 33.0;   // 2000 mW per omnetpp.ini txPower
    const double frequency_hz = 5.89e9;
    const double pathloss_alpha = 2.2;   // Matches config.xml SimplePathlossModel alpha
    const double noise_floor_dbm = -110.0;

    const double d_m = std::max(1.0, distanceMeters);
    const double lambda = 299792458.0 / frequency_hz;
    const double path_loss_db = 20.0 * std::log10(4.0 * M_PI / lambda)
                                + 10.0 * pathloss_alpha * std::log10(d_m);
    const double rx_power_dbm = tx_power_dbm - path_loss_db;
    const double sinr_db = rx_power_dbm - noise_floor_dbm;

    return std::max(-30.0, std::min(80.0, sinr_db));
}

std::string extractJsonStringValue(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\":\"";
    const size_t start = json.find(needle);
    if (start == std::string::npos) return "";
    const size_t valueStart = start + needle.size();
    const size_t valueEnd = json.find('"', valueStart);
    if (valueEnd == std::string::npos || valueEnd <= valueStart) return "";
    return json.substr(valueStart, valueEnd - valueStart);
}

std::string extractJsonNumberValue(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\":";
    const size_t start = json.find(needle);
    if (start == std::string::npos) return "";
    size_t valueStart = start + needle.size();
    while (valueStart < json.size() && std::isspace(static_cast<unsigned char>(json[valueStart]))) {
        ++valueStart;
    }
    size_t valueEnd = valueStart;
    while (valueEnd < json.size() && json[valueEnd] != ',' && json[valueEnd] != '}') {
        ++valueEnd;
    }
    if (valueEnd <= valueStart) return "";
    std::string token = json.substr(valueStart, valueEnd - valueStart);

    // Trim trailing whitespace
    while (!token.empty() && std::isspace(static_cast<unsigned char>(token.back()))) {
        token.pop_back();
    }

    // JSON null should map to empty so SQL NULLIF(...,'') casts safely.
    if (token == "null" || token == "NULL") {
        return "";
    }

    // Accept numeric values encoded as quoted strings.
    if (token.size() >= 2 && token.front() == '"' && token.back() == '"') {
        token = token.substr(1, token.size() - 2);
    }

    // Guard against non-numeric sentinels that would fail ::double precision casts.
    if (token == "nan" || token == "NaN" || token == "inf" || token == "-inf"
            || token == "Infinity" || token == "-Infinity") {
        return "";
    }

    return token;
}

std::string classifyLifecycleStatus(const std::string& eventType) {
    if (eventType.find("FAIL") != std::string::npos || eventType.find("REJECT") != std::string::npos) {
        return "FAILED";
    }
    if (eventType.find("COMPLETE") != std::string::npos || eventType.find("SUCCESS") != std::string::npos) {
        return "SUCCESS";
    }
    return "IN_PROGRESS";
}
}

Define_Module(MyRSUApp);

void MyRSUApp::initialize(int stage) {
    DemoBaseApplLayer::initialize(stage);
    
    if (stage == 0) {
        // Get RSU ID from module index
        rsu_id = getParentModule()->getIndex();
        
        // Initialize edge server resources
        edgeCPU_GHz = par("edgeCPU_GHz").doubleValue();
        edgeMemory_GB = par("edgeMemory_GB").doubleValue();
        maxVehicles = par("maxVehicles").intValue();
        processingDelay_ms = par("processingDelay_ms").doubleValue();
        terminalVerboseLogs = hasPar("terminalVerboseLogs") ? par("terminalVerboseLogs").boolValue() : false;
        directLinkTxPower_mW = par("directLinkTxPower_mW").doubleValue();
        directLinkCarrierFrequency_Hz = par("directLinkCarrierFrequency_Hz").doubleValue();
        directLinkAntennaHeight_m = par("directLinkAntennaHeight_m").doubleValue();
        directLinkRssiThreshold_dBm = par("directLinkRssiThreshold_dBm").doubleValue();
        offload_link_bandwidth_hz = par("offloadLinkBandwidthHz").doubleValue();
        offload_rate_efficiency = par("offloadRateEfficiency").doubleValue();
        offload_rate_cap_bps = par("offloadRateCapBps").doubleValue();
        offload_response_ratio = par("offloadResponseRatio").doubleValue();

        secondary_link_context_export = par("secondaryLinkContextExport").boolValue();
        secondary_run_id = par("secondaryRunId").stdstringValue();
        secondary_link_sample_interval = par("secondaryLinkSampleInterval").doubleValue();
        secondary_redis_max_series_len = par("secondaryRedisMaxSeriesLen").intValue();
        secondary_predictor_mode = par("secondaryPredictorMode").stdstringValue();
        secondary_use_external_predictions = par("secondaryUseExternalPredictions").boolValue();
        secondary_prediction_horizon_s = par("secondaryPredictionHorizon").doubleValue();
        secondary_prediction_step_s = par("secondaryPredictionStep").doubleValue();
        secondary_cycle_interval_s = par("secondaryCycleInterval").doubleValue();
        secondary_candidate_radius_m = par("secondaryCandidateRadius").doubleValue();
        secondary_q_publish_enabled = par("secondaryQPublishEnabled").boolValue();
        secondary_sinr_threshold_db = par("secondarySinrThresholdDb").doubleValue();
        secondary_interference_mw = par("secondaryInterferenceMw").doubleValue();
        secondary_q_ttl_s = par("secondaryQTTLSec").intValue();
        secondary_q_stream_maxlen = par("secondaryQStreamMaxLen").intValue();
        secondary_nakagami_enabled = par("secondaryNakagamiEnabled").boolValue();
        secondary_nakagami_m = par("secondaryNakagamiM").doubleValue();
        secondary_nakagami_cell_size_m = par("secondaryNakagamiCellSize").doubleValue();
        if (!secondary_use_external_predictions) {
            initializeSecondaryPredictor();
        }
        initializeSecondaryCycleWorker();
        initializeRsuStaticContexts();
        loadObstaclePolygons();
        // Initialize RSU resource tracking (dynamic state)
        rsu_cpu_total = edgeCPU_GHz;
        rsu_cpu_available = edgeCPU_GHz;  // Initially all available
        rsu_memory_total = edgeMemory_GB;
        rsu_memory_available = edgeMemory_GB;
        rsu_background_cpu_util = uniform(0.05, 0.18);
        rsu_background_mem_util = uniform(0.12, 0.30);
        rsu_last_background_update = simTime().dbl();
        // Use dedicated rsuMaxConcurrent param (prevents maxVehicles from inflating the cap)
        rsu_max_concurrent = par("rsuMaxConcurrent").intValue();
        if (hasPar("rsuQueueCapacity")) {
            rsu_waiting_queue_capacity = std::max(0L, par("rsuQueueCapacity").intValue());
        }
        
        // Initialize Redis Digital Twin
        use_redis = par("useRedis").boolValue();
        if (use_redis) {
            redis_host = par("redisHost").stdstringValue();
            redis_port = par("redisPort").intValue();
            redis_db = par("redisDb").intValue();
            // If unset or invalid, map RSU 0/1/2 to Redis DB 4/5/6.
            if (redis_db < 0) {
                redis_db = rsu_id;
            }
            redis_twin = new RedisDigitalTwin(redis_host, redis_port, redis_db);
            if (redis_twin->isConnected()) {
                EV_INFO << "✓ Redis Digital Twin connected at " << redis_host << ":" << redis_port
                        << " db=" << redis_db << std::endl;
                if (terminalVerboseLogs) {
                    std::cout << "✓ RSU[" << rsu_id << "] Redis Digital Twin connected (db="
                              << redis_db << ")" << std::endl;
                }
            } else {
                EV_WARN << "✗ Redis connection failed, continuing without digital twin" << std::endl;
                std::cerr << "⚠ RSU[" << rsu_id << "] Redis connection failed" << std::endl;
                delete redis_twin;
                redis_twin = nullptr;
            }
        }
        
        // Write sim config so Python agent can label results correctly (written once by RSU 0)
        if (rsu_id == 0 && redis_twin && use_redis) {
            std::string offload_mode_str = par("offloadMode").stdstringValue();
            redis_twin->writeSimConfig(offload_mode_str);
            if (terminalVerboseLogs) {
                std::cout << "RSU[0] sim:offload_mode=" << offload_mode_str << " written to Redis" << std::endl;
            }
        }

        // Insert RSU metadata (static info) once at initialization
        insertRSUMetadata();
        
        // Initialize RSU-to-RSU broadcast parameters (try to get from config, use defaults if not set)
        try {
            rsu_broadcast_interval = par("rsuStatusBroadcastInterval").doubleValue();
        } catch (...) {
            rsu_broadcast_interval = 0.5;  // Default: 500ms
        }
        try {
            neighbor_state_ttl = par("neighborStateTtl").doubleValue();
        } catch (...) {
            neighbor_state_ttl = 1.5;  // Default: 1.5s
        }
        try {
            max_vehicles_in_broadcast = par("maxVehiclesInBroadcast").intValue();
        } catch (...) {
            max_vehicles_in_broadcast = 20;  // Default: 20 vehicles
        }
        try {
            max_redirect_hops = par("maxRedirectHops").intValue();
        } catch (...) {
            max_redirect_hops = 2;  // Default: 2 hops
        }
        
        // Start periodic RSU status updates
        rsu_status_update_timer = new cMessage("rsuStatusUpdate");
        // First RSU status at t=0 so Redis sees RSUs immediately
        scheduleAt(simTime(), rsu_status_update_timer);
        
        // Start periodic RSU-to-RSU status broadcast
        rsu_broadcast_timer = new cMessage("rsuBroadcast");
        scheduleAt(simTime() + uniform(0.0, rsu_broadcast_interval), rsu_broadcast_timer);  // Stagger initial broadcasts

        // Initialize decision checker lazily when the first task metadata arrives.
        checkDecisionMsg = nullptr;
        
        if (terminalVerboseLogs) {
            std::cout << "CONSOLE: MyRSUApp " << getParentModule()->getFullName()
                      << " initialized with edge resources:" << std::endl;
            std::cout << "  - RSU ID: " << rsu_id << std::endl;
            std::cout << "  - CPU: " << edgeCPU_GHz << " GHz" << std::endl;
            std::cout << "  - Memory: " << edgeMemory_GB << " GB" << std::endl;
            std::cout << "  - Max Vehicles: " << maxVehicles << std::endl;
            std::cout << "  - Base Processing Delay: " << processingDelay_ms << " ms" << std::endl;
        }
        
        double interval = par("beaconInterval").doubleValue();

        if (par("sendBeacons").boolValue()) {
            // create and store the beacon self-message so we can cancel/delete it safely later
            beaconMsg = new cMessage("sendMessage");
            scheduleAt(simTime() + 2.0, beaconMsg);
        }

        // Schedule periodic terminal progress printer
        progressMsg_ = new cMessage("simProgress");
        scheduleAt(simTime() + kProgressIntervalS, progressMsg_);

        EV << "RSU initialized with beacon interval: " << interval << "s" << endl;
    }
}

double MyRSUApp::estimateDirectLinkRssiDbm(double distanceMeters) const {
    const double c = 3.0e8;
    const double d = std::max(1.0, distanceMeters);
    const double frequencyHz = std::max(1.0, directLinkCarrierFrequency_Hz);
    const double lambda = c / frequencyHz;
    const double antennaHeightM = std::max(0.1, directLinkAntennaHeight_m);
    const double txPowerDbm = 10.0 * std::log10(std::max(1e-12, directLinkTxPower_mW));
    const double breakpointDistance = (4.0 * M_PI * antennaHeightM * antennaHeightM) / lambda;

    double pathLossDb = 0.0;
    if (d <= breakpointDistance) {
        pathLossDb = 20.0 * std::log10((4.0 * M_PI * d) / lambda);
    } else {
        pathLossDb = 40.0 * std::log10(d)
                   - 20.0 * std::log10(antennaHeightM)
                   - 20.0 * std::log10(antennaHeightM);
    }

    return txPowerDbm - pathLossDb;
}

void MyRSUApp::handleSelfMsg(cMessage* msg) {
    if (msg == beaconMsg && strcmp(msg->getName(), "sendMessage") == 0) {
        DemoSafetyMessage* wsm = new DemoSafetyMessage();
        populateWSM(wsm);
        wsm->setSenderPos(curPosition);
        wsm->setUserPriority(par("beaconUserPriority").intValue());
        sendDown(wsm);

        EV << "RSU: Sent beacon at time " << simTime() << endl;

        double interval = par("beaconInterval").doubleValue();
        scheduleAt(simTime() + interval, msg);
    } 
    else if (msg == rsu_status_update_timer) {
        // Handle RSU status update
        sendRSUStatusUpdate();
    } 
    else if (msg == rsu_broadcast_timer) {
        // Handle RSU-to-RSU status broadcast
        broadcastRSUStatus();
        // Cleanup stale neighbor states periodically
        cleanupStaleNeighborStates();
        // Reschedule next broadcast
        scheduleAt(simTime() + rsu_broadcast_interval, rsu_broadcast_timer);
    }
    else if (msg == progressMsg_) {
        // Print per-agent results summary to stdout
        static const std::vector<std::string> kAgents =
            {"ddqn", "random", "greedy_comp", "min_latency", "least_queue"};
        std::cout << "\n=== SIM PROGRESS t=" << simTime().dbl()
                  << "s | RSU_" << rsu_id
                  << " | rsu_tasks_processed=" << rsu_tasks_processed
                  << " pending=" << rsu_processing_count << " ===\n";
        for (const auto& a : kAgents) {
            int ok   = agent_ok_.count(a)   ? agent_ok_.at(a)   : 0;
            int fail = agent_fail_.count(a) ? agent_fail_.at(a) : 0;
            int tot  = ok + fail;
            double sr = tot ? 100.0 * ok / tot : 0.0;
            std::cout << "  " << std::left << std::setw(12) << a
                      << " OK=" << std::setw(5) << ok
                      << " FAIL=" << std::setw(5) << fail
                      << " (" << std::fixed << std::setprecision(1) << sr << "%)\n";
        }
        std::cout << "===\n";
        scheduleAt(simTime() + kProgressIntervalS, progressMsg_);
    }
    else if (msg == secondary_cycle_timer) {
        runSecondaryCycle();
        scheduleAt(simTime() + secondary_cycle_interval_s, secondary_cycle_timer);
    }
    
    else if (msg == checkDecisionMsg) {
        // Poll Redis for DRL decisions.
        // Iterates only pending_decision_ids_ (O(N_pending)) instead of all task_records
        // (O(N_total)).  N_pending is bounded by rsuDecisionTimeout × generation_rate and
        // stays constant regardless of simulation length — critical for long training runs.
        int decisions_found = 0;
        int decisions_sent = 0;

        // Collect IDs whose decision has been sent (or whose record is already done).
        std::vector<std::string> decided;

        for (const auto& tid : pending_decision_ids_) {
            auto it = task_records.find(tid);
            if (it == task_records.end()) {
                decided.push_back(tid);  // record was cleaned up — drop from pending
                continue;
            }
            TaskRecord& record = it->second;

            // Already decided / completed / failed on a previous cycle — skip
            if (record.completed || record.failed || record.decision_sent) {
                decided.push_back(tid);
                continue;
            }

            {  // scoped block mirrors the original per-task decision logic
                std::string decision_type;
                std::string target_id;
                double confidence = 0.8;  // Default confidence
                bool found_decision = false;
                bool decision_from_redis = false;
                std::string decision_source = "";
                std::string agentDecisionsPayload;  // will be set if multi-agent
                std::string drl_dequeued_wall_s;
                std::string drl_state_ready_wall_s;
                std::string drl_decision_written_wall_s;
                std::string drl_result_timeout_discard_wall_s;
                std::string drl_result_timeout_s;

                // Single-agent decision: poll task:{id}:decision written by Python agent
                if (redis_twin && use_redis) {
                    auto dec = redis_twin->getSingleDecision(record.task_id);
                    if (!dec.empty() && dec.count("type")) {
                        decision_type = dec.at("type");
                        target_id = dec.count("target") ? dec.at("target") : "";
                        found_decision = true;
                        decision_from_redis = true;
                        decision_source = "REDIS_SINGLE";
                        drl_dequeued_wall_s = dec.count("drl_dequeued_wall_s") ? dec.at("drl_dequeued_wall_s") : "";
                        drl_state_ready_wall_s = dec.count("drl_state_ready_wall_s") ? dec.at("drl_state_ready_wall_s") : "";
                        drl_decision_written_wall_s = dec.count("drl_decision_written_wall_s") ? dec.at("drl_decision_written_wall_s") : "";
                        drl_result_timeout_discard_wall_s = dec.count("drl_result_timeout_discard_wall_s") ? dec.at("drl_result_timeout_discard_wall_s") : "";
                        drl_result_timeout_s = dec.count("drl_result_timeout_s") ? dec.at("drl_result_timeout_s") : "";
                        record.decision_poll_miss_count = 0;
                        record.decision_poll_miss_logged = false;
                        EV_INFO << "✓ Single-agent decision for task " << record.task_id
                                << ": type=" << decision_type << " target=" << target_id << endl;
                        if (terminalVerboseLogs) {
                            std::cout << "ML_DECISION: Task " << record.task_id
                                      << " -> " << decision_type << " target=" << target_id << std::endl;
                        }
                        } else {
                        auto multi = redis_twin->getMultiAgentDecisions(record.task_id);
                        if (!multi.empty() && multi.count("ddqn_type")) {
                            decision_type = multi.at("ddqn_type");
                            target_id = multi.count("ddqn_target") ? multi.at("ddqn_target") : "";
                            found_decision = true;
                            decision_from_redis = true;
                            decision_source = "REDIS_MULTI";
                            record.decision_poll_miss_count = 0;
                            record.decision_poll_miss_logged = false;
                            agentDecisionsPayload = multi.count("agents") ? multi.at("agents") : "";
                            EV_INFO << "✓ Multi-agent DDQN decision for task " << record.task_id
                                << ": type=" << decision_type << " target=" << target_id << endl;
                            if (terminalVerboseLogs) {
                                std::cout << "ML_DECISION: Task " << record.task_id
                                          << " -> " << decision_type << " target=" << target_id
                                          << " [compat:task:{id}:decisions]" << std::endl;
                            }
                        }
                    }
                    // When Redis is active, only use single-agent decision.
                    // Do not use any legacy storage fallback path.
                }

                // No legacy persistence fallback. Decision dispatch is Redis-only.
                
                // Send decision if found
                if (found_decision) {
                    decisions_found++;
                    
                    // Create decision message
                    OffloadingDecisionMessage* dMsg = new OffloadingDecisionMessage();
                    dMsg->setTask_id(record.task_id.c_str());
                    dMsg->setVehicle_id(record.vehicle_id.c_str());
                    dMsg->setDecision_type(decision_type.c_str());
                    dMsg->setTarget_service_vehicle_id(target_id.c_str());
                    dMsg->setConfidence_score(confidence);
                    dMsg->setDecision_time(simTime().dbl());
                    
                    double estimated_completion_time = 0.0;
                    if (decision_type == "SERVICE_VEHICLE" && !target_id.empty()) {
                        auto svIt = vehicle_twins.find(target_id);
                        auto tvIt = vehicle_twins.find(record.vehicle_id);
                        if (svIt != vehicle_twins.end() && tvIt != vehicle_twins.end()) {
                            const auto& twin = svIt->second;
                            const auto& tvTwin = tvIt->second;
                            
                            Coord tvPos(tvTwin.pos_x, tvTwin.pos_y);
                            Coord svPos(twin.pos_x, twin.pos_y);
                            const double distance_m = tvPos.distance(svPos);
                            const double rssi_dbm = estimateDirectLinkRssiDbm(distance_m);

                            const double quality = std::max(0.0, std::min(1.0,
                                (rssi_dbm - directLinkRssiThreshold_dBm) / 20.0));
                            const double base_rate_bps = std::max(1.0, offload_link_bandwidth_hz * offload_rate_efficiency);
                            double est_rate_bps = base_rate_bps * (0.2 + 0.8 * quality);
                            if (offload_rate_cap_bps > 0.0) {
                                est_rate_bps = std::min(est_rate_bps, offload_rate_cap_bps);
                            }

                            const double input_bytes = static_cast<double>(record.is_profile_task ? record.input_size_bytes : record.mem_footprint_bytes);
                            const double output_bytes = static_cast<double>(record.is_profile_task ? record.output_size_bytes : (record.mem_footprint_bytes * offload_response_ratio));
                            
                            const double uplink_s = (input_bytes * 8.0) / std::max(1.0, est_rate_bps);
                            const double downlink_s = (output_bytes * 8.0) / std::max(1.0, est_rate_bps);

                            const double svc_cpu_hz = std::max(1e8, twin.cpu_available * 1e9);
                            const double exec_s = static_cast<double>(record.cpu_cycles) / svc_cpu_hz;

                            const double pending_factor = static_cast<double>(twin.current_queue_length + twin.current_processing_count);
                            const double queue_wait_s = pending_factor * exec_s * 0.6;

                            estimated_completion_time = uplink_s + queue_wait_s + exec_s + downlink_s + 0.01;
                        } else {
                            estimated_completion_time = record.deadline_seconds * 0.9;
                        }
                    } else if (decision_type == "RSU") {
                        estimated_completion_time = record.deadline_seconds * 0.6;
                    } else if (decision_type == "LOCAL") {
                        estimated_completion_time = record.deadline_seconds * 0.5;
                    }
                    dMsg->setEstimated_completion_time(estimated_completion_time);
                    
                    dMsg->setSenderAddress(myId);  // RSU MAC address
                    dMsg->setAgentDecisions(agentDecisionsPayload.c_str());  // baseline decisions

                    // Persist DRL decisions through the active runtime path.
                    // even when runtime decisions come from Redis.
                    if (decision_from_redis) {
                        const double rsu_wait_s = std::max(0.0, simTime().dbl() - record.received_time);
                        insertOffloadingDecision(record.task_id, dMsg);
                        insertLifecycleEvent(record.task_id, "OFFLOADING_DECISION_IN_PROCESS",
                            "RSU_" + std::to_string(rsu_id), record.vehicle_id,
                            std::string("{\"decision_type\":\"") + decision_type + "\","
                            "\"target\":\"" + target_id + "\","
                            "\"decision_source\":\"" + decision_source + "\","
                            "\"poll_miss_count\":" + std::to_string(record.decision_poll_miss_count) + ","
                            "\"rsu_wait_s\":" + std::to_string(rsu_wait_s) + ","
                            "\"drl_dequeued_wall_s\":" + (drl_dequeued_wall_s.empty() ? "null" : drl_dequeued_wall_s) + ","
                            "\"drl_state_ready_wall_s\":" + (drl_state_ready_wall_s.empty() ? "null" : drl_state_ready_wall_s) + ","
                            "\"drl_decision_written_wall_s\":" + (drl_decision_written_wall_s.empty() ? "null" : drl_decision_written_wall_s) + ","
                            "\"drl_result_timeout_discard_wall_s\":" + (drl_result_timeout_discard_wall_s.empty() ? "null" : drl_result_timeout_discard_wall_s) + ","
                            "\"drl_result_timeout_s\":" + (drl_result_timeout_s.empty() ? "null" : drl_result_timeout_s) + "}");
                    }

                    // Set target service vehicle MAC if applicable
                    if (decision_type == "SERVICE_VEHICLE" && !target_id.empty()) {
                        if (vehicle_macs.count(target_id)) {
                            dMsg->setTarget_service_vehicle_mac(vehicle_macs[target_id]);
                            EV_INFO << "  → Target SV: " << target_id << endl;
                        } else {
                            EV_WARN << "  ⚠ Service vehicle " << target_id << " MAC not found" << endl;
                        }
                    }
                    
                    // Send decision to requesting vehicle
                    if (vehicle_macs.count(record.vehicle_id)) {
                        populateWSM(dMsg, vehicle_macs[record.vehicle_id]);
                        sendDown(dMsg);

                        if (decision_from_redis) {
                            const double rsu_wait_s = std::max(0.0, simTime().dbl() - record.received_time);
                            insertLifecycleEvent(record.task_id, "OFFLOADING_DECISION_SENDING",
                                "RSU_" + std::to_string(rsu_id), record.vehicle_id,
                                std::string("{\"decision_type\":\"") + decision_type + "\","
                                "\"target\":\"" + target_id + "\","
                                "\"decision_source\":\"" + decision_source + "\","
                                "\"send_target_vehicle\":\"" + record.vehicle_id + "\","
                                "\"rsu_wait_s\":" + std::to_string(rsu_wait_s) + "}");
                        }
                        
                        record.decision_sent = true;
                        result_awaiting_[record.task_id] = simTime();
                        decided.push_back(tid);  // remove from pending after decision sent
                        decisions_sent++;

                        EV_INFO << "✓ Sent ML decision to vehicle " << record.vehicle_id << endl;
                        std::cout << "RSU_SEND: Decision sent to " << record.vehicle_id
                                  << " for task " << record.task_id << std::endl;
                    } else {
                        EV_WARN << "⚠ Vehicle " << record.vehicle_id << " MAC not found, cannot send decision" << endl;
                        insertLifecycleEvent(record.task_id, "OFFLOADING_DECISION_SEND_FAIL",
                            "RSU_" + std::to_string(rsu_id), record.vehicle_id,
                            std::string("{\"reason_code\":\"DECISION_TIMEOUT_VEHICLE_MAC_UNKNOWN\",") +
                            "\"decision_type\":\"" + decision_type + "\","
                            "\"target\":\"" + target_id + "\"}");
                        delete dMsg;
                        // Write FAILED to Redis so DRL's pending{} entry resolves immediately
                        // instead of waiting 30 s for its own timeout. Vehicle has left coverage
                        // so no execution will happen; wasted_s = time held at RSU.
                        if (redis_twin && use_redis) {
                            double wasted_s = std::max(0.0, simTime().dbl() - record.received_time);
                            redis_twin->writeSingleResult(
                                record.task_id, "FAILED", wasted_s, 0.0, "VEHICLE_DEPARTED");
                        }
                        // MAC not found — drop from pending so we don't retry forever.
                        // Mark decision_sent so the periodic GC can reclaim the record.
                        record.decision_sent = true;
                        decided.push_back(tid);
                    }
                } else {
                    record.decision_poll_miss_count++;
                    if (!record.decision_poll_miss_logged || (record.decision_poll_miss_count % 20 == 0)) {
                        insertLifecycleEvent(record.task_id, "DECISION_POLL_MISS",
                            "RSU_" + std::to_string(rsu_id), record.vehicle_id,
                            std::string("{\"reason_code\":\"DECISION_TIMEOUT_REDIS_NO_DECISION\",") +
                            "\"miss_count\":" + std::to_string(record.decision_poll_miss_count) + ","
                            "\"source\":\"" + ((redis_twin && use_redis) ? "REDIS" : "NONE") + "\"}");
                        record.decision_poll_miss_logged = true;
                    }
                    // 600 misses * 50ms timer interval = 30 sim-seconds — matches Python's
                    // REDIS_RESULT_TIMEOUT so C++ gives up exactly when Python does.
                    // Write FAILED so any in-flight Python watcher gets a definitive answer
                    // and drop the task from pending to prevent unbounded polling.
                    if (record.decision_poll_miss_count >= 600) {
                        if (redis_twin && use_redis) {
                            double wasted_s = std::max(0.0, simTime().dbl() - record.received_time);
                            redis_twin->writeSingleResult(
                                record.task_id, "FAILED", wasted_s, 0.0, "DECISION_POLL_EXHAUSTED");
                        }
                        insertLifecycleEvent(record.task_id, "DECISION_POLL_EXHAUSTED",
                            "RSU_" + std::to_string(rsu_id), record.vehicle_id,
                            std::string("{\"miss_count\":") + std::to_string(record.decision_poll_miss_count) + "}");
                        record.decision_sent = true;
                        decided.push_back(tid);
                    }
                }
            }  // end scoped decision-logic block
        }  // end for (const auto& tid : pending_decision_ids_)

        // Remove sent/resolved IDs from the pending set in one pass.
        for (const auto& id : decided) pending_decision_ids_.erase(id);

        // Periodic task_records GC — runs every 100 sim-seconds (10 000 timer fires).
        // Removes records that are old enough for their task lifecycle to be certainly
        // over, preventing unbounded RAM growth during arbitrarily long training runs.
        // Tasks decided LOCAL/REJECT never generate an RSU completion callback, so
        // they would otherwise accumulate indefinitely in task_records.
        if (++cleanup_tick_ >= 10000) {
            cleanup_tick_ = 0;
            const double cutoff = simTime().dbl() - 120.0;  // 120 sim-s grace window
            std::vector<std::string> to_erase;
            for (const auto& pr : task_records) {
                if (pr.second.decision_sent && pr.second.created_time < cutoff) {
                    to_erase.push_back(pr.first);
                }
            }
            for (const auto& k : to_erase) task_records.erase(k);
            if (!to_erase.empty()) {
                EV_INFO << "[GC] Pruned " << to_erase.size()
                        << " old task records; remaining=" << task_records.size() << endl;
            }
        }

        // Sweep sv_subtask_pending_: SV agent subtasks that never returned a result
        // (SV unreachable, out of range, or message dropped).
        if (redis_twin && use_redis) {
            for (auto it = sv_subtask_pending_.begin(); it != sv_subtask_pending_.end(); ) {
                if ((simTime() - it->second).dbl() > sv_subtask_timeout_s) {
                    EV_WARN << "[TIMEOUT] SV subtask " << it->first
                            << " timed out after " << sv_subtask_timeout_s << "s" << endl;
                    redis_twin->writeSingleResult(
                        it->first, "FAILED", sv_subtask_timeout_s, 0.0, "SV_RESULT_TIMEOUT");
                    it = sv_subtask_pending_.erase(it);
                } else {
                    ++it;
                }
            }

            // Sweep result_awaiting_: regular offloaded tasks whose completion/failure
            // message was never received (vehicle drove out of RSU range, message lost).
            for (auto it = result_awaiting_.begin(); it != result_awaiting_.end(); ) {
                if ((simTime() - it->second).dbl() > result_awaiting_timeout_s) {
                    auto recIt = task_records.find(it->first);
                    const bool already_resolved = (recIt != task_records.end())
                                                  && (recIt->second.completed || recIt->second.failed);
                    if (!already_resolved) {
                        EV_WARN << "[TIMEOUT] Task " << it->first
                                << " result never received after " << result_awaiting_timeout_s
                                << "s — writing FAILED to Redis" << endl;
                        std::cout << "RESULT_LOST: task=" << it->first
                                  << " no completion/failure from vehicle in "
                                  << result_awaiting_timeout_s << "s" << std::endl;
                        double waited = (simTime() - it->second).dbl();
                        redis_twin->writeSingleResult(
                            it->first, "FAILED", waited, 0.0, "RESULT_NEVER_RECEIVED");
                    }
                    it = result_awaiting_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        if (decisions_found > 0) {
            EV_INFO << "📊 Decision poll: " << decisions_found << " found, "
                    << decisions_sent << " sent" << endl;
        }

        // Keep alive while decisions are pending OR while awaiting execution results.
        // New metadata arrivals will re-arm the timer if it has stopped.
        if (!pending_decision_ids_.empty() || !result_awaiting_.empty() || !sv_subtask_pending_.empty()) {
            scheduleAt(simTime() + 0.05, checkDecisionMsg);
        }

    }
    else if (strcmp(msg->getName(), "rsuTaskComplete") == 0) {
        // Physics-based RSU task completion — fire when exec_time elapses
        std::string* task_id_ptr = static_cast<std::string*>(msg->getContextPointer());
        std::string task_id = *task_id_ptr;
        delete task_id_ptr;
        delete msg;

        auto it = rsuPendingTasks.find(task_id);
        if (it != rsuPendingTasks.end()) {
            PendingRSUTask& pending = it->second;
            rsu_processing_count = std::max(0, rsu_processing_count - 1);
            rsu_tasks_processed++;
            rsu_total_processing_time += pending.exec_time_s;
            releaseRSUMemory(pending.mem_footprint_bytes);

            EV_INFO << "✅ RSU task complete: " << task_id
                    << " (exec=" << pending.exec_time_s << "s)" << endl;

            // Check if this is an agent sub-task (task_id encoded as "{orig}::{agent}")
            size_t sep = task_id.find("::");
            if (sep != std::string::npos && redis_twin && use_redis) {
                std::string orig_id    = task_id.substr(0, sep);
                std::string agent_name = task_id.substr(sep + 2);

                // Energy: CMOS model — κ_rsu * f² * cycles (κ_rsu=2e-27, f in Hz)
                double f_hz    = pending.cpu_allocated_hz;
                double energy_j = 2e-27 * f_hz * f_hz * static_cast<double>(pending.cpu_cycles);

                double total_latency = pending.exec_time_s;
                if (task_records.count(orig_id)) {
                    total_latency = std::max(0.0, simTime().dbl() - task_records.at(orig_id).created_time);
                }
                std::string status = "COMPLETED_ON_TIME";
                if (task_records.count(orig_id)) {
                    double deadline = task_records.at(orig_id).deadline_seconds;
                    status = (total_latency <= deadline) ? "COMPLETED_ON_TIME" : "FAILED";
                }
                std::string fail_reason = (status == "FAILED") ? "DEADLINE_MISSED" : "NONE";

                redis_twin->writeSingleResult(orig_id, status, total_latency, energy_j, fail_reason);
                std::cout << "RSU_RESULT: task=" << orig_id
                          << " status=" << status << " reason=" << fail_reason
                          << " latency=" << total_latency << "s" << std::endl;

                rsuPendingTasks.erase(it);
                if (rsu_processing_count > 0) {
                    reallocateRSUTasks();
                }
                return;
            }

            std::cout << "RSU_DONE: Task " << task_id
                      << " completed in " << pending.exec_time_s << "s" << std::endl;

            // Write DDQN metrics for real task completed on RSU
            if (redis_twin && use_redis) {
                double total_latency = pending.exec_time_s;
                if (task_records.count(task_id)) {
                    total_latency = std::max(0.0, simTime().dbl() - task_records.at(task_id).created_time);
                }
                std::string ddqn_status = "COMPLETED_ON_TIME";
                if (task_records.count(task_id)) {
                    double deadline = task_records.at(task_id).deadline_seconds;
                    ddqn_status = (total_latency <= deadline) ? "COMPLETED_ON_TIME" : "FAILED";
                }
                std::string ddqn_reason = (ddqn_status == "FAILED") ? "DEADLINE_MISSED" : "NONE";
                double f_hz = pending.cpu_allocated_hz;
                double energy_j = 2e-27 * f_hz * f_hz * static_cast<double>(pending.cpu_cycles);
                redis_twin->writeSingleResult(task_id, ddqn_status, total_latency, energy_j, ddqn_reason);
                std::cout << "RSU_RESULT: task=" << task_id
                          << " status=" << ddqn_status << " reason=" << ddqn_reason
                          << " latency=" << total_latency << "s" << std::endl;
            }

            sendTaskResultToVehicle(task_id, pending.vehicle_id, pending.vehicle_mac,
                                    pending.ingress_rsu_mac, true, pending.exec_time_s);
            rsuPendingTasks.erase(it);

            // Task completed: reallocate weighted CPU shares and admit queued work.
            reallocateRSUTasks();
            tryStartQueuedRSUTasks();
        } else {
            EV_WARN << "⚠ rsuTaskComplete: no pending record for " << task_id << endl;
        }
        return;
    }
    else {
        DemoBaseApplLayer::handleSelfMsg(msg);
    }
}

void MyRSUApp::handleLowerMsg(cMessage* msg) {
    EV << "MyRSUApp: handleLowerMsg() called" << endl;
    DemoBaseApplLayer::handleLowerMsg(msg);
}

void MyRSUApp::initializeSecondaryPredictor() {
    secondary_predictor = createFuturePositionPredictor(secondary_predictor_mode);
    if (!secondary_predictor) {
        throw cRuntimeError("Failed to initialize secondary predictor");
    }

    EV_INFO << "Secondary predictor initialized: mode=" << secondary_predictor->mode()
            << " horizon=" << secondary_prediction_horizon_s
            << "s step=" << secondary_prediction_step_s << "s" << endl;
}

void MyRSUApp::onWSM(BaseFrame1609_4* wsm) {
    // Check for task-related messages first
    TaskMetadataMessage* taskMetadata = dynamic_cast<TaskMetadataMessage*>(wsm);
    if (taskMetadata) {
        EV_INFO << "📥 RSU received TaskMetadataMessage" << endl;
        handleTaskMetadata(taskMetadata);
        return;
    }
    
    TaskCompletionMessage* taskCompletion = dynamic_cast<TaskCompletionMessage*>(wsm);
    if (taskCompletion) {
        EV_INFO << "📥 RSU received TaskCompletionMessage" << endl;
        handleTaskCompletion(taskCompletion);
        return;
    }
    
    TaskFailureMessage* taskFailure = dynamic_cast<TaskFailureMessage*>(wsm);
    if (taskFailure) {
        EV_INFO << "📥 RSU received TaskFailureMessage" << endl;
        handleTaskFailure(taskFailure);
        return;
    }
    
    VehicleResourceStatusMessage* resourceStatus = dynamic_cast<VehicleResourceStatusMessage*>(wsm);
    if (resourceStatus) {
        EV_INFO << "📥 RSU received VehicleResourceStatusMessage" << endl;
        handleVehicleResourceStatus(resourceStatus);
        return;
    }
    
    // Handle RSU-to-RSU status broadcasts
    RSUStatusBroadcastMessage* rsuStatus = dynamic_cast<RSUStatusBroadcastMessage*>(wsm);
    if (rsuStatus) {
        EV_DEBUG << "📥 RSU received RSUStatusBroadcastMessage" << endl;
        handleRSUStatusBroadcast(rsuStatus);
        return;
    }
    
    // Handle TaskResultMessage with completion timing data
    TaskResultMessage* taskResult = dynamic_cast<TaskResultMessage*>(wsm);
    if (taskResult) {
        // Agent subtask results (task_id contains "::") arrive directly from the SV without a
        // relay-hint prefix, so parseServiceResultRelayHint() would return false and the message
        // would fall through to handleTaskResultWithCompletion (wrong).  Intercept them here.
        {
            std::string _tid = taskResult->getTask_id();
            if (_tid.find("::") != std::string::npos) {
                handleServiceVehicleResultRelay(taskResult, 0);
                return;
            }
        }
        LAddress::L2Type svTargetRsuMac = 0;
        std::string cleanPayload;
        if (parseServiceResultRelayHint(taskResult->getTask_output_data(), svTargetRsuMac, cleanPayload)) {
            handleServiceVehicleResultRelay(taskResult, svTargetRsuMac);
            return;
        }
        if (isRelayTaskResult(taskResult)) {
            EV_INFO << "📥 RSU received relayed TaskResultMessage" << endl;
            handleRSUTaskResultRelay(taskResult);
            return;
        }
        EV_INFO << "📥 RSU received TaskResultMessage with completion data" << endl;
        handleTaskResultWithCompletion(taskResult);
        return;
    }

    // ========================================================================
    // NEW: Intercept and Log Task Lifecycle Events to Redis Stream
    // ========================================================================
    // This catches lifecycle event messages from vehicles and writes them to
    // the Redis stream for dashboard visualization (non-destructive addition)
    veins::TaskOffloadingEvent* lifecycleEvent =
        dynamic_cast<veins::TaskOffloadingEvent*>(wsm);
    if (lifecycleEvent) {
        EV_INFO << "📊 Intercepted lifecycle event: " << lifecycleEvent->getEvent_type()
                << " for task " << lifecycleEvent->getTask_id() << endl;

        if (redis_twin && use_redis) {
            // Write to Redis stream for dashboard
            redis_twin->appendTaskLifecycleEvent(
                lifecycleEvent->getTask_id(),
                lifecycleEvent->getEvent_type(),
                lifecycleEvent->getEvent_time(),
                lifecycleEvent->getSource_entity_id(),
                lifecycleEvent->getTarget_entity_id(),
                lifecycleEvent->getEvent_details()
            );
        }

        delete lifecycleEvent;
        return;
    }

    // Original DemoSafetyMessage handling (regular vehicle beacons)
    DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
    if(!dsm) {
        // Non-DSM frames can still arrive on this path; drop safely.
        EV_WARN << "MyRSUApp: Dropping non-DSM frame: " << wsm->getClassName() << endl;
        delete wsm;
        return;
    }
    
    if (terminalVerboseLogs) {
        std::cout << "CONSOLE: MyRSUApp - ✓ Message IS DemoSafetyMessage" << std::endl;
    }
    
    // Shadowing diagnostics intentionally muted to avoid high-volume console noise.
    
    // Get payload from message name
    const char* nm = dsm->getName();
    std::string payload = nm ? std::string(nm) : std::string();
    
    if (terminalVerboseLogs) {
        std::cout << "CONSOLE: MyRSUApp - Raw payload length: " << payload.length() << std::endl;
        std::cout << "CONSOLE: MyRSUApp - Raw payload: '" << payload << "'" << std::endl;
    }
    
    EV << "RSU: Received message from vehicle at time " << simTime() << endl;
    
    // Note: Vehicle telemetry (position, speed) now comes with VehicleResourceStatusMessage
    // This onWSM() handler primarily receives regular beacons
    
    if (terminalVerboseLogs) {
        std::cout << "***** MyRSUApp - onWSM() COMPLETED *****\n" << std::endl;
    }

    // Explicit ownership model: onWSM path must release any frame that was
    // handled locally and not forwarded via sendDown().
    delete wsm;
}

void MyRSUApp::finish() {
    // Block handleTaskMetadata() from rescheduling checkDecisionMsg during teardown.
    isFinishing = true;

    // Log final Digital Twin statistics
    logDigitalTwinState();

    // Close Redis connection
    if (redis_twin) {
        EV_INFO << "Closing Redis Digital Twin connection..." << endl;
        delete redis_twin;
        redis_twin = nullptr;
    }
    
    EV << "MyRSUApp: stopping RSUHttpPoster...\n";
    // Null self-message pointers — deleteModule() handles FES cleanup.
    // Calling cancelAndDelete/cancelEvent here risks crashing in
    // cSoftOwner::yieldOwnership if ownership was disturbed by prior signal
    // emissions (traciModuleRemovedSignal).  Same safe pattern as PayloadVehicleApp.
    beaconMsg               = nullptr;
    rsu_status_update_timer = nullptr;
    rsu_broadcast_timer     = nullptr;
    checkDecisionMsg        = nullptr;
    progressMsg_            = nullptr;

    // Null completion_event pointers for in-flight RSU tasks — deleteModule()
    // will drain the FES, so calling cancelEvent here is unsafe.
    for (auto& kv : rsuPendingTasks) {
        kv.second.completion_event = nullptr;
    }
    rsuPendingTasks.clear();
    rsuWaitingQueue.clear();

    // Legacy task structures: null completion_msg pointers (deleteModule handles FES).
    for (auto& kv : active_tasks) {
        kv.second.completion_msg = nullptr;
    }
    active_tasks.clear();
    task_queue.clear();

    // Clear in-memory DT and orchestration state.
    pending_offloading_requests.clear();
    neighbor_rsus.clear();
    vehicle_twins.clear();
    pending_decision_ids_.clear();
    task_records.clear();
    vehicle_coverage_records.clear();
    secondary_last_export_time.clear();

    // Cancel and delete secondary cycle timer
    if (secondary_cycle_timer) {
        cancelAndDelete(secondary_cycle_timer);
        secondary_cycle_timer = nullptr;
    }

    EV << "MyRSUApp: finished\n";
    DemoBaseApplLayer::finish();
}

void MyRSUApp::initializeSecondaryCycleWorker() {
    if (!secondary_link_context_export) {
        return;
    }

    if (!secondary_cycle_timer) {
        secondary_cycle_timer = new cMessage("secondaryCycle");
    }
    if (!secondary_cycle_timer->isScheduled()) {
        scheduleAt(simTime() + secondary_cycle_interval_s, secondary_cycle_timer);
    }

    EV_INFO << "Secondary cycle worker started: interval=" << secondary_cycle_interval_s
            << "s candidate_radius=" << secondary_candidate_radius_m << "m" << endl;
}

void MyRSUApp::loadObstaclePolygons() {
    // Attenuation values match SimpleObstacleShadowing entries in config.xml
    const std::map<std::string, std::pair<double, double>> type_att = {
        {"building",            {2.0,  0.1}},
        {"office_building",     {18.0, 0.9}},
        {"apartment_complex",   {12.0, 0.6}},
        {"single_house",        {8.0,  0.4}},
        {"warehouse",           {6.0,  0.3}},
        {"shopping_mall",       {15.0, 0.75}},
        {"urban_park",          {4.0,  0.2}},
        {"street_trees",        {2.0,  0.1}},
        {"parking_garage",      {10.0, 0.5}},
        {"road_infrastructure", {14.0, 0.7}},
    };

    cXMLElement* root = nullptr;
    try {
        root = getEnvir()->getXMLDocument("erlangen.poly.xml");
    } catch (...) {
        EV_WARN << "loadObstaclePolygons: could not open erlangen.poly.xml" << endl;
        return;
    }
    if (!root) return;

    cXMLElement* shapes = root->getFirstChildWithTag("shapes");
    if (!shapes) shapes = root;

    for (cXMLElement* poly = shapes->getFirstChildWithTag("poly");
         poly; poly = poly->getNextSiblingWithTag("poly")) {
        const char* type_c  = poly->getAttribute("type");
        const char* shape_c = poly->getAttribute("shape");
        if (!type_c || !shape_c) continue;

        const auto it = type_att.find(type_c);
        if (it == type_att.end()) continue;

        ObstaclePolygon p;
        p.db_per_cut   = it->second.first;
        p.db_per_meter = it->second.second;
        p.aabb_min_x = p.aabb_min_y =  1e18;
        p.aabb_max_x = p.aabb_max_y = -1e18;

        std::istringstream ss(shape_c);
        std::string token;
        while (ss >> token) {
            const size_t comma = token.find(',');
            if (comma == std::string::npos) continue;
            const double x = std::stod(token.substr(0, comma));
            const double y = std::stod(token.substr(comma + 1));
            p.vertices.emplace_back(x, y);
            p.aabb_min_x = std::min(p.aabb_min_x, x);
            p.aabb_min_y = std::min(p.aabb_min_y, y);
            p.aabb_max_x = std::max(p.aabb_max_x, x);
            p.aabb_max_y = std::max(p.aabb_max_y, y);
        }
        if (p.vertices.size() >= 3) {
            secondary_obstacle_polygons.push_back(std::move(p));
        }
    }
    EV_INFO << "Loaded " << secondary_obstacle_polygons.size()
            << " obstacle polygons for secondary SINR evaluation" << endl;
}

double MyRSUApp::computeObstacleLossDb(double tx_x, double tx_y, double rx_x, double rx_y) const {
    const double dx = rx_x - tx_x;
    const double dy = rx_y - tx_y;
    const double ray_len = std::sqrt(dx * dx + dy * dy);
    if (ray_len < 1e-9) return 0.0;

    const double ray_min_x = std::min(tx_x, rx_x);
    const double ray_max_x = std::max(tx_x, rx_x);
    const double ray_min_y = std::min(tx_y, rx_y);
    const double ray_max_y = std::max(tx_y, rx_y);

    double total_loss = 0.0;
    for (const auto& poly : secondary_obstacle_polygons) {
        if (poly.aabb_max_x < ray_min_x || poly.aabb_min_x > ray_max_x) continue;
        if (poly.aabb_max_y < ray_min_y || poly.aabb_min_y > ray_max_y) continue;

        std::vector<double> ts;
        const int n = static_cast<int>(poly.vertices.size());
        for (int i = 0; i < n; ++i) {
            const int j = (i + 1) % n;
            const double ax = poly.vertices[i].first;
            const double ay = poly.vertices[i].second;
            const double ex = poly.vertices[j].first - ax;
            const double ey = poly.vertices[j].second - ay;
            // Cramer's rule: det = ex*dy - dx*ey
            const double det = ex * dy - dx * ey;
            if (std::abs(det) < 1e-12) continue;
            const double fx = ax - tx_x;
            const double fy = ay - tx_y;
            const double t = (ex * fy - ey * fx) / det;
            const double u = (dx * fy - dy * fx) / det;
            if (t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0) {
                ts.push_back(t);
            }
        }
        if (ts.size() < 2) continue;

        std::sort(ts.begin(), ts.end());
        const auto last = std::unique(ts.begin(), ts.end(),
            [](double a, double b) { return std::abs(a - b) < 1e-9; });
        ts.erase(last, ts.end());
        if (ts.size() < 2) continue;

        const int cuts = static_cast<int>(ts.size()) / 2;
        const double inside_len = (ts.back() - ts.front()) * ray_len;
        total_loss += cuts * poly.db_per_cut + inside_len * poly.db_per_meter;
    }
    return total_loss;
}

double MyRSUApp::estimateSinrDbWithObstacles(double tx_x, double tx_y, double rx_x, double rx_y, double dist,
                                             double vehicle_shadow_loss_db,
                                             double lognormal_shadow_db,
                                             double nakagami_fading_db,
                                             double interference_mw) const {
    constexpr double tx_power_dbm    = 33.0;   // 2000 mW per omnetpp.ini txPower
    constexpr double frequency_hz    = 5.89e9;
    constexpr double pathloss_alpha  = 2.2;    // Matches config.xml SimplePathlossModel alpha
    constexpr double noise_floor_dbm = -110.0;

    const double d_m            = std::max(1.0, dist);
    const double lambda         = 299792458.0 / frequency_hz;
    const double path_loss_db   = 20.0 * std::log10(4.0 * M_PI / lambda)
                                  + 10.0 * pathloss_alpha * std::log10(d_m);
    const double obstacle_loss  = computeObstacleLossDb(tx_x, tx_y, rx_x, rx_y);
    const double rx_power_dbm   = tx_power_dbm - path_loss_db - obstacle_loss
                                  - std::max(0.0, vehicle_shadow_loss_db) - lognormal_shadow_db
                                  + nakagami_fading_db;
    const double signal_mw      = std::pow(10.0, rx_power_dbm / 10.0);
    const double noise_mw       = std::pow(10.0, noise_floor_dbm / 10.0);
    // Backward-compatible fallback when no explicit per-link interference is provided.
    const double fallback_interference_mw = std::max(0.0, secondary_interference_mw) * noise_mw;
    const double effective_interference_mw = (interference_mw >= 0.0)
        ? interference_mw
        : fallback_interference_mw;
    const double sinr_linear    = signal_mw / std::max(1e-15, noise_mw + effective_interference_mw);
    const double sinr_db        = 10.0 * std::log10(std::max(1e-15, sinr_linear));

    return std::max(-30.0, std::min(80.0, sinr_db));
}

void MyRSUApp::runSecondaryCycle() {
    if (!secondary_link_context_export) {
        return;
    }
    if (!secondary_use_external_predictions && !secondary_predictor) {
        return;
    }

    std::vector<PredictorInputState> inputs;
    inputs.reserve(vehicle_twins.size());

    if (secondary_use_external_predictions) {
        if (redis_twin && use_redis) {
            const int64_t latest_cycle = redis_twin->getLatestSecondaryPredictionCycle(secondary_run_id);
            if (latest_cycle >= 0) {
                const auto points = redis_twin->getSecondaryPredictedPoints(secondary_run_id, static_cast<uint64_t>(latest_cycle));
                secondary_cycle_predictions.clear();
                secondary_cycle_candidates.clear();

                for (const auto& point : points) {
                    PredictedVehicleState st;
                    st.vehicle_id = point.vehicle_id;
                    st.step_index = point.step_index;
                    st.sim_time = point.predicted_time;
                    st.pos_x = point.pos_x;
                    st.pos_y = point.pos_y;
                    st.speed = point.speed;
                    st.heading_deg = point.heading;
                    st.acceleration = point.acceleration;
                    secondary_cycle_predictions[point.vehicle_id].push_back(st);
                }

                for (auto& kv : secondary_cycle_predictions) {
                    auto& traj = kv.second;
                    std::sort(traj.begin(), traj.end(), [](const PredictedVehicleState& a, const PredictedVehicleState& b) {
                        return a.step_index < b.step_index;
                    });
                }
            }
        }

        if (secondary_cycle_predictions.empty()) {
            EV_WARN << "Secondary cycle skipped: no external predicted trajectories available in Redis" << endl;
            return;
        }
    } else {
        if (redis_twin && use_redis) {
            const auto snapshots = redis_twin->getAllVehicles();
            inputs.reserve(snapshots.size());
            for (const auto& snap : snapshots) {
                PredictorInputState in;
                in.vehicle_id = snap.vehicle_id;
                in.source_timestamp = snap.source_timestamp > 0.0 ? snap.source_timestamp : simTime().dbl();
                in.pos_x = snap.pos_x;
                in.pos_y = snap.pos_y;
                in.speed = snap.speed;
                in.heading_deg = snap.heading;
                in.acceleration = snap.acceleration;
                inputs.push_back(in);
            }
        }

        if (inputs.empty()) {
            for (const auto& kv : vehicle_twins) {
                const auto& twin = kv.second;
                PredictorInputState in;
                in.vehicle_id = twin.vehicle_id;
                in.source_timestamp = simTime().dbl();
                in.pos_x = twin.pos_x;
                in.pos_y = twin.pos_y;
                in.speed = twin.speed;
                in.heading_deg = twin.heading;
                in.acceleration = 0.0;
                inputs.push_back(in);
            }
        }
    }

    if (!secondary_use_external_predictions) {
        secondary_cycle_predictions.clear();
        secondary_cycle_candidates.clear();
    }
    std::map<std::string, PredictedVehicleState> first_step_state;

    if (secondary_use_external_predictions) {
        for (const auto& kv : secondary_cycle_predictions) {
            if (!kv.second.empty()) {
                first_step_state[kv.first] = kv.second.front();
            }
        }
    } else {
        for (const auto& input : inputs) {
            std::vector<PredictedVehicleState> predicted;
            predicted = secondary_predictor->predict(
                input,
                secondary_prediction_horizon_s,
                secondary_prediction_step_s
            );
            if (predicted.empty()) {
                continue;
            }
            first_step_state[input.vehicle_id] = predicted.front();
            secondary_cycle_predictions.emplace(input.vehicle_id, std::move(predicted));
        }
    }

    for (const auto& kv : first_step_state) {
        const std::string& src_id = kv.first;
        const auto& src = kv.second;

        SecondaryCandidateSet set;
        for (const auto& rsu : rsu_static_contexts) {
            const double dx = src.pos_x - rsu.pos_x;
            const double dy = src.pos_y - rsu.pos_y;
            const double dist = std::sqrt(dx * dx + dy * dy);
            if (dist <= secondary_candidate_radius_m) {
                set.rsu_ids.push_back(rsu.rsu_id);
            }
        }

        for (const auto& other : first_step_state) {
            if (other.first == src_id) {
                continue;
            }
            const double dx = src.pos_x - other.second.pos_x;
            const double dy = src.pos_y - other.second.pos_y;
            const double dist = std::sqrt(dx * dx + dy * dy);
            if (dist <= secondary_candidate_radius_m) {
                set.vehicle_ids.push_back(other.first);
            }
        }

        secondary_cycle_candidates.emplace(src_id, std::move(set));
    }

    const uint64_t cycle_id = secondary_cycle_index + 1;
    int q_entry_count = 0;
    std::map<std::string, RsuStaticContext> rsu_by_id;
    for (const auto& rsu : rsu_static_contexts) {
        rsu_by_id[rsu.rsu_id] = rsu;
    }

    // Index predicted vehicle states by step to evaluate dynamic vehicle blocking per snapshot.
    std::map<int, std::vector<const PredictedVehicleState*>> step_vehicle_states;
    for (const auto& kv : secondary_cycle_predictions) {
        for (const auto& st : kv.second) {
            step_vehicle_states[st.step_index].push_back(&st);
        }
    }

    struct VehicleDims {
        double len_m = 4.8;
        double wid_m = 1.9;
        double hgt_m = 1.5;
    };
    std::map<std::string, VehicleDims> vehicle_dims_cache;
    static std::unordered_set<std::string> missing_vehicle_dims_cache;

    auto getVehicleDims = [&](const std::string& vehicle_id) {
        auto it = vehicle_dims_cache.find(vehicle_id);
        if (it != vehicle_dims_cache.end()) return it->second;

        if (missing_vehicle_dims_cache.count(vehicle_id) != 0) {
            VehicleDims dims;
            vehicle_dims_cache[vehicle_id] = dims;
            return dims;
        }

        VehicleDims dims;
        try {
            auto* manager = TraCIScenarioManagerAccess().get();
            if (manager && manager->isConnected() && manager->getCommandInterface()) {
                auto veh = manager->getCommandInterface()->vehicle(vehicle_id);
                const double l = veh.getLength();
                const double w = veh.getWidth();
                const double h = veh.getHeight();
                if (std::isfinite(l) && l > 0.5) dims.len_m = l;
                if (std::isfinite(w) && w > 0.3) dims.wid_m = w;
                if (std::isfinite(h) && h > 0.5) dims.hgt_m = h;
            }
        }
        catch (const cRuntimeError&) {
            missing_vehicle_dims_cache.insert(vehicle_id);
            // Fallback to defaults when a vehicle disappears between snapshots.
        }
        catch (...) {
            missing_vehicle_dims_cache.insert(vehicle_id);
            // Keep defaults on any TraCI lookup error.
        }

        vehicle_dims_cache[vehicle_id] = dims;
        return dims;
    };

    auto estimateVehicleShadowLossDb = [&](double tx_x,
                                           double tx_y,
                                           double rx_x,
                                           double rx_y,
                                           int step_index,
                                           const std::string& tx_id,
                                           const std::string& rx_id) {
        const auto it = step_vehicle_states.find(step_index);
        if (it == step_vehicle_states.end()) return 0.0;

        const double dx = rx_x - tx_x;
        const double dy = rx_y - tx_y;
        const double d = std::sqrt(dx * dx + dy * dy);
        if (d < 1e-6) return 0.0;

        constexpr double freq_hz = 5.89e9;
        constexpr double c_mps = 299792458.0;
        constexpr double lambda_m = c_mps / freq_hz;
        constexpr double tx_h_m = 1.5;
        constexpr double rx_h_m = 1.5;
        constexpr double default_vehicle_h_m = 1.5;

        struct BlockerInfo {
            double d1 = 0.0;
            double h = default_vehicle_h_m;
            double len_m = 4.8;
            double wid_m = 1.9;
        };

        auto cross2d = [](double ax, double ay, double bx, double by) {
            return ax * by - ay * bx;
        };

        auto segIntersects = [&](double ax, double ay, double bx, double by,
                                 double cx, double cy, double dx2, double dy2) {
            const double r_x = bx - ax;
            const double r_y = by - ay;
            const double s_x = dx2 - cx;
            const double s_y = dy2 - cy;
            const double denom = cross2d(r_x, r_y, s_x, s_y);
            if (std::fabs(denom) < 1e-12) return false;
            const double qmp_x = cx - ax;
            const double qmp_y = cy - ay;
            const double t = cross2d(qmp_x, qmp_y, s_x, s_y) / denom;
            const double u = cross2d(qmp_x, qmp_y, r_x, r_y) / denom;
            return t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0;
        };

        auto lineIntersectsVehicleRect = [&](const PredictedVehicleState& st) {
            const VehicleDims dims = getVehicleDims(st.vehicle_id);
            const double heading_rad = st.heading_deg * M_PI / 180.0;
            const double hx = std::cos(heading_rad);
            const double hy = std::sin(heading_rad);
            const double wx = -hy;
            const double wy = hx;
            const double hl = 0.5 * dims.len_m;
            const double hw = 0.5 * dims.wid_m;

            const double c0x = st.pos_x + hx * hl + wx * hw;
            const double c0y = st.pos_y + hy * hl + wy * hw;
            const double c1x = st.pos_x + hx * hl - wx * hw;
            const double c1y = st.pos_y + hy * hl - wy * hw;
            const double c2x = st.pos_x - hx * hl - wx * hw;
            const double c2y = st.pos_y - hy * hl - wy * hw;
            const double c3x = st.pos_x - hx * hl + wx * hw;
            const double c3y = st.pos_y - hy * hl + wy * hw;

            return segIntersects(tx_x, tx_y, rx_x, rx_y, c0x, c0y, c1x, c1y) ||
                   segIntersects(tx_x, tx_y, rx_x, rx_y, c1x, c1y, c2x, c2y) ||
                   segIntersects(tx_x, tx_y, rx_x, rx_y, c2x, c2y, c3x, c3y) ||
                   segIntersects(tx_x, tx_y, rx_x, rx_y, c3x, c3y, c0x, c0y);
        };

        auto knifeEdgeLossDb = [&](double d1, double obs_h) {
            if (d1 <= 1e-6 || d1 >= d - 1e-6) return 0.0;
            const double d2 = d - d1;
            const double y = ((rx_h_m - tx_h_m) / d) * d1 + tx_h_m;
            const double H = obs_h - y;
            const double r1 = std::sqrt(lambda_m * d1 * d2 / d);
            if (r1 <= 1e-9) return 0.0;
            const double nu = std::sqrt(2.0) * H / r1;
            if (nu <= -0.7) return 0.0;
            const double inside = std::sqrt((nu - 0.1) * (nu - 0.1) + 1.0) + nu - 0.1;
            if (inside <= 1e-12) return 0.0;
            return 6.9 + 20.0 * std::log10(inside);
        };

        std::vector<BlockerInfo> blockers;
        blockers.reserve(it->second.size());
        for (const auto* st : it->second) {
            if (!st) continue;
            if (st->vehicle_id == tx_id || st->vehicle_id == rx_id) continue;

            const double px = st->pos_x - tx_x;
            const double py = st->pos_y - tx_y;
            const double t = (px * dx + py * dy) / (d * d);
            if (t <= 0.01 || t >= 0.99) continue;
            if (!lineIntersectsVehicleRect(*st)) continue;

            BlockerInfo bi;
            const VehicleDims dims = getVehicleDims(st->vehicle_id);
            bi.d1 = t * d;
            bi.h = dims.hgt_m;
            bi.len_m = dims.len_m;
            bi.wid_m = dims.wid_m;
            blockers.push_back(bi);
        }

        if (blockers.empty()) return 0.0;

        std::sort(blockers.begin(), blockers.end(), [](const BlockerInfo& a, const BlockerInfo& b) {
            return a.d1 < b.d1;
        });

        // Veins-style diffraction trend: knife-edge loss per blocker.
        double total_loss_db = 0.0;
        for (const auto& b : blockers) {
            total_loss_db += knifeEdgeLossDb(b.d1, b.h);
        }

        // Multi-obstacle correction term inspired by VehicleObstacleShadowing DZ model.
        if (blockers.size() >= 2) {
            std::vector<double> seg;
            seg.reserve(blockers.size() + 1);
            double prev = 0.0;
            for (const auto& b : blockers) {
                const double s = std::max(1e-6, b.d1 - prev);
                seg.push_back(s);
                prev = b.d1;
            }
            seg.push_back(std::max(1e-6, d - prev));

            double prod_s = 1.0;
            double sum_s = 0.0;
            for (double s : seg) {
                prod_s *= s;
                sum_s += s;
            }

            double prod_pair = 1.0;
            for (size_t i = 1; i < seg.size(); ++i) {
                prod_pair *= (seg[i] + seg[i - 1]);
            }

            const double denom = prod_pair * seg.front() * seg.back();
            const double numer = prod_s * sum_s;
            if (numer > 1e-18 && denom > 1e-18) {
                const double ratio = numer / denom;
                if (ratio > 1e-12) {
                    total_loss_db += std::max(0.0, -10.0 * std::log10(ratio));
                }
            }
        }

        return std::min(30.0, total_loss_db);
    };

    auto estimateLogNormalShadowDb = [&](double tx_x,
                                         double tx_y,
                                         double rx_x,
                                         double rx_y,
                                         int step_index,
                                         const std::string& tx_id,
                                         const std::string& rx_id) {
        constexpr double sigma_db = 6.0;
        constexpr double corr_distance_m = 50.0;
        if (sigma_db <= 0.0) return 0.0;

        const double mid_x = 0.5 * (tx_x + rx_x);
        const double mid_y = 0.5 * (tx_y + rx_y);
        const long long cell_x = static_cast<long long>(std::floor(mid_x / corr_distance_m));
        const long long cell_y = static_cast<long long>(std::floor(mid_y / corr_distance_m));

        const std::string a = (tx_id < rx_id) ? tx_id : rx_id;
        const std::string b = (tx_id < rx_id) ? rx_id : tx_id;
        uint64_t seed = static_cast<uint64_t>(std::hash<std::string>{}(a + "|" + b));
        seed ^= static_cast<uint64_t>(cell_x) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        seed ^= static_cast<uint64_t>(cell_y) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        seed ^= static_cast<uint64_t>(step_index / 2) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);

        auto splitmix64 = [](uint64_t& x) {
            x += 0x9e3779b97f4a7c15ULL;
            uint64_t z = x;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            return z ^ (z >> 31);
        };

        const double u1 = std::max(1e-12, (splitmix64(seed) + 1.0) / 18446744073709551616.0);
        const double u2 = (splitmix64(seed) + 1.0) / 18446744073709551616.0;
        const double z = std::sqrt(-2.0 * std::log(u1)) * std::cos(6.283185307179586 * u2);
        return sigma_db * z;
    };

    auto estimateNakagamiFadingDb = [&](double tx_x,
                                        double tx_y,
                                        double rx_x,
                                        double rx_y,
                                        int step_index,
                                        const std::string& tx_id,
                                        const std::string& rx_id) {
        if (!secondary_nakagami_enabled) return 0.0;
        const double m = std::max(1e-3, secondary_nakagami_m);
        const double cell_size_m = std::max(0.1, secondary_nakagami_cell_size_m);
        if (m <= 0.0) return 0.0;

        const std::string a = (tx_id < rx_id) ? tx_id : rx_id;
        const std::string b = (tx_id < rx_id) ? rx_id : tx_id;

        // Small-scale fading decorrelation cell size is runtime-configurable.
        const double mid_x = 0.5 * (tx_x + rx_x);
        const double mid_y = 0.5 * (tx_y + rx_y);
        const long long cell_x = static_cast<long long>(std::floor(mid_x / cell_size_m));
        const long long cell_y = static_cast<long long>(std::floor(mid_y / cell_size_m));

        uint64_t seed = static_cast<uint64_t>(std::hash<std::string>{}(a + "|" + b + "|nakagami"));
        seed ^= static_cast<uint64_t>(cell_x) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        seed ^= static_cast<uint64_t>(cell_y) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        seed ^= static_cast<uint64_t>(step_index) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);

        std::mt19937_64 rng(seed);
        // Nakagami power gain follows Gamma(shape=m, scale=1/m), E[gain]=1.
        std::gamma_distribution<double> gamma_dist(m, 1.0 / m);
        const double gain_linear = std::max(1e-12, gamma_dist(rng));
        const double fading_db = 10.0 * std::log10(std::max(1e-12, gain_linear));
        return std::max(-30.0, std::min(30.0, fading_db));
    };

    auto estimateInterferenceMw = [&](double rx_x,
                                      double rx_y,
                                      int step_index,
                                      const std::string& desired_tx_id,
                                      const std::string& desired_rx_id) {
        const auto it = step_vehicle_states.find(step_index);
        if (it == step_vehicle_states.end()) return 0.0;

        // secondary_interference_mw now acts as activity ratio (e.g. 0.09 => 9%).
        const double activity_ratio = std::max(0.0, std::min(1.0, secondary_interference_mw));
        if (activity_ratio <= 0.0) return 0.0;

        constexpr double tx_power_dbm = 33.0;
        constexpr double frequency_hz = 5.89e9;
        constexpr double pathloss_alpha = 2.2;
        const double lambda = 299792458.0 / frequency_hz;
        double total_interference_mw = 0.0;

        for (const auto* interferer : it->second) {
            if (!interferer) continue;
            const std::string& interferer_id = interferer->vehicle_id;
            if (interferer_id == desired_tx_id || interferer_id == desired_rx_id) continue;

            // "Cars in the area": only nearby interferers around the receiver are considered.
            const double dx = interferer->pos_x - rx_x;
            const double dy = interferer->pos_y - rx_y;
            const double dist = std::sqrt(dx * dx + dy * dy);
            if (dist > secondary_candidate_radius_m) continue;

            // Deterministic random activation per slot: pick ~9% (activity_ratio) of nearby cars.
            const std::string sample_key = std::to_string(cycle_id) + "|" + std::to_string(step_index) +
                                           "|" + desired_rx_id + "|" + interferer_id;
            const uint64_t h = static_cast<uint64_t>(std::hash<std::string>{}(sample_key));
            const double u = static_cast<double>(h & 0xffffffffULL) / 4294967295.0;
            if (u >= activity_ratio) continue;

            const double d_m = std::max(1.0, dist);
            const double path_loss_db = 20.0 * std::log10(4.0 * M_PI / lambda)
                                        + 10.0 * pathloss_alpha * std::log10(d_m);
            const double obstacle_loss_db = computeObstacleLossDb(interferer->pos_x, interferer->pos_y, rx_x, rx_y);
            const double veh_shadow_db = estimateVehicleShadowLossDb(
                interferer->pos_x,
                interferer->pos_y,
                rx_x,
                rx_y,
                step_index,
                interferer_id,
                desired_rx_id
            );
            const double lognormal_db = estimateLogNormalShadowDb(
                interferer->pos_x,
                interferer->pos_y,
                rx_x,
                rx_y,
                step_index,
                interferer_id,
                desired_rx_id
            );
            const double nakagami_db = estimateNakagamiFadingDb(
                interferer->pos_x,
                interferer->pos_y,
                rx_x,
                rx_y,
                step_index,
                interferer_id,
                desired_rx_id
            );

            const double int_rx_dbm = tx_power_dbm - path_loss_db - obstacle_loss_db
                                      - std::max(0.0, veh_shadow_db) - lognormal_db
                                      + nakagami_db;
            total_interference_mw += std::pow(10.0, int_rx_dbm / 10.0);
        }

        return total_interference_mw;
    };

    auto emitQEntry = [&](const std::string& link_type,
                          const std::string& tx_id,
                          const std::string& rx_id,
                          int step_index,
                          double predicted_time,
                          double tx_x,
                          double tx_y,
                          double rx_x,
                          double rx_y,
                          double dist,
                          double sinr_db) {
        insertSecondaryQEntry(
            cycle_id,
            link_type,
            tx_id,
            rx_id,
            step_index,
            predicted_time,
            tx_x,
            tx_y,
            rx_x,
            rx_y,
            dist,
            sinr_db
        );

        if (secondary_q_publish_enabled && redis_twin && use_redis) {
            redis_twin->pushSecondaryQEntry(
                secondary_run_id,
                cycle_id,
                link_type,
                tx_id,
                rx_id,
                step_index,
                predicted_time,
                tx_x,
                tx_y,
                rx_x,
                rx_y,
                dist,
                sinr_db,
                secondary_q_stream_maxlen
            );
        }
        q_entry_count++;
    };

    for (const auto& kv : secondary_cycle_candidates) {
        const std::string& src_id = kv.first;
        const auto pred_it = secondary_cycle_predictions.find(src_id);
        if (pred_it == secondary_cycle_predictions.end()) {
            continue;
        }

        const auto& src_traj = pred_it->second;
        const auto& candidates = kv.second;

        for (const auto& rsu_id : candidates.rsu_ids) {
            const auto rsu_it = rsu_by_id.find(rsu_id);
            if (rsu_it == rsu_by_id.end()) {
                continue;
            }
            const auto& rsu = rsu_it->second;

            for (const auto& step : src_traj) {
                const double dx = step.pos_x - rsu.pos_x;
                const double dy = step.pos_y - rsu.pos_y;
                const double dist = std::sqrt(dx * dx + dy * dy);
                const double veh_shadow_db = estimateVehicleShadowLossDb(
                    step.pos_x, step.pos_y, rsu.pos_x, rsu.pos_y, step.step_index, src_id, rsu_id);
                const double lognormal_db = estimateLogNormalShadowDb(
                    step.pos_x, step.pos_y, rsu.pos_x, rsu.pos_y, step.step_index, src_id, rsu_id);
                const double nakagami_db = estimateNakagamiFadingDb(
                    step.pos_x, step.pos_y, rsu.pos_x, rsu.pos_y, step.step_index, src_id, rsu_id);
                const double interference_mw = estimateInterferenceMw(
                    rsu.pos_x,
                    rsu.pos_y,
                    step.step_index,
                    src_id,
                    rsu_id
                );
                const double sinr_db = estimateSinrDbWithObstacles(
                    step.pos_x,
                    step.pos_y,
                    rsu.pos_x,
                    rsu.pos_y,
                    dist,
                    veh_shadow_db,
                    lognormal_db,
                    nakagami_db,
                    interference_mw
                );
                if (sinr_db < secondary_sinr_threshold_db) {
                    continue;
                }

                emitQEntry(
                    "V2RSU",
                    src_id,
                    rsu_id,
                    step.step_index,
                    step.sim_time,
                    step.pos_x,
                    step.pos_y,
                    rsu.pos_x,
                    rsu.pos_y,
                    dist,
                    sinr_db
                );
                emitQEntry(
                    "RSU2V",
                    rsu_id,
                    src_id,
                    step.step_index,
                    step.sim_time,
                    rsu.pos_x,
                    rsu.pos_y,
                    step.pos_x,
                    step.pos_y,
                    dist,
                    sinr_db
                );
            }
        }

        for (const auto& peer_id : candidates.vehicle_ids) {
            const auto peer_it = secondary_cycle_predictions.find(peer_id);
            if (peer_it == secondary_cycle_predictions.end()) {
                continue;
            }
            const auto& peer_traj = peer_it->second;
            const int common_steps = std::min(static_cast<int>(src_traj.size()), static_cast<int>(peer_traj.size()));

            for (int i = 0; i < common_steps; ++i) {
                const auto& src_step = src_traj[i];
                const auto& peer_step = peer_traj[i];
                const double dx = src_step.pos_x - peer_step.pos_x;
                const double dy = src_step.pos_y - peer_step.pos_y;
                const double dist = std::sqrt(dx * dx + dy * dy);
                const double veh_shadow_db = estimateVehicleShadowLossDb(
                    src_step.pos_x, src_step.pos_y, peer_step.pos_x, peer_step.pos_y,
                    src_step.step_index, src_id, peer_id);
                const double lognormal_db = estimateLogNormalShadowDb(
                    src_step.pos_x, src_step.pos_y, peer_step.pos_x, peer_step.pos_y,
                    src_step.step_index, src_id, peer_id);
                const double nakagami_db = estimateNakagamiFadingDb(
                    src_step.pos_x, src_step.pos_y, peer_step.pos_x, peer_step.pos_y,
                    src_step.step_index, src_id, peer_id);
                const double interference_mw = estimateInterferenceMw(
                    peer_step.pos_x,
                    peer_step.pos_y,
                    src_step.step_index,
                    src_id,
                    peer_id
                );
                const double sinr_db = estimateSinrDbWithObstacles(
                    src_step.pos_x, src_step.pos_y, peer_step.pos_x, peer_step.pos_y,
                    dist,
                    veh_shadow_db,
                    lognormal_db,
                    nakagami_db,
                    interference_mw
                );
                if (sinr_db < secondary_sinr_threshold_db) {
                    continue;
                }

                emitQEntry(
                    "V2V",
                    src_id,
                    peer_id,
                    src_step.step_index,
                    src_step.sim_time,
                    src_step.pos_x,
                    src_step.pos_y,
                    peer_step.pos_x,
                    peer_step.pos_y,
                    dist,
                    sinr_db
                );
                emitQEntry(
                    "V2V",
                    peer_id,
                    src_id,
                    src_step.step_index,
                    src_step.sim_time,
                    peer_step.pos_x,
                    peer_step.pos_y,
                    src_step.pos_x,
                    src_step.pos_y,
                    dist,
                    sinr_db
                );
            }
        }
    }

    secondary_cycle_index = cycle_id;
    insertSecondaryQCycle(
        cycle_id,
        simTime().dbl(),
        static_cast<int>(secondary_cycle_predictions.size()),
        static_cast<int>(secondary_cycle_candidates.size()),
        q_entry_count
    );
    if (secondary_q_publish_enabled && redis_twin && use_redis) {
        redis_twin->updateSecondaryQCycle(
            secondary_run_id,
            cycle_id,
            simTime().dbl(),
            secondary_prediction_horizon_s,
            secondary_prediction_step_s,
            secondary_sinr_threshold_db,
            static_cast<int>(secondary_cycle_predictions.size()),
            q_entry_count,
            secondary_q_ttl_s
        );
    }
    EV_INFO << "Secondary cycle #" << secondary_cycle_index
            << " generated " << secondary_cycle_predictions.size() << " trajectories and "
            << secondary_cycle_candidates.size() << " candidate sets, published "
            << q_entry_count << " directional q entries" << endl;
}

void MyRSUApp::handleMessage(cMessage* msg) {
    // ========================================================================
    // HANDLE OFFLOADING REQUEST MESSAGES
    // ========================================================================
    veins::OffloadingRequestMessage* offloadReq = dynamic_cast<veins::OffloadingRequestMessage*>(msg);
    if (offloadReq) {
        EV_INFO << "Received OffloadingRequestMessage" << endl;
        handleOffloadingRequest(offloadReq);
        delete offloadReq;
        return;
    }
    
    // ========================================================================
    // HANDLE TASK OFFLOAD PACKETS (for RSU processing)
    // ========================================================================
    veins::TaskOffloadPacket* taskPacket = dynamic_cast<veins::TaskOffloadPacket*>(msg);
    if (taskPacket) {
        EV_INFO << "Received TaskOffloadPacket" << endl;
        handleTaskOffloadPacket(taskPacket);
        return;
    }
    
    // ========================================================================
    // HANDLE TASK OFFLOADING LIFECYCLE EVENTS
    // ========================================================================
    veins::TaskOffloadingEvent* offloadEvent = dynamic_cast<veins::TaskOffloadingEvent*>(msg);
    if (offloadEvent) {
        EV_INFO << "Received TaskOffloadingEvent" << endl;
        handleTaskOffloadingEvent(offloadEvent);
        return;
    }

    veins::TaskResultMessage* relayedResult = dynamic_cast<veins::TaskResultMessage*>(msg);
    if (relayedResult) {
        if (isRelayTaskResult(relayedResult)) {
            handleRSUTaskResultRelay(relayedResult);
            return;
        }
    }

    BaseFrame1609_4* wsm = dynamic_cast<BaseFrame1609_4*>(msg);
    if (wsm) {
        onWSM(wsm);
        return;
    }

    DemoBaseApplLayer::handleMessage(msg);
}

// ============================================================================
// DIGITAL TWIN TRACKING IMPLEMENTATION
// ============================================================================

void MyRSUApp::handleTaskMetadata(TaskMetadataMessage* msg) {
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║                   RSU: TASK METADATA RECEIVED                            ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    std::string task_id = msg->getTask_id();
    std::string vehicle_id = msg->getVehicle_id();
    
    // Create task record
    TaskRecord record;
    record.task_id = task_id;
    record.vehicle_id = vehicle_id;
    record.mem_footprint_bytes = msg->getMem_footprint_bytes();
    record.cpu_cycles = msg->getCpu_cycles();
    record.created_time = msg->getCreated_time();
    record.deadline_seconds = msg->getDeadline_seconds();
    record.received_time = simTime().dbl();
    record.qos_value = msg->getQos_value();
    
    // Profile fields
    record.is_profile_task = msg->getIs_profile_task();
    record.task_type_name = msg->getTask_type_name();
    record.task_type_id = msg->getTask_type_id();
    record.input_size_bytes = msg->getInput_size_bytes();
    record.output_size_bytes = msg->getOutput_size_bytes();
    record.is_offloadable = msg->getIs_offloadable();
    record.is_safety_critical = msg->getIs_safety_critical();
    record.priority_level = msg->getPriority_level();
    
    // Store in task records and mark as awaiting a DRL decision.
    task_records[task_id] = record;
    pending_decision_ids_.insert(task_id);
    
    // Update vehicle digital twin
    VehicleDigitalTwin& twin = getOrCreateVehicleTwin(vehicle_id);
    twin.tasks_generated++;
    twin.last_update_time = simTime().dbl();
    
    EV_INFO << "Task metadata stored:" << endl;
    EV_INFO << "  Task ID: " << task_id << endl;
    EV_INFO << "  Vehicle ID: " << vehicle_id << endl;
    EV_INFO << "  Size: " << (record.mem_footprint_bytes / 1024.0) << " KB" << endl;
    EV_INFO << "  CPU Cycles: " << (record.cpu_cycles / 1e9) << " G" << endl;
    EV_INFO << "  Deadline: " << record.deadline_seconds << " sec" << endl;
    EV_INFO << "  QoS: " << record.qos_value << endl;
    if (record.is_profile_task) {
        EV_INFO << "  Task Type: " << record.task_type_name << endl;
        EV_INFO << "  Offloadable: " << (record.is_offloadable ? "Yes" : "No") << endl;
        EV_INFO << "  Safety Critical: " << (record.is_safety_critical ? "Yes" : "No") << endl;
    }
    EV_INFO << "  Received at: " << record.received_time << " (delay: " 
            << (record.received_time - record.created_time) << " sec)" << endl;
    
    logTaskRecord(record, "METADATA_RECEIVED");
    
    // Update Redis with task state and push to request queue for ML model
    if (redis_twin && use_redis) {
        // Create task state
        redis_twin->createTask(
            task_id,
            vehicle_id,
            record.created_time,
            record.deadline_seconds,
            record.task_type_name,
            record.is_offloadable,
            record.is_safety_critical,
            record.priority_level
        );
        
        // Push offloading request to queue for ML model to process (fast Redis queue)
        redis_twin->pushOffloadingRequest(
            task_id,
            vehicle_id,
            "RSU_" + std::to_string(rsu_id),
            record.mem_footprint_bytes,
            record.cpu_cycles,
            record.deadline_seconds,
            record.qos_value,
            record.created_time,
            record.task_type_name,
            record.input_size_bytes,
            record.output_size_bytes,
            record.is_offloadable,
            record.is_safety_critical,
            record.priority_level
        );
        
        std::cout << "REDIS_PUSH: Task " << task_id << " pushed to ML request queue" << std::endl;
    }
    
    // Optional persistence hook (disabled in this build)
    insertTaskMetadata(msg);
    
    std::cout << "DT_TASK: RSU received metadata for task " << task_id 
              << " from vehicle " << vehicle_id << std::endl;

    // Schedule a check for decision (skip if module is being torn down).
    if (!isFinishing) {
        if (!checkDecisionMsg) {
            checkDecisionMsg = new cMessage("checkDecision");
        }
        if (!checkDecisionMsg->isScheduled()) {
            scheduleAt(simTime() + 0.01, checkDecisionMsg);
        }
    }

    // Ownership: this message came from the air and onWSM's caller does not delete it,
    // so we must delete here.
    delete msg;
}

void MyRSUApp::handleTaskCompletion(TaskCompletionMessage* msg) {
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║                   RSU: TASK COMPLETION RECEIVED                          ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    std::string task_id = msg->getTask_id();
    std::string vehicle_id = msg->getVehicle_id();
    
    // Update task record
    auto it = task_records.find(task_id);
    if (it != task_records.end()) {
        TaskRecord& record = it->second;
        record.completed = true;
        result_awaiting_.erase(task_id);
        record.completion_time = msg->getCompletion_time();
        record.processing_time = msg->getProcessing_time();
        record.completed_on_time = msg->getCompleted_on_time();
        
        EV_INFO << "Task completion details:" << endl;
        EV_INFO << "  Task ID: " << task_id << endl;
        EV_INFO << "  Vehicle ID: " << vehicle_id << endl;
        EV_INFO << "  Completion time: " << record.completion_time << endl;
        EV_INFO << "  Processing time: " << record.processing_time << " sec" << endl;
        EV_INFO << "  On time: " << (record.completed_on_time ? "YES" : "NO (LATE)") << endl;
        EV_INFO << "  CPU allocated: " << (msg->getCpu_allocated() / 1e9) << " GHz" << endl;
        
        logTaskRecord(record, record.completed_on_time ? "COMPLETED_ON_TIME" : "COMPLETED_LATE");
        
        // Update Redis task status
        if (redis_twin && use_redis) {
            redis_twin->updateTaskStatus(
                task_id,
                record.completed_on_time ? "COMPLETED_ON_TIME" : "COMPLETED_LATE"
            );

            // Write local_result so Python can collect per-type metrics
            if (msg->getIs_local_execution()) {
                double latency = (msg->getCompletion_time() > 0.0) ? msg->getProcessing_time() : 0.0;
                std::string fail_reason = msg->getFailure_reason();
                // Also override "NONE" for tasks that didn't complete on time (COMPLETED_LATE).
                if (fail_reason.empty() || fail_reason == "NONE") {
                    fail_reason = record.completed_on_time ? "NONE" : "DEADLINE_MISSED";
                }
                redis_twin->writeLocalResult(
                    task_id,
                    msg->getTask_type_name(),
                    msg->getQos_value(),
                    msg->getDeadline_seconds(),
                    record.completed_on_time ? "COMPLETED_ON_TIME" : "FAILED",
                    latency,
                    0.0,   // energy: vehicle does not compute this here
                    fail_reason
                );
                std::cout << "LOCAL_RESULT: task=" << task_id
                          << " type=" << msg->getTask_type_name()
                          << " status=" << (record.completed_on_time ? "COMPLETED_ON_TIME" : "FAILED")
                          << std::endl;

                // This task was in the offload flow (task_records entry exists), so the DRL
                // has it in pending{} and is polling task:{id}:result.  Write the result now
                // so the DRL gets an answer instead of timing out after 30 s.
                // This covers local fallbacks that didn't call sendTaskFailureToRSU first
                // (SV_INVALID, REDIRECT_EXHAUSTED, gate-A RSSI override, etc.).
                redis_twin->writeSingleResult(
                    task_id,
                    record.completed_on_time ? "COMPLETED_ON_TIME" : "FAILED",
                    latency,
                    0.0,
                    fail_reason
                );
                std::cout << "LOCAL_FALLBACK_RESULT: task=" << task_id
                          << " written to task:result for DRL"
                          << " status=" << (record.completed_on_time ? "COMPLETED_ON_TIME" : "FAILED")
                          << " reason=" << fail_reason << std::endl;
            }
        }

        // Optional persistence hook (disabled in this build)
        insertTaskCompletion(msg);
        
        // Update vehicle digital twin
        VehicleDigitalTwin& twin = getOrCreateVehicleTwin(vehicle_id);
        if (record.completed_on_time) {
            twin.tasks_completed_on_time++;
            std::cout << "DT_SUCCESS: Task " << task_id << " completed ON TIME" << std::endl;
        } else {
            twin.tasks_completed_late++;
            std::cout << "DT_LATE: Task " << task_id << " completed LATE" << std::endl;
        }
        twin.last_update_time = simTime().dbl();
    } else {
        // task_records only contains offloaded tasks (populated via handleTaskMetadata).
        // Locally-executed tasks are never registered there — they arrive here directly.
        // We must write the local result to Redis so Python can collect per-type metrics.
        if (msg->getIs_local_execution() && redis_twin && use_redis) {
            double latency = msg->getProcessing_time();
            std::string fail_reason = msg->getFailure_reason();
            if (fail_reason.empty() || fail_reason == "NONE") {
                fail_reason = msg->getCompleted_on_time() ? "NONE" : "DEADLINE_MISSED";
            }
            redis_twin->writeLocalResult(
                task_id,
                msg->getTask_type_name(),
                msg->getQos_value(),
                msg->getDeadline_seconds(),
                msg->getCompleted_on_time() ? "COMPLETED_ON_TIME" : "FAILED",
                latency,
                0.0,
                fail_reason
            );
            std::cout << "LOCAL_RESULT: task=" << task_id
                      << " type=" << msg->getTask_type_name()
                      << " status=" << (msg->getCompleted_on_time() ? "COMPLETED_ON_TIME" : "FAILED")
                      << " latency=" << latency << "s"
                      << std::endl;
        } else {
            // Offloaded task whose record was GC'd after 120s. Write result to Redis
            // so the DRL's pending{} entry resolves instead of timing out.
            if (redis_twin && use_redis) {
                double latency = msg->getProcessing_time();
                bool on_time = msg->getCompleted_on_time();
                redis_twin->writeSingleResult(
                    task_id,
                    on_time ? "COMPLETED_ON_TIME" : "FAILED",
                    latency, 0.0,
                    on_time ? "NONE" : "DEADLINE_MISSED");
            }
            result_awaiting_.erase(task_id);
            EV_INFO << "⚠ Task record not found for task " << task_id << endl;
            std::cout << "DT_WARNING: RSU received completion for unknown task " << task_id << std::endl;
        }
    }

    updateDigitalTwinStatistics();

    // Ownership: this message came from the air and onWSM's caller does not delete it,
    // so we must delete here.
    delete msg;
}

void MyRSUApp::handleTaskFailure(TaskFailureMessage* msg) {
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║                   RSU: TASK FAILURE RECEIVED                             ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    std::string task_id = msg->getTask_id();
    std::string vehicle_id = msg->getVehicle_id();
    std::string reason = msg->getFailure_reason();
    if (reason.empty()) {
        auto recIt = task_records.find(task_id);
        if (recIt != task_records.end() && !recIt->second.failure_reason.empty()) {
            reason = recIt->second.failure_reason;
        } else {
            reason = "UNKNOWN_FAILURE";
        }
    }
    
    // Update task record
    auto it = task_records.find(task_id);
    if (it != task_records.end()) {
        TaskRecord& record = it->second;
        record.failed = true;
        result_awaiting_.erase(task_id);
        record.completion_time = msg->getFailure_time();
        record.failure_reason = reason;
        
        EV_INFO << "Task failure details:" << endl;
        EV_INFO << "  Task ID: " << task_id << endl;
        EV_INFO << "  Vehicle ID: " << vehicle_id << endl;
        EV_INFO << "  Failure time: " << msg->getFailure_time() << endl;
        EV_INFO << "  Reason: " << reason << endl;
        EV_INFO << "  Wasted time: " << msg->getWasted_time() << " sec" << endl;
        
        logTaskRecord(record, "FAILED_" + reason);
        
        // Update Redis task status
        if (redis_twin && use_redis) {
            redis_twin->updateTaskStatus(
                task_id,
                "FAILED",
                "",
                reason
            );
            // Also write to task:{id}:results hash so DRL's check_results_nonblocking()
            // detects this failure immediately (it polls :results, not :status).
            // Without this, the DRL waits 15+ real-seconds then marks it TIMEOUT,
            // polluting all fail-reason TensorBoard curves with wrong reason codes.
            double wasted = msg->getWasted_time();
            redis_twin->writeSingleResult(task_id, "FAILED", wasted, 0.0, reason);
        }
        
        // Optional persistence hook (disabled in this build)
        insertTaskFailure(msg);
        
        // Update vehicle digital twin
        VehicleDigitalTwin& twin = getOrCreateVehicleTwin(vehicle_id);
        if (reason == "REJECTED") {
            twin.tasks_rejected++;
            std::cout << "DT_REJECT: Task " << task_id << " REJECTED" << std::endl;
        } else {
            twin.tasks_failed++;
            std::cout << "DT_FAIL: Task " << task_id << " FAILED - " << reason << std::endl;
        }
        twin.last_update_time = simTime().dbl();
    } else {
        // Task record not found: either a pure-local task (never registered in task_records)
        // or an offloaded task whose record was GC'd. Write FAILED to Redis in both cases —
        // harmless for local tasks (Python never tracked them), necessary for GC'd offloaded
        // tasks so the DRL pending{} entry resolves instead of hitting its 60-s timeout.
        if (redis_twin && use_redis) {
            double wasted = msg->getWasted_time();
            redis_twin->writeSingleResult(task_id, "FAILED", wasted, 0.0, reason);
        }
        result_awaiting_.erase(task_id);
        EV_INFO << "⚠ Task failure for unregistered task: " << task_id
                << " reason=" << reason << endl;
    }

    updateDigitalTwinStatistics();

    // Ownership: this message came from the air and onWSM's caller does not delete it,
    // so we must delete here.
    delete msg;
}

void MyRSUApp::handleVehicleResourceStatus(VehicleResourceStatusMessage* msg) {
    EV_INFO << "📊 RSU: Vehicle resource status update received" << endl;
    
    std::string vehicle_id = msg->getVehicle_id();
    VehicleDigitalTwin& twin = getOrCreateVehicleTwin(vehicle_id);

    // Register vehicle MAC from heartbeat — populateWSM() on vehicle side sets senderAddress.
    // This ensures service-vehicle MACs are known before any offloading request arrives,
    // preventing SV_MAC_UNKNOWN failures when DDQN/baselines target service vehicles.
    LAddress::L2Type hb_mac = msg->getSenderAddress();
    if (hb_mac != 0 && vehicle_macs.find(vehicle_id) == vehicle_macs.end()) {
        vehicle_macs[vehicle_id] = hb_mac;
        EV_INFO << "  → Registered MAC for vehicle " << vehicle_id
                << " from heartbeat: " << hb_mac << endl;
    }
    
    // Update vehicle state
    twin.pos_x = msg->getPos_x();
    twin.pos_y = msg->getPos_y();
    twin.speed = msg->getSpeed();
    twin.heading = msg->getHeading();
    
    // Update resource information
    twin.cpu_total = msg->getCpu_total();
    twin.cpu_allocable = msg->getCpu_allocable();
    twin.cpu_available = msg->getCpu_available();
    twin.cpu_utilization = msg->getCpu_utilization();
    
    twin.mem_total = msg->getMem_total();
    twin.mem_available = msg->getMem_available();
    twin.mem_utilization = msg->getMem_utilization();

    twin.battery_level_pct = msg->getBattery_level_pct();
    twin.battery_current_mAh = msg->getBattery_current_mAh();
    twin.battery_capacity_mAh = msg->getBattery_capacity_mAh();
    twin.energy_task_j_total = msg->getEnergy_task_j_total();
    twin.energy_task_j_last = msg->getEnergy_task_j_last();
    twin.vehicle_type = msg->getVehicle_type();
    twin.tx_power_mw = msg->getTx_power_mw();
    twin.storage_capacity_gb = msg->getStorage_capacity_gb();
    twin.max_queue_size = msg->getMax_queue_size();
    twin.max_concurrent_tasks = msg->getMax_concurrent_tasks();
    
    // Update task statistics
    twin.tasks_generated = msg->getTasks_generated();
    twin.tasks_completed_on_time = msg->getTasks_completed_on_time();
    twin.tasks_completed_late = msg->getTasks_completed_late();
    twin.tasks_failed = msg->getTasks_failed();
    twin.tasks_rejected = msg->getTasks_rejected();
    twin.current_queue_length = msg->getCurrent_queue_length();
    twin.current_processing_count = msg->getCurrent_processing_count();
    
    twin.avg_completion_time = msg->getAvg_completion_time();
    twin.deadline_miss_ratio = msg->getDeadline_miss_ratio();
    
    twin.last_update_time = simTime().dbl();
    refreshVehicleCoverageRecord(vehicle_id);
    
    EV_INFO << "Vehicle " << vehicle_id << " Digital Twin updated:" << endl;
    
    // Update Redis Digital Twin (instant, for ML queries)
    if (redis_twin && use_redis) {
        const double source_timestamp = msg->getCreationTime().dbl();
        redis_twin->updateVehicleState(
            vehicle_id,
            twin.pos_x, twin.pos_y, twin.speed, twin.heading,
            twin.cpu_available, twin.cpu_utilization,
            twin.mem_available, twin.mem_utilization,
            twin.battery_level_pct, twin.battery_current_mAh,
            twin.battery_capacity_mAh,
            twin.energy_task_j_total, twin.energy_task_j_last,
            twin.current_queue_length, twin.current_processing_count,
            simTime().dbl(),
            msg->getAcceleration(),
            source_timestamp
        );
    }
    
    // Optional persistence hook (disabled in this build)
    insertVehicleStatus(msg);
    
    // Insert/update vehicle metadata (first time or periodic update)
    insertVehicleMetadata(vehicle_id);

    if (secondary_link_context_export) {
        exportSecondaryContextSamples(msg, twin);
    }
    
    EV_INFO << "  Position: (" << twin.pos_x << ", " << twin.pos_y << ")" << endl;
    EV_INFO << "  CPU Utilization: " << (twin.cpu_utilization * 100.0) << "%" << endl;
    EV_INFO << "  Memory Utilization: " << (twin.mem_utilization * 100.0) << "%" << endl;
    EV_INFO << "  Queue Length: " << twin.current_queue_length << endl;
    EV_INFO << "  Processing Count: " << twin.current_processing_count << endl;
    
    if (terminalVerboseLogs) {
        std::cout << "DT_UPDATE: Vehicle " << vehicle_id << " - CPU:" << (twin.cpu_utilization * 100.0)
                  << "% Mem:" << (twin.mem_utilization * 100.0) << "% Queue:" << twin.current_queue_length << std::endl;
    }

    // Ownership: this message came from the air and onWSM's caller does not delete it,
    // so we must delete here.
    delete msg;
}

void MyRSUApp::initializeRsuStaticContexts() {
    rsu_static_contexts.clear();

    cModule* networkModule = getModuleByPath("^.^");
    if (!networkModule) {
        return;
    }

    for (int idx = 0;; ++idx) {
        cModule* rsuModule = networkModule->getSubmodule("rsu", idx);
        if (!rsuModule) break;

        RsuStaticContext ctx;
        ctx.rsu_id = "RSU_" + std::to_string(idx);

        try {
            cModule* mob = rsuModule->getSubmodule("mobility");
            if (mob && mob->hasPar("x") && mob->hasPar("y")) {
                ctx.pos_x = mob->par("x").doubleValue();
                ctx.pos_y = mob->par("y").doubleValue();
            }
        } catch (...) {
            // Keep defaults if position is unavailable.
        }

        rsu_static_contexts.push_back(ctx);
    }
}

void MyRSUApp::exportSecondaryContextSamples(const VehicleResourceStatusMessage* msg,
                                             const VehicleDigitalTwin& sourceTwin) {
    const std::string source_id = msg->getVehicle_id();
    const double now = simTime().dbl();

    auto itLast = secondary_last_export_time.find(source_id);
    if (itLast != secondary_last_export_time.end()) {
        if ((now - itLast->second) < secondary_link_sample_interval) {
            return;
        }
    }
    secondary_last_export_time[source_id] = now;

    insertSecondaryProgress(now);
    insertSecondaryVehicleSample(msg);

    if (redis_twin && use_redis) {
        redis_twin->updateSecondaryProgress(secondary_run_id, now, secondary_link_sample_interval);
        redis_twin->pushSecondaryVehicleSample(
            secondary_run_id,
            source_id,
            now,
            sourceTwin.pos_x,
            sourceTwin.pos_y,
            sourceTwin.speed,
            sourceTwin.heading,
            msg->getAcceleration(),
            sourceTwin.cpu_available,
            sourceTwin.cpu_utilization,
            sourceTwin.mem_available,
            sourceTwin.mem_utilization,
            static_cast<int>(sourceTwin.current_queue_length),
            static_cast<int>(sourceTwin.current_processing_count),
            secondary_redis_max_series_len
        );
    }

    // V2RSU link context samples for this source vehicle.
    for (const auto& rsu : rsu_static_contexts) {
        const double dx = sourceTwin.pos_x - rsu.pos_x;
        const double dy = sourceTwin.pos_y - rsu.pos_y;
        const double dist = std::sqrt(dx * dx + dy * dy);
        const double rel_speed = std::fabs(sourceTwin.speed);

        insertSecondaryLinkSample(
            "V2RSU", source_id, rsu.rsu_id, now,
            sourceTwin.pos_x, sourceTwin.pos_y,
            rsu.pos_x, rsu.pos_y,
            dist, rel_speed,
            sourceTwin.heading, 0.0
        );

        if (redis_twin && use_redis) {
            redis_twin->pushSecondaryV2RsuLinkSample(
                secondary_run_id,
                source_id,
                rsu.rsu_id,
                now,
                sourceTwin.pos_x,
                sourceTwin.pos_y,
                rsu.pos_x,
                rsu.pos_y,
                dist,
                rel_speed,
                sourceTwin.heading,
                secondary_redis_max_series_len
            );
        }
    }

    // V2V link context samples from source vehicle to all known peers.
    for (const auto& kv : vehicle_twins) {
        const std::string& peer_id = kv.first;
        if (peer_id == source_id) continue;

        const VehicleDigitalTwin& peer = kv.second;
        const double dx = sourceTwin.pos_x - peer.pos_x;
        const double dy = sourceTwin.pos_y - peer.pos_y;
        const double dist = std::sqrt(dx * dx + dy * dy);
        const double rel_speed = std::fabs(sourceTwin.speed - peer.speed);

        insertSecondaryLinkSample(
            "V2V", source_id, peer_id, now,
            sourceTwin.pos_x, sourceTwin.pos_y,
            peer.pos_x, peer.pos_y,
            dist, rel_speed,
            sourceTwin.heading, peer.heading
        );

        if (redis_twin && use_redis) {
            redis_twin->pushSecondaryV2vLinkSample(
                secondary_run_id,
                source_id,
                peer_id,
                now,
                sourceTwin.pos_x,
                sourceTwin.pos_y,
                peer.pos_x,
                peer.pos_y,
                dist,
                rel_speed,
                sourceTwin.heading,
                peer.heading,
                secondary_redis_max_series_len
            );
        }
    }
}

void MyRSUApp::refreshVehicleCoverageRecord(const std::string& vehicle_id) {
    auto twinIt = vehicle_twins.find(vehicle_id);
    if (twinIt == vehicle_twins.end()) {
        return;
    }

    const VehicleDigitalTwin& vt = twinIt->second;
    Coord vehiclePos(vt.pos_x, vt.pos_y);

    double selfDistance = vehiclePos.distance(curPosition);
    double selfRssi = estimateDirectLinkRssiDbm(selfDistance);

    LAddress::L2Type bestMac = myId;
    std::string bestId = "RSU_" + std::to_string(rsu_id);
    double bestRssi = selfRssi;

    for (const auto& kv : neighbor_rsus) {
        const RSUNeighborState& neighbor = kv.second;
        if (!isNeighborStateFresh(neighbor)) {
            continue;
        }
        Coord npos(neighbor.pos_x, neighbor.pos_y);
        double rssi = estimateDirectLinkRssiDbm(vehiclePos.distance(npos));
        if (rssi > bestRssi) {
            bestRssi = rssi;
            bestMac = neighbor.rsu_mac;
            bestId = neighbor.rsu_id;
        }
    }

    double coverageRadius = 300.0;
    try {
        coverageRadius = par("rsuCoverageRadius_m").doubleValue();
    } catch (...) {
        // keep default radius
    }

    VehicleCoverageRecord rec;
    rec.vehicle_id = vehicle_id;
    rec.best_rsu_id = bestId;
    rec.best_rsu_mac = bestMac;
    rec.best_rssi = bestRssi;
    rec.self_rssi = selfRssi;
    rec.in_self_range = (selfDistance <= coverageRadius) && (bestMac == myId);
    rec.last_update_time = simTime().dbl();

    vehicle_coverage_records[vehicle_id] = rec;
}

LAddress::L2Type MyRSUApp::getBestRsuByRssiForVehicle(const std::string& vehicle_id,
                                                       std::string* bestRsuId,
                                                       bool* inSelfRange) {
    refreshVehicleCoverageRecord(vehicle_id);

    auto it = vehicle_coverage_records.find(vehicle_id);
    if (it == vehicle_coverage_records.end()) {
        if (bestRsuId) *bestRsuId = "";
        if (inSelfRange) *inSelfRange = false;
        return 0;
    }

    if (bestRsuId) *bestRsuId = it->second.best_rsu_id;
    if (inSelfRange) *inSelfRange = it->second.in_self_range;
    return it->second.best_rsu_mac;
}

VehicleDigitalTwin& MyRSUApp::getOrCreateVehicleTwin(const std::string& vehicle_id) {
    auto it = vehicle_twins.find(vehicle_id);
    if (it == vehicle_twins.end()) {
        VehicleDigitalTwin twin;
        twin.vehicle_id = vehicle_id;
        twin.first_seen_time = simTime().dbl();
        twin.last_update_time = simTime().dbl();
        vehicle_twins[vehicle_id] = twin;
        
        EV_INFO << "✨ Created new Digital Twin for vehicle " << vehicle_id << endl;
        if (terminalVerboseLogs) {
            std::cout << "DT_NEW: Created Digital Twin for vehicle " << vehicle_id << std::endl;
        }
        
        return vehicle_twins[vehicle_id];
    }
    return it->second;
}

void MyRSUApp::updateDigitalTwinStatistics() {
    // This could compute aggregate statistics across all vehicles
    // For now, just log current state
    logDigitalTwinState();
}

void MyRSUApp::logDigitalTwinState() {
    EV_INFO << "\n" << endl;
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║                    DIGITAL TWIN STATE SUMMARY                            ║" << endl;
    EV_INFO << "╠══════════════════════════════════════════════════════════════════════════╣" << endl;
    EV_INFO << "║ Tracked Vehicles: " << std::setw(54) << vehicle_twins.size() << "║" << endl;
    EV_INFO << "║ Task Records:     " << std::setw(54) << task_records.size() << "║" << endl;
    EV_INFO << "╠══════════════════════════════════════════════════════════════════════════╣" << endl;
    
    uint32_t total_generated = 0;
    uint32_t total_completed_on_time = 0;
    uint32_t total_completed_late = 0;
    uint32_t total_failed = 0;
    uint32_t total_rejected = 0;
    
    for (const auto& pair : vehicle_twins) {
        const VehicleDigitalTwin& twin = pair.second;
        total_generated += twin.tasks_generated;
        total_completed_on_time += twin.tasks_completed_on_time;
        total_completed_late += twin.tasks_completed_late;
        total_failed += twin.tasks_failed;
        total_rejected += twin.tasks_rejected;
        
        EV_INFO << "║ Vehicle " << std::left << std::setw(63) << twin.vehicle_id << "║" << endl;
        EV_INFO << "║   Generated: " << std::setw(11) << twin.tasks_generated 
                << " OnTime: " << std::setw(11) << twin.tasks_completed_on_time
                << " Late: " << std::setw(11) << twin.tasks_completed_late 
                << " Failed: " << std::setw(8) << twin.tasks_failed << "║" << endl;
        EV_INFO << "║   CPU: " << std::setw(5) << (int)twin.cpu_utilization << "%  "
                << "Mem: " << std::setw(5) << (int)twin.mem_utilization << "%  "
                << "Queue: " << std::setw(5) << twin.current_queue_length << "  "
                << "Processing: " << std::setw(19) << twin.current_processing_count << "║" << endl;
    }
    
    EV_INFO << "╠══════════════════════════════════════════════════════════════════════════╣" << endl;
    EV_INFO << "║ AGGREGATE STATISTICS                                                     ║" << endl;
    EV_INFO << "╠══════════════════════════════════════════════════════════════════════════╣" << endl;
    EV_INFO << "║ Total Generated:      " << std::setw(48) << total_generated << "║" << endl;
    EV_INFO << "║ Total On Time:        " << std::setw(48) << total_completed_on_time << "║" << endl;
    EV_INFO << "║ Total Late:           " << std::setw(48) << total_completed_late << "║" << endl;
    EV_INFO << "║ Total Failed:         " << std::setw(48) << total_failed << "║" << endl;
    EV_INFO << "║ Total Rejected:       " << std::setw(48) << total_rejected << "║" << endl;
    
    if (total_generated > 0) {
        double success_rate = (double)total_completed_on_time / total_generated * 100.0;
        double late_rate = (double)total_completed_late / total_generated * 100.0;
        double fail_rate = (double)total_failed / total_generated * 100.0;
        double reject_rate = (double)total_rejected / total_generated * 100.0;
        
        EV_INFO << "║ ────────────────────────────────────────────────────────────────────────║" << endl;
        EV_INFO << "║ Success Rate:         " << std::setw(38) << success_rate << " %        ║" << endl;
        EV_INFO << "║ Late Rate:            " << std::setw(38) << late_rate << " %        ║" << endl;
        EV_INFO << "║ Failure Rate:         " << std::setw(38) << fail_rate << " %        ║" << endl;
        EV_INFO << "║ Rejection Rate:       " << std::setw(38) << reject_rate << " %        ║" << endl;
    }
    
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;

    if (terminalVerboseLogs && !secondary_link_context_export) {
        std::cout << "DT_SUMMARY: Vehicles:" << vehicle_twins.size() 
                  << " Tasks:" << task_records.size()
                  << " Generated:" << total_generated
                  << " OnTime:" << total_completed_on_time
                  << " Late:" << total_completed_late
                  << " Failed:" << total_failed
                  << " Rejected:" << total_rejected << std::endl;
    }
}

void MyRSUApp::logTaskRecord(const TaskRecord& record, const std::string& event) {
    EV_INFO << "📝 TASK RECORD [" << event << "]:" << endl;
    EV_INFO << "  ID: " << record.task_id << endl;
    EV_INFO << "  Vehicle: " << record.vehicle_id << endl;
    EV_INFO << "  Size: " << (record.mem_footprint_bytes / 1024.0) << " KB" << endl;
    EV_INFO << "  Cycles: " << (record.cpu_cycles / 1e9) << " G" << endl;
    EV_INFO << "  QoS: " << record.qos_value << endl;
    EV_INFO << "  Created: " << record.created_time << " s" << endl;
    EV_INFO << "  Received: " << record.received_time << " s" << endl;
    if (record.completed || record.failed) {
        EV_INFO << "  Completion: " << record.completion_time << " s" << endl;
        EV_INFO << "  Processing: " << record.processing_time << " s" << endl;
    }
    if (record.failed) {
        EV_INFO << "  Failure reason: " << record.failure_reason << endl;
    }
}

// ============================================================================
// OPTIONAL PERSISTENCE HOOKS (disabled in this build)
// ============================================================================

void MyRSUApp::insertTaskMetadata(const TaskMetadataMessage* msg) {
    (void)msg;
}

void MyRSUApp::insertTaskCompletion(const TaskCompletionMessage* msg) {
    (void)msg;
}

void MyRSUApp::insertTaskFailure(const TaskFailureMessage* msg) {
    (void)msg;
}

void MyRSUApp::insertVehicleStatus(const VehicleResourceStatusMessage* msg) {
    (void)msg;
}

void MyRSUApp::insertSecondaryProgress(double sim_time) {
    (void)sim_time;
}

void MyRSUApp::insertSecondaryVehicleSample(const VehicleResourceStatusMessage* msg) {
    (void)msg;
}

void MyRSUApp::insertSecondaryLinkSample(const std::string& link_type,
                                         const std::string& tx_entity_id,
                                         const std::string& rx_entity_id,
                                         double sim_time,
                                         double tx_pos_x,
                                         double tx_pos_y,
                                         double rx_pos_x,
                                         double rx_pos_y,
                                         double distance_m,
                                         double relative_speed,
                                         double tx_heading,
                                         double rx_heading) {
    (void)link_type;
    (void)tx_entity_id;
    (void)rx_entity_id;
    (void)sim_time;
    (void)tx_pos_x;
    (void)tx_pos_y;
    (void)rx_pos_x;
    (void)rx_pos_y;
    (void)distance_m;
    (void)relative_speed;
    (void)tx_heading;
    (void)rx_heading;
}

void MyRSUApp::insertSecondaryQCycle(uint64_t cycle_id,
                                     double sim_time,
                                     int trajectory_count,
                                     int candidate_count,
                                     int entry_count) {
    (void)cycle_id;
    (void)sim_time;
    (void)trajectory_count;
    (void)candidate_count;
    (void)entry_count;
}

void MyRSUApp::insertSecondaryQEntry(uint64_t cycle_id,
                                     const std::string& link_type,
                                     const std::string& tx_entity_id,
                                     const std::string& rx_entity_id,
                                     int step_index,
                                     double predicted_time,
                                     double tx_pos_x,
                                     double tx_pos_y,
                                     double rx_pos_x,
                                     double rx_pos_y,
                                     double distance_m,
                                     double sinr_db) {
    (void)cycle_id;
    (void)link_type;
    (void)tx_entity_id;
    (void)rx_entity_id;
    (void)step_index;
    (void)predicted_time;
    (void)tx_pos_x;
    (void)tx_pos_y;
    (void)rx_pos_x;
    (void)rx_pos_y;
    (void)distance_m;
    (void)sinr_db;
}

void MyRSUApp::insertOffloadingRequest(const OffloadingRequest& request) {
    (void)request;
}

void MyRSUApp::insertOffloadingDecision(const std::string& task_id, const veins::OffloadingDecisionMessage* decision) {
    (void)task_id;
    (void)decision;
}

void MyRSUApp::insertTaskOffloadingEvent(const veins::TaskOffloadingEvent* event) {
    (void)event;
}

void MyRSUApp::updateTaskStaticTraceFromEvent(const std::string& task_id,
                                              const std::string& event_type,
                                              double event_time,
                                              const std::string& details) {
    (void)task_id;
    (void)event_type;
    (void)event_time;
    (void)details;
}

void MyRSUApp::insertOffloadedTaskCompletion(const std::string& task_id, const std::string& vehicle_id,
                                             const std::string& decision_type, const std::string& processor_id,
                                             double request_time, double decision_time, double start_time,
                                             double completion_time, bool success, bool completed_on_time,
                                             double deadline_seconds, uint64_t mem_footprint_bytes, uint64_t cpu_cycles,
                                             double qos_value, const std::string& result_data, const std::string& failure_reason) {
    (void)task_id;
    (void)vehicle_id;
    (void)decision_type;
    (void)processor_id;
    (void)request_time;
    (void)decision_time;
    (void)start_time;
    (void)completion_time;
    (void)success;
    (void)completed_on_time;
    (void)deadline_seconds;
    (void)mem_footprint_bytes;
    (void)cpu_cycles;
    (void)qos_value;
    (void)result_data;
    (void)failure_reason;
}

// ============================================================================
// TASK OFFLOADING ORCHESTRATION IMPLEMENTATION
// ============================================================================

void MyRSUApp::handleOffloadingRequest(veins::OffloadingRequestMessage* msg) {
    EV_INFO << "\n" << endl;
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║              RSU: OFFLOADING REQUEST RECEIVED                            ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    std::string task_id = msg->getTask_id();
    std::string vehicle_id = msg->getVehicle_id();
    
    std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << "INFO: RSU RECEIVED OFFLOADING REQUEST" << std::endl;
    std::cout << "INFO: Task ID: " << task_id << std::endl;
    std::cout << "INFO: Vehicle ID: " << vehicle_id << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n" << std::endl;
    
    // Store request details
    OffloadingRequest request;
    request.task_id = task_id;
    request.vehicle_id = vehicle_id;
    request.vehicle_mac = msg->getSenderAddress();  // Inherited from BaseFrame1609_4
    request.request_time = simTime().dbl();
    request.local_decision = msg->getLocal_decision();
    request.initial_gate_classification = msg->getInitial_gate_classification();
    request.initial_gate_reason = msg->getInitial_gate_reason();
    
    // Store vehicle MAC address for later decision sending
    vehicle_macs[vehicle_id] = request.vehicle_mac;
    EV_DEBUG << "  → Stored MAC address for vehicle " << vehicle_id << endl;

    // Update Digital Twin with current resource state so the RSU decision path
    // has fresh CPU/queue data for this vehicle.
    VehicleDigitalTwin& req_twin = getOrCreateVehicleTwin(vehicle_id);
    req_twin.cpu_available            = msg->getLocal_cpu_available_ghz();
    req_twin.cpu_utilization          = msg->getLocal_cpu_utilization();
    req_twin.current_queue_length     = msg->getLocal_queue_length();
    req_twin.current_processing_count = msg->getLocal_processing_count();
    req_twin.pos_x = msg->getPos_x();
    req_twin.pos_y = msg->getPos_y();
    req_twin.last_update_time = simTime().dbl();

    // Task characteristics
    request.mem_footprint_bytes = msg->getMem_footprint_bytes();
    request.cpu_cycles = msg->getCpu_cycles();
    request.deadline_seconds = msg->getDeadline_seconds();
    request.qos_value = msg->getQos_value();

    // Vehicle state
    request.vehicle_cpu_available = msg->getLocal_cpu_available_ghz();
    request.vehicle_cpu_utilization = msg->getLocal_cpu_utilization();
    request.vehicle_mem_available = msg->getLocal_mem_available_mb();
    request.vehicle_queue_length = msg->getLocal_queue_length();
    request.vehicle_processing_count = msg->getLocal_processing_count();

    // Vehicle location
    request.pos_x = msg->getPos_x();
    request.pos_y = msg->getPos_y();
    request.speed = msg->getSpeed();

    pending_offloading_requests[task_id] = request;

    // Persist request-time context for timeout root-cause analysis.
    insertOffloadingRequest(request);
    insertLifecycleEvent(task_id, "OFFLOADING_REQUEST_RECEIVED",
        vehicle_id, "RSU_" + std::to_string(rsu_id),
        std::string("{\"local_decision\":\"") + request.local_decision + "\"," +
        "\"initial_gate_classification\":\"" + request.initial_gate_classification + "\"," +
        "\"initial_gate_reason\":\"" + request.initial_gate_reason + "\"," +
        "\"request_received_sim_s\":" + std::to_string(request.request_time) + "," +
        "\"deadline_s\":" + std::to_string(request.deadline_seconds) + "," +
        "\"queue_len\":" + std::to_string(request.vehicle_queue_length) + "," +
        "\"processing_count\":" + std::to_string(request.vehicle_processing_count) + "}");
    
    EV_INFO << "  Task ID: " << task_id << endl;
    EV_INFO << "  Vehicle ID: " << vehicle_id << endl;
    EV_INFO << "  Task Size: " << (request.mem_footprint_bytes / 1024.0) << " KB" << endl;
    EV_INFO << "  CPU Cycles: " << (request.cpu_cycles / 1e9) << " G" << endl;
    EV_INFO << "  Deadline: " << request.deadline_seconds << " s" << endl;
    EV_INFO << "  QoS: " << request.qos_value << endl;
    EV_INFO << "  Local Decision Recommendation: " << request.local_decision << endl;
    EV_INFO << "  Vehicle CPU Available: " << request.vehicle_cpu_available << " GHz" << endl;
    EV_INFO << "  Vehicle CPU Utilization: " << (request.vehicle_cpu_utilization * 100) << "%" << endl;
    EV_INFO << "  Vehicle Queue Length: " << request.vehicle_queue_length << endl;

    // When using Redis/DRL, decisions come from the checkDecisionMsg timer.
    if (redis_twin && use_redis) {
        EV_INFO << "Redis/DRL active — decision will be provided via checkDecisionMsg timer" << endl;
        if (terminalVerboseLogs) {
            std::cout << "RSU_OFFLOAD: Task " << task_id
                      << " — awaiting DRL decision via Redis (heuristic skipped)" << std::endl;
        }
    } else {
        // Legacy heuristic path (no DRL)
        veins::OffloadingDecisionMessage* decision = makeOffloadingDecision(request);

        if (decision) {
            insertOffloadingDecision(task_id, decision);
            insertLifecycleEvent(task_id, "OFFLOADING_DECISION_IN_PROCESS",
                "RSU_" + std::to_string(rsu_id), vehicle_id,
                std::string("{\"decision_type\":\"") + decision->getDecision_type() + "\","
                "\"confidence\":" + std::to_string(decision->getConfidence_score()) + "}");

            populateWSM(decision, request.vehicle_mac);
            sendDown(decision);
            insertLifecycleEvent(task_id, "OFFLOADING_DECISION_SENDING",
                "RSU_" + std::to_string(rsu_id), vehicle_id,
                std::string("{\"decision_type\":\"") + decision->getDecision_type() + "\"}");

            EV_INFO << "✓ Offloading decision sent back to vehicle " << vehicle_id << endl;
            if (terminalVerboseLogs) {
                std::cout << "RSU_OFFLOAD: Decision sent for task " << task_id
                          << ": " << decision->getDecision_type() << std::endl;
            }
        } else {
            EV_ERROR << "Failed to create offloading decision" << endl;
        }
    }
    
    // Leave request message teardown to the framework path that delivered it.
    // This avoids destructor-time corruption if the generated packet ownership
    // chain is not safe to release here.

}

std::string MyRSUApp::selectBestServiceVehicle(const OffloadingRequest& request,
                                               double* outPredictedSeconds,
                                               std::string* outReason) const {
    std::string bestId;
    double bestScore = std::numeric_limits<double>::infinity();
    double bestPredicted = std::numeric_limits<double>::infinity();
    std::string bestReason;

    for (const auto& kv : vehicle_twins) {
        const std::string& candidateId = kv.first;
        const auto& twin = kv.second;

        if (candidateId == request.vehicle_id) {
            continue;
        }
        if (vehicle_macs.find(candidateId) == vehicle_macs.end()) {
            continue;
        }

        // Keep state freshness bounded; stale twins are risky SV choices.
        if ((simTime().dbl() - twin.last_update_time) > 0.9) {
            continue;
        }

        Coord tvPos(request.pos_x, request.pos_y);
        Coord svPos(twin.pos_x, twin.pos_y);
        const double distance_m = tvPos.distance(svPos);
        const double rssi_dbm = estimateDirectLinkRssiDbm(distance_m);

        // Hard prune very weak direct links.
        if (rssi_dbm < (directLinkRssiThreshold_dBm - 3.0)) {
            continue;
        }

        // Conservative direct-link throughput estimate from PHY params and RSSI quality.
        const double quality = std::max(0.0, std::min(1.0,
            (rssi_dbm - directLinkRssiThreshold_dBm) / 20.0));
        const double base_rate_bps = std::max(1.0, offload_link_bandwidth_hz * offload_rate_efficiency);
        double est_rate_bps = base_rate_bps * (0.2 + 0.8 * quality);
        if (offload_rate_cap_bps > 0.0) {
            est_rate_bps = std::min(est_rate_bps, offload_rate_cap_bps);
        }

        const double uplink_s = (static_cast<double>(request.mem_footprint_bytes) * 8.0) / std::max(1.0, est_rate_bps);
        const double downlink_bytes = std::max(1.0, static_cast<double>(request.mem_footprint_bytes) * offload_response_ratio);
        const double downlink_s = (downlink_bytes * 8.0) / std::max(1.0, est_rate_bps);

        // Use SV available CPU as a proxy for actual service compute budget.
        const double svc_cpu_hz = std::max(5e8, twin.cpu_available * 1e9);
        const double exec_s = static_cast<double>(request.cpu_cycles) / svc_cpu_hz;

        // Approximate waiting from current queued + processing load.
        const double pending_factor = static_cast<double>(twin.current_queue_length + twin.current_processing_count);
        const double queue_wait_s = pending_factor * exec_s * 0.6;

        const double predicted_total_s = uplink_s + queue_wait_s + exec_s + downlink_s + 0.01;

        // QoS-aware feasibility: keep margins closer to real deadline so we
        // request SV only when conversion likelihood is high.
        double qos_margin = 1.15;
        double min_slack_s = 0.03;
        if (request.qos_value >= 0.8) {
            qos_margin = 1.0;
            min_slack_s = 0.02;
        } else if (request.qos_value <= 0.4) {
            qos_margin = 1.22;
            min_slack_s = 0.05;
        }
        const double feasible_budget_s = std::max(0.001, (request.deadline_seconds * qos_margin) - min_slack_s);
        if (predicted_total_s > feasible_budget_s) {
            continue;
        }

        // Multi-factor score: prioritize lower predicted completion time,
        // then lower queue pressure and better link quality.
        const double normalized_queue = std::min(1.0, pending_factor / 8.0);
        const double link_penalty = 1.0 - quality;
        const double deadline_pressure = std::max(0.0,
            predicted_total_s / std::max(0.001, request.deadline_seconds) - 0.75);
        const double score = predicted_total_s
                           + (0.16 * normalized_queue)
                           + (0.10 * link_penalty)
                           + (0.14 * deadline_pressure);

        if (score < bestScore) {
            bestScore = score;
            bestPredicted = predicted_total_s;
            bestId = candidateId;
            std::ostringstream oss;
            oss << "SV score=" << score
                << " pred=" << predicted_total_s
                << "s q=" << pending_factor
                << " rssi=" << rssi_dbm << "dBm";
            bestReason = oss.str();
        }
    }

    if (outPredictedSeconds) {
        *outPredictedSeconds = std::isfinite(bestPredicted) ? bestPredicted : 0.0;
    }
    if (outReason) {
        *outReason = bestReason;
    }

    return bestId;
}

veins::OffloadingDecisionMessage* MyRSUApp::makeOffloadingDecision(const OffloadingRequest& request) {
    EV_INFO << "🤖 RSU: Making ML-based offloading decision..." << endl;
    
    // ========================================================================
    // ML DECISION ENGINE (PLACEHOLDER - REPLACE WITH ACTUAL ML MODEL)
    // ========================================================================
    
    std::string decisionType;
    std::string reason;
    double confidence = 0.0;
    double estimated_completion_time = 0.0;
    std::string target_service_vehicle_id;
    std::string remote_processor_rsu_id;
    LAddress::L2Type target_service_vehicle_mac = 0;
    LAddress::L2Type remote_processor_rsu_mac = 0;
    
    if (!mlModelEnabled) {
        // ----------------------------------------------------------------
        // PLACEHOLDER: Round-robin rule-based decision
        // (Will be replaced by the DRL algorithm)
        //
        // Routing: every 3rd offloadable request → SERVICE_VEHICLE (V2V),
        //          others → RSU or LOCAL. Exercises all 3 execution paths
        //          so each can be independently verified before DRL lands.
        // ----------------------------------------------------------------
        EV_INFO << "  Using placeholder round-robin decision" << endl;

        // Prefer SV when origin vehicle is loaded or local pre-decision already asked for SV.
        bool sv_pressure = (request.vehicle_cpu_utilization >= 0.45) ||
                           (request.vehicle_queue_length > 0) ||
                           (request.local_decision == "OFFLOAD_TO_SERVICE_VEHICLE");
        bool try_sv = serviceVehicleSelectionEnabled && sv_pressure;

        if (try_sv) {
            double sv_predicted_s = 0.0;
            std::string sv_pick_reason;
            std::string sv_id = selectBestServiceVehicle(request, &sv_predicted_s, &sv_pick_reason);
            if (!sv_id.empty() && vehicle_macs.count(sv_id)) {
                decisionType = "SERVICE_VEHICLE";
                target_service_vehicle_id  = canonicalServiceVehicleId(sv_id);
                target_service_vehicle_mac = vehicle_macs[sv_id];  // real MAC from Digital Twin
                reason = "Deadline-aware SV selection: " + sv_id + " (" + sv_pick_reason + ")";
                confidence = 0.82;
                estimated_completion_time = (sv_predicted_s > 0.0)
                    ? sv_predicted_s
                    : (request.deadline_seconds * 0.7);
                std::cout << "SV_DECISION: Task " << request.task_id
                          << " routed to service vehicle " << target_service_vehicle_id
                          << " MAC=" << target_service_vehicle_mac << std::endl;
            } else {
                try_sv = false;  // no suitable SV → fall through to RSU/LOCAL
            }
        }

        if (!try_sv) {
            // Use CPU utilization ratio (0-1) not raw GHz — vehicles have 3-7 GHz
            // so a raw 0.5 GHz threshold is always satisfied and everything became LOCAL.
            // Now: utilization < 0.3 (under 30% load) AND vehicle didn't request RSU → LOCAL.
            // Otherwise → RSU processes the task.
            if (request.vehicle_cpu_utilization < 0.3 &&
                request.local_decision != "OFFLOAD_TO_RSU" &&
                request.local_decision != "OFFLOAD_TO_SERVICE_VEHICLE") {
                decisionType = "LOCAL";
                reason = "Vehicle has low utilization ("
                         + std::to_string((int)(request.vehicle_cpu_utilization * 100))
                         + "%), execute locally";
                confidence = 0.85;
                estimated_completion_time = request.deadline_seconds * 0.5;
            } else {
                decisionType = "RSU";
                reason = "RSU offloading (round-robin placeholder)";
                confidence = 0.8;
                estimated_completion_time = request.deadline_seconds * 0.6;
            }
        }
    } else {
        // TODO: ML MODEL INFERENCE HERE
        // Extract features from request and Digital Twin
        // Call ML model prediction
        // Parse ML model output
        EV_INFO << "  ML model inference (NOT YET IMPLEMENTED)" << endl;
        decisionType = "LOCAL";  // Fallback
        reason = "ML model not yet integrated";
        confidence = 0.5;
        estimated_completion_time = request.deadline_seconds * 0.8;
    }
    
    EV_INFO << "  Decision: " << decisionType << endl;
    EV_INFO << "  Reason: " << reason << endl;
    EV_INFO << "  Confidence: " << confidence << endl;
    
    // Create decision message
    veins::OffloadingDecisionMessage* decision = new veins::OffloadingDecisionMessage();
    decision->setSenderAddress(myId);  // Set RSU's MAC address
    decision->setTask_id(request.task_id.c_str());
    decision->setVehicle_id(request.vehicle_id.c_str());
    decision->setDecision_type(decisionType.c_str());
    decision->setDecision_time(simTime().dbl());
    decision->setConfidence_score(confidence);
    decision->setEstimated_completion_time(estimated_completion_time);
    decision->setDecision_reason(reason.c_str());
    
    if (decisionType == "SERVICE_VEHICLE") {
        decision->setTarget_service_vehicle_id(target_service_vehicle_id.c_str());
        decision->setTarget_service_vehicle_mac(target_service_vehicle_mac);

        // Service-vehicle case routing policy:
        // - If TV<->SV direct RSSI is strong: direct V2V.
        // - Otherwise use RSU infrastructure with anchor RSU nearest to SV.
        auto svIt = vehicle_twins.find(extractCanonicalVehicleId(target_service_vehicle_id));
        if (svIt != vehicle_twins.end()) {
            Coord tvPos(request.pos_x, request.pos_y);
            Coord svPos(svIt->second.pos_x, svIt->second.pos_y);
            double tvSvRssi = estimateDirectLinkRssiDbm(tvPos.distance(svPos));

            if (tvSvRssi < directLinkRssiThreshold_dBm) {
                LAddress::L2Type bestAnchorMac = myId;
                std::string bestAnchorId = "RSU_" + std::to_string(rsu_id);
                double bestRssiToSv = estimateDirectLinkRssiDbm(svPos.distance(curPosition));

                for (const auto& kv : neighbor_rsus) {
                    const RSUNeighborState& neighbor = kv.second;
                    if (!isNeighborStateFresh(neighbor)) {
                        continue;
                    }
                    Coord npos(neighbor.pos_x, neighbor.pos_y);
                    double rssiToSv = estimateDirectLinkRssiDbm(svPos.distance(npos));
                    if (rssiToSv > bestRssiToSv) {
                        bestRssiToSv = rssiToSv;
                        bestAnchorMac = neighbor.rsu_mac;
                        bestAnchorId = neighbor.rsu_id;
                    }
                }

                decision->setRedirect_target_rsu_id(bestAnchorId.c_str());
                decision->setRedirect_target_rsu_mac(bestAnchorMac);
                decision->setNext_candidate_index(0);
                reason += "; infra SV route via " + bestAnchorId;
                decision->setDecision_reason(reason.c_str());
            }
        }
    }

    // Phase 3: keep ingress stable but allow ingress RSU to forward execution to a healthier neighbor.
    if (decisionType == "RSU") {
        double localLoad = (rsu_max_concurrent > 0)
            ? static_cast<double>(rsu_processing_count) / static_cast<double>(rsu_max_concurrent)
            : 1.0;

        if (localLoad >= 0.75) {
            double bestLoad = std::numeric_limits<double>::max();
            std::string bestNeighborId;
            LAddress::L2Type bestNeighborMac = 0;

            for (const auto& kv : neighbor_rsus) {
                const RSUNeighborState& neighbor = kv.second;
                if (!isNeighborStateFresh(neighbor)) {
                    continue;
                }
                if (neighbor.is_overloaded) {
                    continue;
                }
                if (neighbor.load_factor >= bestLoad) {
                    continue;
                }
                bestLoad = neighbor.load_factor;
                bestNeighborId = neighbor.rsu_id;
                bestNeighborMac = neighbor.rsu_mac;
            }

            if (bestNeighborMac != 0) {
                remote_processor_rsu_id = bestNeighborId;
                remote_processor_rsu_mac = bestNeighborMac;
                reason += "; remote processor " + remote_processor_rsu_id
                       + " selected (ingress load=" + std::to_string(localLoad) + ")";
            }
        }
    }

    if (remote_processor_rsu_mac != 0) {
        decision->setRedirect_target_rsu_id(remote_processor_rsu_id.c_str());
        decision->setRedirect_target_rsu_mac(remote_processor_rsu_mac);
        decision->setNext_candidate_index(0);
        decision->setDecision_reason(reason.c_str());
    }
    
    return decision;
}

void MyRSUApp::handleTaskOffloadPacket(veins::TaskOffloadPacket* msg) {
    EV_INFO << "\n" << endl;
    EV_INFO << "╔══════════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║              RSU: TASK OFFLOAD PACKET RECEIVED                           ║" << endl;
    EV_INFO << "╚══════════════════════════════════════════════════════════════════════════╝" << endl;
    
    std::string task_id = msg->getTask_id();
    std::string vehicle_id = msg->getOrigin_vehicle_id();
    rsu_tasks_arrived++;
    
    std::cout << "RSU_PROCESS: Received task " << task_id << " from vehicle " << vehicle_id 
              << " for RSU processing" << std::endl;
    
    EV_INFO << "  Task ID: " << task_id << endl;
    EV_INFO << "  Origin Vehicle: " << vehicle_id << endl;
    EV_INFO << "  Task Size: " << (msg->getMem_footprint_bytes() / 1024.0) << " KB" << endl;
    EV_INFO << "  CPU Cycles Required: " << (msg->getCpu_cycles() / 1e9) << " G" << endl;

    // Service-vehicle path routing:
    // TV -> ingress RSU -> optional anchor RSU -> SV
    LAddress::L2Type serviceVehicleMac = 0;
    LAddress::L2Type anchorRsuMac = 0;
    if (parseServiceRouteHint(msg->getTask_input_data(), serviceVehicleMac, anchorRsuMac)) {
        if (anchorRsuMac != 0 && anchorRsuMac != myId) {
            EV_INFO << "↪ Forwarding service task " << task_id << " to anchor RSU MAC " << anchorRsuMac << endl;
            populateWSM(msg, anchorRsuMac);
            sendDown(msg);
            insertLifecycleEvent(task_id, "SV_TASK_FORWARD_TO_ANCHOR_RSU",
                "RSU_" + std::to_string(rsu_id), "RSU",
                std::string("{\"anchor_rsu_mac\":") + std::to_string(anchorRsuMac) + "}");
            return;
        }

        if (serviceVehicleMac != 0) {
            EV_INFO << "↪ Relaying service task " << task_id << " to SV MAC " << serviceVehicleMac << endl;
            populateWSM(msg, serviceVehicleMac);
            sendDown(msg);
            insertLifecycleEvent(task_id, "SV_TASK_RELAY_TO_VEHICLE",
                "RSU_" + std::to_string(rsu_id), "SERVICE_VEHICLE",
                std::string("{\"service_vehicle_mac\":") + std::to_string(serviceVehicleMac) + "}");
            return;
        }
    }

    LAddress::L2Type ingressRsuMac = myId;
    LAddress::L2Type processorRsuMac = 0;
    if (!parseRouteHint(msg->getTask_input_data(), ingressRsuMac, processorRsuMac)) {
        ingressRsuMac = myId;
        processorRsuMac = 0;
    }

    if (processorRsuMac != 0 && processorRsuMac != myId) {
        EV_INFO << "↪ Forwarding task " << task_id << " to processor RSU MAC " << processorRsuMac << endl;
        populateWSM(msg, processorRsuMac);
        sendDown(msg);
        insertLifecycleEvent(task_id, "RSU_TASK_FORWARDED",
            "RSU_" + std::to_string(rsu_id), "RSU_FORWARD",
            std::string("{\"processor_rsu_mac\":") + std::to_string(processorRsuMac) + "}");
        return;
    }
    
    // Process task on RSU edge server
    processTaskOnRSU(task_id, msg, ingressRsuMac);
    
    // msg will be deleted in processTaskOnRSU or here
    delete msg;
}

void MyRSUApp::processTaskOnRSU(const std::string& task_id, veins::TaskOffloadPacket* packet, LAddress::L2Type ingress_rsu_mac) {
    std::string vehicle_id  = packet->getOrigin_vehicle_id();
    LAddress::L2Type vehicle_mac = packet->getOrigin_vehicle_mac();
    uint64_t mem_footprint_bytes = packet->getMem_footprint_bytes();
    uint64_t cpu_cycles     = packet->getCpu_cycles();
    double qos_value        = std::max(0.0, std::min(1.0, packet->getQos_value()));
    double deadline_seconds = packet->getDeadline_seconds();  // Extract absolute deadline from task metadata
    const double uplink_latency_s = std::max(0.0, simTime().dbl() - packet->getOffload_time());

    insertLifecycleEvent(task_id, "TASK_UPLINK_RECEIVED",
        vehicle_id, "RSU_" + std::to_string(rsu_id),
        "{\"uplink_s\":" + std::to_string(uplink_latency_s) +
        ",\"input_bytes\":" + std::to_string(mem_footprint_bytes) + "}");

    std::cout << "RSU_UPLINK_MEASURED: task=" << task_id
              << " uplink=" << uplink_latency_s
              << " input_bytes=" << mem_footprint_bytes << std::endl;

    // ======================================================================
    // ADMISSION CONTROL: bounded waiting queue before rejection
    // ======================================================================
    if (rsu_processing_count >= rsu_max_concurrent) {
        if (static_cast<int>(rsuWaitingQueue.size()) < rsu_waiting_queue_capacity) {
            QueuedRSUTask queued;
            queued.task_id = task_id;
            queued.vehicle_id = vehicle_id;
            queued.vehicle_mac = vehicle_mac;
            queued.ingress_rsu_mac = ingress_rsu_mac;
            queued.mem_footprint_bytes = mem_footprint_bytes;
            queued.cpu_cycles = cpu_cycles;
            queued.qos_value = qos_value;
            queued.enqueue_time = simTime().dbl();
            queued.deadline_seconds = deadline_seconds;  // Store deadline for validation on dequeue
            rsuWaitingQueue.push_back(queued);
            rsu_queue_length = static_cast<int>(rsuWaitingQueue.size());

            insertLifecycleEvent(task_id, "RSU_QUEUED",
                "RSU_" + std::to_string(rsu_id), vehicle_id,
                "{\"queue_len\":" + std::to_string(rsu_queue_length) + "}");

            EV_INFO << "⏸ RSU busy (" << rsu_processing_count << "/" << rsu_max_concurrent
                    << ") - queued task " << task_id
                    << " (waiting " << rsuWaitingQueue.size() << "/" << rsu_waiting_queue_capacity << ")" << endl;
            std::cout << "RSU_QUEUE: Task " << task_id << " queued ("
                      << rsuWaitingQueue.size() << "/" << rsu_waiting_queue_capacity << ")" << std::endl;
            return;
        }

        EV_WARN << "⛔ RSU overloaded and queue full (processing " << rsu_processing_count << "/"
                << rsu_max_concurrent << ", queue " << rsuWaitingQueue.size() << "/"
                << rsu_waiting_queue_capacity << ") — rejecting task " << task_id << endl;
        std::cout << "RSU_OVERLOAD: Task " << task_id << " rejected (processing full + queue full)" << std::endl;
        insertLifecycleEvent(task_id, "RSU_REJECTED_QUEUE_FULL",
            "RSU_" + std::to_string(rsu_id), vehicle_id,
            "{\"queue_capacity\":" + std::to_string(rsu_waiting_queue_capacity) + "}");
        rsu_tasks_rejected++;
        if (redis_twin && use_redis) {
            redis_twin->writeSingleResult(task_id, "FAILED", 0.0, 0.0, "RSU_QUEUE_FULL");
        }
        sendTaskResultToVehicle(task_id, vehicle_id, vehicle_mac, ingress_rsu_mac, false, 0.0);
        return;
    }

    if (!canReserveRSUMemory(mem_footprint_bytes)) {
        if (static_cast<int>(rsuWaitingQueue.size()) < rsu_waiting_queue_capacity) {
            QueuedRSUTask queued;
            queued.task_id = task_id;
            queued.vehicle_id = vehicle_id;
            queued.vehicle_mac = vehicle_mac;
            queued.ingress_rsu_mac = ingress_rsu_mac;
            queued.mem_footprint_bytes = mem_footprint_bytes;
            queued.cpu_cycles = cpu_cycles;
            queued.qos_value = qos_value;
            queued.enqueue_time = simTime().dbl();
            queued.deadline_seconds = deadline_seconds;
            rsuWaitingQueue.push_back(queued);
            rsu_queue_length = static_cast<int>(rsuWaitingQueue.size());

            insertLifecycleEvent(task_id, "RSU_QUEUED_MEMORY_WAIT",
                "RSU_" + std::to_string(rsu_id), vehicle_id,
                "{\"memory_required_mb\":" + std::to_string(mem_footprint_bytes / (1024.0 * 1024.0)) + ","
                "\"memory_available_mb\":" + std::to_string(getEffectiveRSUMemoryAvailable() * 1024.0) + "}");

            EV_INFO << "⏸ RSU memory constrained - queued task " << task_id
                    << " (required=" << (mem_footprint_bytes / (1024.0 * 1024.0))
                    << "MB, available=" << (getEffectiveRSUMemoryAvailable() * 1024.0) << "MB)" << endl;
            return;
        }

        EV_WARN << "⛔ RSU memory saturated and queue full - rejecting task " << task_id << endl;
        insertLifecycleEvent(task_id, "RSU_REJECTED_MEMORY_FULL",
            "RSU_" + std::to_string(rsu_id), vehicle_id,
            "{\"memory_required_mb\":" + std::to_string(mem_footprint_bytes / (1024.0 * 1024.0)) + ","
            "\"memory_available_mb\":" + std::to_string(getEffectiveRSUMemoryAvailable() * 1024.0) + "}");
        rsu_tasks_rejected++;
        if (redis_twin && use_redis) {
            redis_twin->writeSingleResult(task_id, "FAILED", 0.0, 0.0, "RSU_MEMORY_FULL");
        }
        sendTaskResultToVehicle(task_id, vehicle_id, vehicle_mac, ingress_rsu_mac, false, 0.0);
        return;
    }

    // ======================================================================
    // PHYSICS-BASED EXECUTION TIME WITH WEIGHTED CPU SHARING
    //
    // Model: RSU has edgeCPU_GHz total throughput spread across concurrent tasks
    // proportionally to task priority weight (derived from QoS).
    //
    //   cpu_task_i = edgeCPU_GHz * weight_i / sum(weights)
    //   exec_time    = cpu_cycles / cpu_per_task + overhead_once
    //
    // When N changes (new task arrives or task completes), ALL in-flight tasks
    // are rescheduled so each gets its updated weighted share — this correctly
    // slows tasks when load increases and speeds them up when load drops.
    // The overhead (processingDelay_ms) models OS/memory latency and is only
    // applied once at first admission, not at subsequent CPU reallocations.
    // ======================================================================
    rsu_processing_count++;
    rsu_tasks_admitted++;
    reserveRSUMemory(mem_footprint_bytes);
    int concurrent = rsu_processing_count;  // includes this new task

    double existing_weight_sum = 0.0;
    for (const auto& kv : rsuPendingTasks) {
        existing_weight_sum += getTaskPriorityWeight(kv.second.qos_value);
    }
    double this_weight = getTaskPriorityWeight(qos_value);
    double total_weight = std::max(1e-9, existing_weight_sum + this_weight);
    double cpu_per_task_Hz = (edgeCPU_GHz * 1e9) * (this_weight / total_weight);

    // Schedule THIS new task (overhead is applied once at admission)
    double exec_time = static_cast<double>(cpu_cycles) / cpu_per_task_Hz
                       + processingDelay_ms / 1000.0;

    cMessage* completeMsg = new cMessage("rsuTaskComplete");
    completeMsg->setContextPointer(new std::string(task_id));
    scheduleAt(simTime() + exec_time, completeMsg);

    PendingRSUTask pending;
    pending.vehicle_id            = vehicle_id;
    pending.vehicle_mac           = vehicle_mac;
    pending.ingress_rsu_mac       = ingress_rsu_mac;
    pending.mem_footprint_bytes   = mem_footprint_bytes;
    pending.cpu_cycles            = cpu_cycles;
    pending.qos_value             = qos_value;
    pending.cycles_remaining      = static_cast<double>(cpu_cycles);
    pending.exec_time_s           = exec_time;
    pending.scheduled_at          = simTime().dbl();
    pending.last_reschedule_time  = simTime().dbl();
    pending.cpu_allocated_hz      = cpu_per_task_Hz;
    pending.completion_event      = completeMsg;
    rsuPendingTasks[task_id]      = pending;

    // Reallocate all tasks, including the newly admitted one, using weighted shares.
    reallocateRSUTasks();
    auto it_after = rsuPendingTasks.find(task_id);
    if (it_after != rsuPendingTasks.end()) {
        cpu_per_task_Hz = it_after->second.cpu_allocated_hz;
        exec_time = it_after->second.exec_time_s;
    }
    insertLifecycleEvent(task_id, "PROCESSING_STARTED",
        "RSU_" + std::to_string(rsu_id), vehicle_id,
        "{\"exec_time_s\":" + std::to_string(exec_time) + ","
        "\"concurrent\":" + std::to_string(concurrent) + ","
        "\"qos\":" + std::to_string(qos_value) + ","
        "\"priority_weight\":" + std::to_string(this_weight) + ","
        "\"cpu_per_task_ghz\":" + std::to_string(cpu_per_task_Hz/1e9) + "}");

    EV_INFO << "⚙️ RSU: Task " << task_id << " accepted for edge processing" << endl;
    EV_INFO << "  Edge CPU weighted share: weight=" << this_weight
            << ", allocated=" << (cpu_per_task_Hz/1e9) << " GHz" << endl;
    EV_INFO << "  Exec time: " << (cpu_cycles/1e9) << "G cycles / "
            << (cpu_per_task_Hz/1e9) << " GHz + " << processingDelay_ms
            << "ms overhead = " << exec_time << "s" << endl;
    std::cout << "RSU_PROCESS: Task " << task_id << " scheduled, exec=" << exec_time
              << "s (" << concurrent << " concurrent tasks)" << std::endl;
}

// ============================================================================
// RSU WEIGHTED-SHARE CPU REALLOCATION
// Called whenever N_concurrent changes (task arrival or completion).
// Burns down cycles already executed for each in-flight task, then
// reschedules each completion event at the new weighted CPU share.
// ============================================================================
void MyRSUApp::reallocateRSUTasks() {
    if (rsuPendingTasks.empty()) return;

    double now = simTime().dbl();
    double total_weight = 0.0;
    for (const auto& kv : rsuPendingTasks) {
        total_weight += getTaskPriorityWeight(kv.second.qos_value);
    }
    if (total_weight <= 0.0) {
        total_weight = static_cast<double>(rsuPendingTasks.size());
    }

    for (auto& kv : rsuPendingTasks) {
        PendingRSUTask& t = kv.second;
        double task_weight = getTaskPriorityWeight(t.qos_value);
        double new_cpu_per_task_Hz = (edgeCPU_GHz * 1e9) * (task_weight / total_weight);

        // Burn down cycles consumed since this task was last rescheduled
        double elapsed       = now - t.last_reschedule_time;
        double cycles_burned = t.cpu_allocated_hz * elapsed;
        t.cycles_remaining   = std::max(0.0, t.cycles_remaining - cycles_burned);
        t.last_reschedule_time = now;
        t.cpu_allocated_hz   = new_cpu_per_task_Hz;

        // Reschedule completion event — no overhead added here (already paid at admission)
        if (t.completion_event && t.completion_event->isScheduled()) {
            cancelEvent(t.completion_event);
        }
        double new_exec = (new_cpu_per_task_Hz > 0.0)
                          ? t.cycles_remaining / new_cpu_per_task_Hz
                          : 0.0;
        t.exec_time_s = new_exec;
        scheduleAt(simTime() + new_exec, t.completion_event);

        EV_INFO << "  RSU realloc: task " << kv.first
                << " (w=" << task_weight << ") → " << (new_cpu_per_task_Hz/1e9) << " GHz, "
                << (t.cycles_remaining/1e9) << "G cycles left, "
                << "completes in " << new_exec << "s" << endl;
    }
}

void MyRSUApp::reallocateRSUTasks(double new_cpu_per_task_Hz) {
    if (rsuPendingTasks.empty() || new_cpu_per_task_Hz <= 0.0) {
        return;
    }

    double now = simTime().dbl();
    for (auto& kv : rsuPendingTasks) {
        PendingRSUTask& t = kv.second;
        double elapsed = now - t.last_reschedule_time;
        double cycles_burned = t.cpu_allocated_hz * elapsed;
        t.cycles_remaining = std::max(0.0, t.cycles_remaining - cycles_burned);
        t.last_reschedule_time = now;
        t.cpu_allocated_hz = new_cpu_per_task_Hz;

        if (t.completion_event && t.completion_event->isScheduled()) {
            cancelEvent(t.completion_event);
        }
        double new_exec = t.cycles_remaining / new_cpu_per_task_Hz;
        t.exec_time_s = new_exec;
        scheduleAt(simTime() + new_exec, t.completion_event);
    }
}

bool MyRSUApp::canReserveRSUMemory(uint64_t bytes) const {
    const double required_gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    return required_gb <= (getEffectiveRSUMemoryAvailable() + 1e-12);
}

void MyRSUApp::reserveRSUMemory(uint64_t bytes) {
    const double delta_gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    rsu_memory_available = std::max(0.0, rsu_memory_available - delta_gb);
}

void MyRSUApp::releaseRSUMemory(uint64_t bytes) {
    const double delta_gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    rsu_memory_available = std::min(rsu_memory_total, rsu_memory_available + delta_gb);
}

void MyRSUApp::updateRSUBackgroundLoad() {
    const double now = simTime().dbl();
    const double dt = now - rsu_last_background_update;
    if (dt <= 0.0) {
        return;
    }

    // Small bounded random walk models OS/network/cache housekeeping load.
    rsu_background_cpu_util += uniform(-0.01, 0.01) * dt;
    rsu_background_mem_util += uniform(-0.008, 0.008) * dt;

    rsu_background_cpu_util = std::max(0.04, std::min(0.30, rsu_background_cpu_util));
    rsu_background_mem_util = std::max(0.10, std::min(0.50, rsu_background_mem_util));
    rsu_last_background_update = now;
}

double MyRSUApp::getRSUTaskCpuUtilization() const {
    if (rsu_cpu_total <= 0.0) {
        return 0.0;
    }

    double used_hz = 0.0;
    for (const auto& kv : rsuPendingTasks) {
        used_hz += kv.second.cpu_allocated_hz;
    }

    return std::max(0.0, std::min(1.0, used_hz / (rsu_cpu_total * 1e9)));
}

double MyRSUApp::getEffectiveRSUCpuUtilization() const {
    const double task_cpu_util = getRSUTaskCpuUtilization();
    return std::max(0.0, std::min(1.0, rsu_background_cpu_util + task_cpu_util * (1.0 - rsu_background_cpu_util)));
}

double MyRSUApp::getEffectiveRSUMemoryUtilization() const {
    if (rsu_memory_total <= 0.0) {
        return rsu_background_mem_util;
    }

    const double task_mem_util = std::max(0.0, std::min(1.0, (rsu_memory_total - rsu_memory_available) / rsu_memory_total));
    return std::max(0.0, std::min(1.0, rsu_background_mem_util + task_mem_util * (1.0 - rsu_background_mem_util)));
}

double MyRSUApp::getEffectiveRSUMemoryAvailable() const {
    return std::max(0.0, rsu_memory_total * (1.0 - getEffectiveRSUMemoryUtilization()));
}

double MyRSUApp::getTaskPriorityWeight(double qosValue) const {
    // 3-tier weights approximate 50/30/20 distribution for high/medium/low.
    double qos = std::max(0.0, std::min(1.0, qosValue));
    if (qos >= 0.80) return 5.0; // high priority
    if (qos >= 0.50) return 3.0; // medium priority
    return 2.0;                  // low/background priority
}

void MyRSUApp::tryStartQueuedRSUTasks() {
    while (rsu_processing_count < rsu_max_concurrent && !rsuWaitingQueue.empty()) {
        QueuedRSUTask queued = rsuWaitingQueue.front();
        
        // HIGH PRIORITY FIX: Validate deadline before processing
        // Drop expired tasks to avoid wasting RSU cycles and failing SLA checks
        if (queued.deadline_seconds > 0.0 && simTime().dbl() >= queued.deadline_seconds) {
            rsuWaitingQueue.pop_front();
            rsu_queue_length = static_cast<int>(rsuWaitingQueue.size());
            rsu_tasks_rejected++;  // Count as rejected due to deadline expiry
            
            double expiry_margin = simTime().dbl() - queued.deadline_seconds;
            EV_WARN << "⏱ Deadline already expired for queued task " << queued.task_id
                    << " by " << expiry_margin << "s - dropping without processing" << endl;
            std::cout << "RSU_DEADLINE_EXPIRED: Task " << queued.task_id 
                      << " expired in queue by " << expiry_margin << "s" << std::endl;
            
            insertLifecycleEvent(queued.task_id, "RSU_DEADLINE_EXPIRED_IN_QUEUE",
                "RSU_" + std::to_string(rsu_id), queued.vehicle_id,
                "{\"expiry_margin_s\":" + std::to_string(expiry_margin) + "}");

            // Write FAILED result to Redis so DRL training loop sees it instead of waiting
            // 30 s for its own timeout.  wasted_s = time spent in queue past enqueue.
            if (redis_twin && use_redis) {
                double wasted_s = std::max(0.0, simTime().dbl() - queued.enqueue_time);
                redis_twin->writeSingleResult(
                    queued.task_id, "FAILED", wasted_s, 0.0, "RSU_DEADLINE_EXPIRED_IN_QUEUE");
            }

            // Send failure notification to vehicle
            sendTaskResultToVehicle(queued.task_id, queued.vehicle_id, queued.vehicle_mac,
                                   queued.ingress_rsu_mac, false, 0.0);
            continue;  // Skip to next queued task
        }
        
        rsuWaitingQueue.pop_front();
        rsu_queue_length = static_cast<int>(rsuWaitingQueue.size());

        if (!canReserveRSUMemory(queued.mem_footprint_bytes)) {
            // Head-of-line task cannot fit yet; keep FIFO fairness and wait for next release.
            rsuWaitingQueue.push_front(queued);
            rsu_queue_length = static_cast<int>(rsuWaitingQueue.size());
            break;
        }

        double wait_time = std::max(0.0, simTime().dbl() - queued.enqueue_time);
        rsu_total_queue_time += wait_time;

        rsu_processing_count++;
        rsu_tasks_admitted++;
        reserveRSUMemory(queued.mem_footprint_bytes);
        int concurrent = rsu_processing_count;

        double this_weight = getTaskPriorityWeight(queued.qos_value);
        double cpu_per_task_Hz = (edgeCPU_GHz * 1e9) / std::max(1, concurrent);
        double exec_time = static_cast<double>(queued.cpu_cycles) / cpu_per_task_Hz
                           + processingDelay_ms / 1000.0;

        cMessage* completeMsg = new cMessage("rsuTaskComplete");
        completeMsg->setContextPointer(new std::string(queued.task_id));
        scheduleAt(simTime() + exec_time, completeMsg);

        PendingRSUTask pending;
        pending.vehicle_id = queued.vehicle_id;
        pending.vehicle_mac = queued.vehicle_mac;
        pending.ingress_rsu_mac = queued.ingress_rsu_mac;
        pending.mem_footprint_bytes = queued.mem_footprint_bytes;
        pending.cpu_cycles = queued.cpu_cycles;
        pending.qos_value = queued.qos_value;
        pending.cycles_remaining = static_cast<double>(queued.cpu_cycles);
        pending.exec_time_s = exec_time;
        pending.scheduled_at = simTime().dbl();
        pending.last_reschedule_time = simTime().dbl();
        pending.cpu_allocated_hz = cpu_per_task_Hz;
        pending.completion_event = completeMsg;
        rsuPendingTasks[queued.task_id] = pending;

        // Existing tasks lose/gain share when queued task starts.
        reallocateRSUTasks();
        auto it_after = rsuPendingTasks.find(queued.task_id);
        if (it_after != rsuPendingTasks.end()) {
            cpu_per_task_Hz = it_after->second.cpu_allocated_hz;
        }

        insertLifecycleEvent(queued.task_id, "PROCESSING_STARTED",
            "RSU_" + std::to_string(rsu_id), queued.vehicle_id,
            "{\"from_queue\":true,\"queue_wait_s\":" + std::to_string(wait_time) + ","
            "\"concurrent\":" + std::to_string(concurrent) + ","
            "\"cpu_per_task_ghz\":" + std::to_string(cpu_per_task_Hz/1e9) + "}");

        EV_INFO << "▶ RSU admitted queued task " << queued.task_id
                << " after waiting " << wait_time << "s"
                << " (remaining queue=" << rsuWaitingQueue.size() << ")" << endl;
    }
}
void MyRSUApp::sendTaskResultToVehicle(const std::string& task_id, const std::string& vehicle_id,
                                        LAddress::L2Type vehicle_mac, LAddress::L2Type ingress_rsu_mac,
                                        bool success,
                                        double processing_time) {
    EV_INFO << "📤 RSU: Sending task result to vehicle " << vehicle_id << endl;

    insertLifecycleEvent(task_id, "PROCESSING_COMPLETED",
        "RSU_" + std::to_string(rsu_id), vehicle_id,
        "{\"success\":" + std::string(success ? "true" : "false") + ","
        "\"processing_time_s\":" + std::to_string(processing_time) + "}");
    
    // Create result message
    veins::TaskResultMessage* result = new veins::TaskResultMessage();
    result->setTask_id(task_id.c_str());
    result->setOrigin_vehicle_id(vehicle_id.c_str());
    result->setProcessor_id(("RSU_" + std::to_string(rsu_id)).c_str());
    result->setSuccess(success);
    result->setProcessing_time(processing_time);
    result->setCompletion_time(simTime().dbl());
    uint64_t output_bytes = kResultMinPayloadBytes;
    auto recIt = task_records.find(task_id);
    if (recIt != task_records.end()) {
        output_bytes = std::max<uint64_t>(recIt->second.output_size_bytes, static_cast<uint64_t>(kResultMinPayloadBytes));
    }
    setResultPacketSize(result, output_bytes);
    
    if (success) {
        result->setTask_output_data("RSU_EDGE_RESULT");
        EV_INFO << "  Result: SUCCESS (" << processing_time << "s)" << endl;
    } else {
        std::string failure_reason = "RSU_PROCESSING_FAILED";
        auto recordIt = task_records.find(task_id);
        if (recordIt != task_records.end() && !recordIt->second.failure_reason.empty()) {
            failure_reason = recordIt->second.failure_reason;
        }
        result->setFailure_reason(failure_reason.c_str());
        EV_INFO << "  Result: FAILED" << endl;
    }

    std::string bestRsuId;
    bool inSelfRange = false;
    LAddress::L2Type bestRsuMac = getBestRsuByRssiForVehicle(vehicle_id, &bestRsuId, &inSelfRange);

    // Fallback when RSSI ranking is unavailable (e.g., no twin yet).
    if (bestRsuMac == 0 && ingress_rsu_mac != 0) {
        bestRsuMac = ingress_rsu_mac;
        bestRsuId = "INGRESS";
    }

    if (bestRsuMac == 0) {
        bestRsuMac = myId;
        bestRsuId = "RSU_" + std::to_string(rsu_id);
    }

    if (bestRsuMac != myId) {
        std::string relayedPayload = std::string(kRelayPayloadPrefix) + result->getTask_output_data();
        result->setTask_output_data(relayedPayload.c_str());
        populateWSM(result, bestRsuMac);
        sendDown(result);
        insertLifecycleEvent(task_id, "TASK_RESULTS_SENDING",
            "RSU_" + std::to_string(rsu_id), bestRsuId,
            "{\"success\":" + std::string(success ? "true" : "false") + ","
            "\"processing_time_s\":" + std::to_string(processing_time) + "}");
    } else {
        populateWSM(result, vehicle_mac);
        sendDown(result);
        insertLifecycleEvent(task_id, "TASK_RESULTS_SENDING",
            "RSU_" + std::to_string(rsu_id), vehicle_id,
            "{\"success\":" + std::string(success ? "true" : "false") + ","
            "\"processing_time_s\":" + std::to_string(processing_time) + "}");
    }

    std::cout << "RSU_RESULT: Sent task result for " << task_id
              << " to vehicle " << vehicle_id
              << " (processing_time=" << processing_time << "s)" << std::endl;
}

void MyRSUApp::handleRSUTaskResultRelay(TaskResultMessage* msg) {
    const std::string task_id = msg->getTask_id();
    const std::string vehicle_id = msg->getOrigin_vehicle_id();

    // Check if this is an agent sub-task result from a neighbor RSU
    size_t sep = task_id.find("::");
    if (sep != std::string::npos && redis_twin && use_redis) {
        std::string orig_id    = task_id.substr(0, sep);
        std::string agent_name = task_id.substr(sep + 2);

        bool success          = msg->getSuccess();
        double proc_time      = msg->getProcessing_time();
        // Approximate one-hop V2I transmission time (10ms)
        double total_latency  = proc_time + 0.01;

        // Approximate RSU energy (κ_rsu=2e-27, f=4GHz nominal for neighbor)
        constexpr double kRsuNominalHz = 4e9;
        double energy_j = 2e-27 * kRsuNominalHz * kRsuNominalHz;  // placeholder per-cycle, scale later

        std::string status = "FAILED";
        if (success) {
            status = "COMPLETED_ON_TIME";
            if (task_records.count(orig_id)) {
                double deadline = task_records.at(orig_id).deadline_seconds;
                status = (total_latency <= deadline) ? "COMPLETED_ON_TIME" : "FAILED";
            }
        }
        std::string fail_reason_relay = !success ? "EXECUTION_FAILED"
                                       : (status == "FAILED") ? "DEADLINE_MISSED" : "NONE";

        redis_twin->writeSingleResult(orig_id, status, total_latency, energy_j, fail_reason_relay);
        std::cout << "RSU_RELAY_RESULT: task=" << orig_id
                  << " status=" << status << " reason=" << fail_reason_relay
                  << " latency=" << total_latency << "s" << std::endl;
        delete msg;
        return;
    }
    auto macIt = vehicle_macs.find(vehicle_id);
    if (macIt == vehicle_macs.end()) {
        EV_WARN << "RSU relay received task result but vehicle MAC is unknown for " << vehicle_id << endl;
        delete msg;
        return;
    }

    std::string cleanedPayload = stripRelayPrefix(msg->getTask_output_data());
    msg->setTask_output_data(cleanedPayload.c_str());
    populateWSM(msg, macIt->second);
    sendDown(msg);
    insertLifecycleEvent(task_id, "RSU_RESULT_RELAY_TO_VEHICLE",
        "RSU_" + std::to_string(rsu_id), vehicle_id,
        "{\"relay\":true}");
}

void MyRSUApp::handleServiceVehicleResultRelay(TaskResultMessage* msg, LAddress::L2Type targetRsuMac) {
    const std::string task_id = msg->getTask_id();
    const std::string vehicle_id = msg->getOrigin_vehicle_id();

    // Keep relay metadata for intermediate RSU hops and strip it only at final egress RSU.
    LAddress::L2Type parsedTarget = 0;
    std::string cleanPayload;
    if (parseServiceResultRelayHint(msg->getTask_output_data(), parsedTarget, cleanPayload)) {
        if (targetRsuMac == 0) {
            targetRsuMac = parsedTarget;
        }
    }

    // ── Agent sub-task intercept ───────────────────────────────────────────
    // If the task_id contains "::" it's a DRL baseline agent sub-task dispatched
    // by the task vehicle.  Write the metrics to Redis for DRL training — do NOT
    // forward to the vehicle (it doesn't track sub-tasks).
    size_t sep = task_id.find("::");
    if (sep != std::string::npos && redis_twin && use_redis) {
        // Only handle here if we are the anchor RSU (targetRsuMac == myId or 0).
        // If targetRsuMac points to a DIFFERENT RSU, forward first.
        if (targetRsuMac != 0 && targetRsuMac != myId) {
            populateWSM(msg, targetRsuMac);
            sendDown(msg);
            return;
        }
        std::string orig_id    = task_id.substr(0, sep);
        std::string agent_name = task_id.substr(sep + 2);
        bool success           = msg->getSuccess();
        double proc_time       = msg->getProcessing_time();
        double total_latency   = proc_time + 0.01; // +10ms V2V/V2I tx estimate

        std::string status = "FAILED";
        if (success) {
            status = "COMPLETED_ON_TIME";
            if (task_records.count(orig_id)) {
                double deadline = task_records.at(orig_id).deadline_seconds;
                status = (total_latency <= deadline) ? "COMPLETED_ON_TIME" : "FAILED";
            }
        }
        // SV energy: κ_v=5e-27, f≈5 GHz, cycles=actual from task
        double energy_j = 0.0;
        if (success && task_records.count(orig_id)) {
            double f_hz = 5e9;  // Service vehicle CPU ~5 GHz
            energy_j = 5e-27 * f_hz * f_hz * static_cast<double>(task_records.at(orig_id).cpu_cycles);
        }
        std::string fail_reason = !success ? "EXECUTION_FAILED"
                                : (status == "FAILED") ? "DEADLINE_MISSED" : "NONE";

        redis_twin->writeSingleResult(orig_id, status, total_latency, energy_j, fail_reason);
        std::cout << "RSU_SV_RESULT: task=" << orig_id
                  << " status=" << status << " reason=" << fail_reason
                  << " latency=" << total_latency << "s" << std::endl;
        delete msg;
        return;
    }
    if (targetRsuMac != 0 && targetRsuMac != myId) {
        populateWSM(msg, targetRsuMac);
        sendDown(msg);
        insertLifecycleEvent(task_id, "SV_RESULT_FORWARD_TO_TARGET_RSU",
            "RSU_" + std::to_string(rsu_id), "RSU",
            std::string("{\"target_rsu_mac\":") + std::to_string(targetRsuMac) + "}");
        return;
    }

    auto macIt = vehicle_macs.find(vehicle_id);
    if (macIt == vehicle_macs.end()) {
        EV_WARN << "SV result relay: vehicle MAC unknown for " << vehicle_id << endl;
        delete msg;
        return;
    }

    if (!cleanPayload.empty()) {
        msg->setTask_output_data(cleanPayload.c_str());
    }

    populateWSM(msg, macIt->second);
    sendDown(msg);
    insertLifecycleEvent(task_id, "SV_RESULT_DELIVERED_TO_VEHICLE",
        "RSU_" + std::to_string(rsu_id), vehicle_id,
        "{\"relay\":true}");
}

void MyRSUApp::insertLifecycleEvent(const std::string& task_id, const std::string& event_type,
                                     const std::string& source, const std::string& target,
                                     const std::string& details) {
    (void)details;
    if (terminalVerboseLogs || shouldAlwaysPrintLifecycleEvent(event_type)) {
        std::cout << "LIFECYCLE: " << event_type << " task=" << task_id
                  << " (" << source << "->" << target << ")" << std::endl;
    }
}

void MyRSUApp::handleTaskOffloadingEvent(veins::TaskOffloadingEvent* msg) {
    EV_DEBUG << "📊 RSU: Received task offloading event" << endl;
    
    std::string task_id = msg->getTask_id();
    std::string event_type = msg->getEvent_type();
    std::string source = msg->getSource_entity_id();
    std::string target = msg->getTarget_entity_id();
    double event_time = msg->getEvent_time();
    
    EV_DEBUG << "  Task: " << task_id << endl;
    EV_DEBUG << "  Event: " << event_type << endl;
    EV_DEBUG << "  Source: " << source << " → Target: " << target << endl;
    EV_DEBUG << "  Time: " << event_time << "s" << endl;

    // Persist lifecycle events to Redis stream for real-time dashboard consumers.
    // This handler is the common path for TaskOffloadingEvent in handleMessage(),
    // so writing here prevents missing stream entries when onWSM() is bypassed.
    if (redis_twin && use_redis) {
        redis_twin->appendTaskLifecycleEvent(
            task_id,
            event_type,
            event_time,
            source,
            target,
            msg->getEvent_details()
        );
    } else {
        EV_WARN << "Lifecycle event not written to Redis (disabled or disconnected): "
                << event_type << " task=" << task_id << endl;
    }
    
    // Store event in Digital Twin database
    insertTaskOffloadingEvent(msg);
    
    if (terminalVerboseLogs || shouldAlwaysPrintLifecycleEvent(event_type)) {
        std::cout << "RSU_EVENT: " << event_type << " for task " << task_id
                  << " (" << source << " → " << target << ")" << std::endl;
    }
    
    delete msg;
}

void MyRSUApp::handleTaskResultWithCompletion(TaskResultMessage* msg) {
    std::string task_id = msg->getTask_id();
    EV_INFO << "📊 RSU received task completion data for " << task_id << endl;

    // Check if this is an agent sub-task result (service vehicle or remote execution)
    size_t sep = task_id.find("::");
    if (sep != std::string::npos && redis_twin && use_redis) {
        std::string orig_id    = task_id.substr(0, sep);
        std::string agent_name = task_id.substr(sep + 2);

        bool success         = msg->getSuccess();
        double proc_time     = msg->getProcessing_time();
        double total_latency = proc_time + 0.01; // +10ms V2V/V2I tx estimate

        std::string status = "FAILED";
        if (success) {
            status = "COMPLETED_ON_TIME";
            if (task_records.count(orig_id)) {
                double deadline = task_records.at(orig_id).deadline_seconds;
                status = (total_latency <= deadline) ? "COMPLETED_ON_TIME" : "FAILED";
            }
        }
        // Energy for service vehicle execution: κ_v=5e-27, f≈5 GHz, cycles=actual from task
        double energy_j = 0.0;
        if (success && task_records.count(orig_id)) {
            double f_hz = 5e9;  // Service vehicle CPU ~5 GHz
            energy_j = 5e-27 * f_hz * f_hz * static_cast<double>(task_records.at(orig_id).cpu_cycles);
        }
        std::string fail_reason_sv = !success ? "EXECUTION_FAILED"
                                    : (status == "FAILED") ? "DEADLINE_MISSED" : "NONE";

        redis_twin->writeSingleResult(orig_id, status, total_latency, energy_j, fail_reason_sv);
        std::cout << "RSU_SV_RESULT2: task=" << orig_id
                  << " status=" << status << " reason=" << fail_reason_sv
                  << " latency=" << total_latency << "s" << std::endl;
        delete msg;
        return;
    }

    // Parse timing JSON from task_output_data field
    std::string result_data = msg->getTask_output_data();
    
    try {
        // Manual JSON parsing (simple approach)
        double request_time = 0.0, decision_time = 0.0, start_time = 0.0, completion_time = 0.0;
        double qos_value = 0.0;
        uint64_t mem_footprint_bytes = 0, cpu_cycles = 0;
        bool on_time = false;
        std::string decision_type, processor_id, result;

        // Extract values from JSON string (simple parsing)
        size_t pos = 0;

        // Helper lambda to extract numeric value
        auto extractDouble = [&result_data](const std::string& key) -> double {
            size_t keyPos = result_data.find("\"" + key + "\":");
            if (keyPos == std::string::npos) return 0.0;
            size_t valueStart = result_data.find(":", keyPos) + 1;
            size_t valueEnd = result_data.find_first_of(",}", valueStart);
            std::string valueStr = result_data.substr(valueStart, valueEnd - valueStart);
            return std::stod(valueStr);
        };

        auto extractUint64 = [&result_data](const std::string& key) -> uint64_t {
            size_t keyPos = result_data.find("\"" + key + "\":");
            if (keyPos == std::string::npos) return 0;
            size_t valueStart = result_data.find(":", keyPos) + 1;
            size_t valueEnd = result_data.find_first_of(",}", valueStart);
            std::string valueStr = result_data.substr(valueStart, valueEnd - valueStart);
            return std::stoull(valueStr);
        };

        auto extractString = [&result_data](const std::string& key) -> std::string {
            size_t keyPos = result_data.find("\"" + key + "\":");
            if (keyPos == std::string::npos) return "";
            size_t valueStart = result_data.find("\"", keyPos + key.length() + 3) + 1;
            size_t valueEnd = result_data.find("\"", valueStart);
            return result_data.substr(valueStart, valueEnd - valueStart);
        };

        auto extractBool = [&result_data](const std::string& key) -> bool {
            size_t keyPos = result_data.find("\"" + key + "\":");
            if (keyPos == std::string::npos) return false;
            size_t valueStart = result_data.find(":", keyPos) + 1;
            std::string valueStr = result_data.substr(valueStart, 4);
            return (valueStr.find("true") != std::string::npos);
        };

        // Extract all timing values
        request_time = extractDouble("request_time");
        decision_time = extractDouble("decision_time");
        start_time = extractDouble("start_time");
        completion_time = extractDouble("completion_time");
        qos_value = extractDouble("qos_value");
        mem_footprint_bytes = extractUint64("mem_footprint_bytes");
        cpu_cycles = extractUint64("cpu_cycles");
        on_time = extractBool("on_time");
        decision_type = extractString("decision_type");
        processor_id = extractString("processor_id");
        result = extractString("result");
        
        bool success = msg->getSuccess();
        
        EV_INFO << "Parsed completion data:" << endl;
        EV_INFO << "  Request time: " << request_time << "s" << endl;
        EV_INFO << "  Decision time: " << decision_time << "s" << endl;
        EV_INFO << "  Start time: " << start_time << "s" << endl;
        EV_INFO << "  Completion time: " << completion_time << "s" << endl;
        EV_INFO << "  Decision type: " << decision_type << endl;
        EV_INFO << "  Processor: " << processor_id << endl;
        EV_INFO << "  Success: " << (success ? "Yes" : "No") << endl;
        EV_INFO << "  On-time: " << (on_time ? "Yes" : "No") << endl;
        
        // Get vehicle and RSU info
        std::string origin_vehicle_id = msg->getOrigin_vehicle_id();
        
        // Calculate deadline from timing
        double total_latency = completion_time - request_time;
        double deadline_seconds = total_latency * 1.5;  // Estimate deadline (could be extracted from original request)
        
        // Insert into offloaded_task_completions only for actual offloads (RSU / SERVICE_VEHICLE).
        // LOCAL completions are tracked in consolidated task_static_metadata/task_runtime_events.
        if (decision_type != "LOCAL") {
            insertOffloadedTaskCompletion(
                task_id,
                origin_vehicle_id,
                decision_type,
                processor_id,
                request_time,
                decision_time,
                start_time,
                completion_time,
                success,
                on_time,
                deadline_seconds,
                mem_footprint_bytes,
                cpu_cycles,
                qos_value,
                result_data,
                msg->getFailure_reason()
            );
        }
        
        std::cout << "COMPLETION_DB: Task " << task_id << " completion data inserted - "
                  << "Total latency: " << total_latency << "s, Success: " << success 
                  << ", On-time: " << on_time << std::endl;

        // Update Redis task status with full timing and decision data
        if (redis_twin && use_redis) {
            std::string status = on_time ? "COMPLETED_ON_TIME" : "COMPLETED_LATE";
            double processing_time = completion_time - start_time;
            redis_twin->updateTaskCompletion(
                task_id,
                status,
                decision_type,
                processor_id,
                processing_time,
                total_latency,
                completion_time
            );
            std::cout << "REDIS_UPDATE: Task " << task_id << " status -> " << status
                      << " decision_type=" << decision_type
                      << " processor=" << processor_id
                      << " latency=" << total_latency << "s" << std::endl;

            // Write DDQN agent metrics for DRL training.
            // The real task IS the DDQN decision's execution, so its metrics
            // are the ground truth for the "ddqn" agent in the results hash.
            {
                std::string ddqn_status = on_time ? "COMPLETED_ON_TIME" : "FAILED";
                std::string ddqn_reason = on_time ? "NONE" : "DEADLINE_MISSED";
                
                // --- PENALIZE FALLBACKS ---
                // If the DDQN algorithm originally requested an SV offload, but it 
                // failed the TV's safety gate and fell back to the RSU, this execution
                // was technically a failure of the agent's policy.
                std::string original_ddqn_type = redis_twin->getAgentDecisionType(task_id, "ddqn");
                if (original_ddqn_type == "SERVICE_VEHICLE" && decision_type == "RSU") {
                    std::cout << "DDQN_PENALTY: Task " << task_id << " was bailed out by fallback. Penalizing DDQN agent." << std::endl;
                    ddqn_status = "FAILED";
                    ddqn_reason = "SV_FALLBACK_TO_RSU";
                    // Override the latency to be worse than the deadline so the agent receives negative reward
                    total_latency = deadline_seconds * 1.5; 
                }

                // Energy estimate: κ * f² * cycles  (CMOS dynamic power model)
                double energy_j = 0.0;
                if (decision_type == "RSU") {
                    // RSU: κ_rsu=2e-27, f ≈ 4 GHz
                    energy_j = 2e-27 * 4e9 * 4e9 * static_cast<double>(cpu_cycles);
                } else {
                    // SV: κ_v=5e-27, f ≈ 5 GHz, cycles=actual from task
                    double f_hz = 5e9;  // Service vehicle CPU ~5 GHz
                    energy_j = 5e-27 * f_hz * f_hz * static_cast<double>(cpu_cycles);
                }
                redis_twin->writeSingleResult(task_id, ddqn_status, total_latency, energy_j, ddqn_reason);
                if (terminalVerboseLogs || ddqn_status == "FAILED") {
                    std::cout << "RSU_OFFLOAD_RESULT: task=" << task_id
                              << " status=" << ddqn_status << " reason=" << ddqn_reason
                              << " latency=" << total_latency << "s" << std::endl;
                }
            }
        }

    } catch (const std::exception& e) {
        EV_ERROR << "Failed to parse completion data JSON: " << e.what() << endl;
        std::cout << "ERROR: Failed to parse completion data for task " << task_id 
                  << ": " << e.what() << std::endl;
    }
    
    delete msg;
}

// ============================================================================
// RSU STATUS AND METADATA TRACKING
// ============================================================================

void MyRSUApp::sendRSUStatusUpdate() {
    EV_DEBUG << "📊 RSU: Sending status update to database" << endl;

    updateRSUBackgroundLoad();
    const double cpu_util = getEffectiveRSUCpuUtilization();
    const double effective_cpu_available = rsu_cpu_total * (1.0 - cpu_util);
    const double effective_memory_available = getEffectiveRSUMemoryAvailable();
    rsu_cpu_available = effective_cpu_available;
    
    // Update dynamic metrics
    double avg_proc_time = (rsu_tasks_processed > 0) ? (rsu_total_processing_time / rsu_tasks_processed) : 0.0;
    double avg_queue_time = (rsu_tasks_processed > 0) ? (rsu_total_queue_time / rsu_tasks_processed) : 0.0;
    double success_rate = (rsu_tasks_admitted > 0) ? ((double)rsu_tasks_processed / rsu_tasks_admitted) : 0.0;

    // Accounting sanity check:
    // arrived == admitted + rejected + currently_queued
    int accounted = rsu_tasks_admitted + rsu_tasks_rejected + rsu_queue_length;
    if (accounted != rsu_tasks_arrived) {
        EV_WARN << "RSU counter mismatch: arrived=" << rsu_tasks_arrived
                << " accounted=" << accounted
                << " (admitted=" << rsu_tasks_admitted
                << ", rejected=" << rsu_tasks_rejected
                << ", queued=" << rsu_queue_length
                << ", in_flight=" << rsu_processing_count << ")" << endl;
    }
    
    // Update Redis with RSU state (include position)
    if (redis_twin && use_redis) {
        std::string rsu_id_str = "RSU_" + std::to_string(rsu_id);
        
        // Get RSU position from mobility module
        double pos_x = 0.0, pos_y = 0.0;
        cModule* mobilityModule = getParentModule()->getSubmodule("mobility");
        if (mobilityModule) {
            pos_x = mobilityModule->par("x").doubleValue();
            pos_y = mobilityModule->par("y").doubleValue();
        }
        
        redis_twin->updateRSUResources(
            rsu_id_str,
            effective_cpu_available,
            effective_memory_available,
            rsu_queue_length,
            rsu_processing_count,
            simTime().dbl(),
            pos_x,
            pos_y,
            cpu_util,  // written directly so DRL reads it without needing cpu_total
            100.0      // RSU energy level fixed at 100% (grid-powered infrastructure)
        );
    }
    
    // Insert into database
    insertRSUStatus();
    
    // Reschedule
    scheduleAt(simTime() + rsu_status_update_interval, rsu_status_update_timer);
}

void MyRSUApp::insertRSUStatus() {
    // Persistence disabled in this build.
}

void MyRSUApp::insertRSUMetadata() {
    // Persistence disabled in this build.
}

void MyRSUApp::insertVehicleMetadata(const std::string& vehicle_id) {
    (void)vehicle_id;
}


void MyRSUApp::broadcastRSUStatus() {
    EV_DEBUG << "📡 RSU broadcasting status to neighbors..." << endl;

    updateRSUBackgroundLoad();
    const double cpu_util = getEffectiveRSUCpuUtilization();
    const double effective_cpu_available = rsu_cpu_total * (1.0 - cpu_util);
    const double effective_memory_available = getEffectiveRSUMemoryAvailable();
    rsu_cpu_available = effective_cpu_available;
    
    // Create RSU status broadcast message
    RSUStatusBroadcastMessage* msg = new RSUStatusBroadcastMessage();
    
    // RSU identification
    std::string rsu_id_str = "RSU_" + std::to_string(rsu_id);
    msg->setRsu_id(rsu_id_str.c_str());
    msg->setRsu_mac(myId);
    msg->setBroadcast_time(simTime().dbl());
    
    // Resource state
    msg->setQueue_length(rsu_queue_length);
    msg->setProcessing_count(rsu_processing_count);
    msg->setMax_concurrent_tasks(rsu_max_concurrent);
    msg->setCpu_available_ghz(effective_cpu_available);
    msg->setCpu_total_ghz(rsu_cpu_total);
    msg->setMemory_available_gb(effective_memory_available);
    msg->setMemory_total_gb(rsu_memory_total);
    
    // Coverage information with full vehicle details
    // Include only vehicles currently owned by self range, limit list size to avoid huge packets
    std::vector<VehicleDetailInBroadcast> vehicle_details_list;
    std::vector<std::string> vehicle_list;
    int count = 0;
    for (const auto& pair : vehicle_twins) {
        if (count >= max_vehicles_in_broadcast) break;
        refreshVehicleCoverageRecord(pair.first);
        auto covIt = vehicle_coverage_records.find(pair.first);
        if (covIt != vehicle_coverage_records.end() && covIt->second.in_self_range) {
            // Extract full vehicle detail from VehicleDigitalTwin
            const VehicleDigitalTwin& vdt = pair.second;
            VehicleDetailInBroadcast detail;
            detail.setVehicle_id(vdt.vehicle_id.c_str());
            detail.setPos_x(vdt.pos_x);
            detail.setPos_y(vdt.pos_y);
            detail.setSpeed(vdt.speed);
            detail.setHeading(vdt.heading);
            detail.setCpu_available(vdt.cpu_available);
            detail.setCpu_utilization(vdt.cpu_utilization);
            detail.setMem_available(vdt.mem_available);
            detail.setMem_utilization(vdt.mem_utilization);
            detail.setQueue_length(vdt.current_queue_length);

            vehicle_details_list.push_back(detail);
            vehicle_list.push_back(pair.first);
            count++;
        }
    }

    msg->setVehicle_details_in_coverageArraySize(vehicle_details_list.size());
    for (size_t i = 0; i < vehicle_details_list.size(); i++) {
        msg->setVehicle_details_in_coverage(i, vehicle_details_list[i]);
    }
    msg->setVehicle_ids_in_coverageArraySize(vehicle_list.size());
    for (size_t i = 0; i < vehicle_list.size(); i++) {
        msg->setVehicle_ids_in_coverage(i, vehicle_list[i].c_str());
    }
    msg->setVehicle_count(vehicle_twins.size());
    
    // Position (from mobility or parameters)
    cModule* mobilityModule = getParentModule()->getSubmodule("mobility");
    if (mobilityModule) {
        msg->setPos_x(mobilityModule->par("x").doubleValue());
        msg->setPos_y(mobilityModule->par("y").doubleValue());
    } else {
        msg->setPos_x(0.0);
        msg->setPos_y(0.0);
    }
    
    // Broadcast to all RSU neighbors (wireless for now, will be wired backhaul later)
    populateWSM(msg);  // Broadcast
    sendDown(msg);
    
    EV_DEBUG << "  Broadcast: queue=" << rsu_queue_length 
             << ", processing=" << rsu_processing_count
             << ", cpu_avail=" << effective_cpu_available << " GHz"
             << ", vehicles=" << vehicle_twins.size() << endl;
    
    std::cout << "RSU_BROADCAST: " << rsu_id_str << " status - Q:" << rsu_queue_length 
              << " P:" << rsu_processing_count << " CPU:" << effective_cpu_available << "GHz" << std::endl;
}

void MyRSUApp::handleRSUStatusBroadcast(veins::RSUStatusBroadcastMessage* msg) {
    std::string neighbor_rsu_id = msg->getRsu_id();
    
    // Don't process our own broadcasts
    std::string our_rsu_id = "RSU_" + std::to_string(rsu_id);
    if (neighbor_rsu_id == our_rsu_id) {
        delete msg;
        return;
    }
    
    EV_DEBUG << "📥 Processing status from neighbor " << neighbor_rsu_id << endl;
    
    // Build neighbor state structure
    RSUNeighborState state;
    state.rsu_id = neighbor_rsu_id;
    state.rsu_mac = msg->getRsu_mac();
    state.last_update_time = simTime().dbl();
    
    state.queue_length = msg->getQueue_length();
    state.processing_count = msg->getProcessing_count();
    state.max_concurrent_tasks = msg->getMax_concurrent_tasks();
    state.cpu_available_ghz = msg->getCpu_available_ghz();
    state.cpu_total_ghz = msg->getCpu_total_ghz();
    state.memory_available_gb = msg->getMemory_available_gb();
    state.memory_total_gb = msg->getMemory_total_gb();
    
    state.vehicle_count = msg->getVehicle_count();
    state.vehicle_details_in_coverage.clear();
    for (size_t i = 0; i < msg->getVehicle_details_in_coverageArraySize(); i++) {
        const VehicleDetailInBroadcast& detail = msg->getVehicle_details_in_coverage(i);
        VehicleDetail vd;
        vd.vehicle_id = detail.getVehicle_id();
        vd.pos_x = detail.getPos_x();
        vd.pos_y = detail.getPos_y();
        vd.speed = detail.getSpeed();
        vd.heading = detail.getHeading();
        vd.cpu_available = detail.getCpu_available();
        vd.cpu_utilization = detail.getCpu_utilization();
        vd.mem_available = detail.getMem_available();
        vd.mem_utilization = detail.getMem_utilization();
        vd.queue_length = detail.getQueue_length();
        state.vehicle_details_in_coverage.push_back(vd);
    }
    state.vehicle_ids_in_coverage.clear();
    for (size_t i = 0; i < msg->getVehicle_ids_in_coverageArraySize(); i++) {
        state.vehicle_ids_in_coverage.push_back(msg->getVehicle_ids_in_coverage(i));
    }
    
    state.pos_x = msg->getPos_x();
    state.pos_y = msg->getPos_y();
    
    // Compute derived metrics
    double cpu_util = (state.cpu_total_ghz > 0) ? 
                      ((state.cpu_total_ghz - state.cpu_available_ghz) / state.cpu_total_ghz) : 0.0;
    state.load_factor = cpu_util;
    
    // Simple overload heuristic: queue near max OR processing at max OR CPU < 10%
    state.is_overloaded = (state.queue_length >= (state.max_concurrent_tasks * 0.8)) ||
                          (state.processing_count >= state.max_concurrent_tasks) ||
                          (state.cpu_available_ghz < (state.cpu_total_ghz * 0.1));
    
    // Update or insert neighbor state
    updateNeighborState(state);

    // Persist neighbor RSU state to OUR Redis DB so DRL can read all RSU states
    // from its single DB without needing cross-DB queries.
    if (redis_twin && use_redis) {
        redis_twin->updateRSUResources(
            neighbor_rsu_id,
            state.cpu_available_ghz,    // same unit as local (GHz)
            state.memory_available_gb,  // same unit as local (GB)
            state.queue_length,
            state.processing_count,
            simTime().dbl(),
            state.pos_x,
            state.pos_y,
            state.load_factor,          // cpu_utilization (0-1)
            100.0                       // RSU energy level fixed at 100%
        );
    }

    // Refresh ownership records now that neighbor position/coverage set has changed.
    for (const auto& v : state.vehicle_details_in_coverage) {
        refreshVehicleCoverageRecord(v.vehicle_id);
    }
    for (const auto& v : state.vehicle_ids_in_coverage) {
        refreshVehicleCoverageRecord(v);
    }
    EV_DEBUG << "  Neighbor " << neighbor_rsu_id << ": "
             << "Q=" << state.queue_length << "/" << state.max_concurrent_tasks
             << ", CPU=" << state.cpu_available_ghz << "/" << state.cpu_total_ghz << " GHz"
             << ", overloaded=" << (state.is_overloaded ? "YES" : "NO") << endl;
    
    std::cout << "RSU_NEIGHBOR_UPDATE: " << neighbor_rsu_id 
              << " Q:" << state.queue_length 
              << " P:" << state.processing_count
              << " CPU:" << state.cpu_available_ghz << "GHz"
              << " overload:" << (state.is_overloaded ? "YES" : "NO") << std::endl;
    
    delete msg;
}

void MyRSUApp::updateNeighborState(const RSUNeighborState& state) {
    neighbor_rsus[state.rsu_id] = state;
    
    EV_DEBUG << "✓ Updated neighbor cache for " << state.rsu_id 
             << " (total neighbors: " << neighbor_rsus.size() << ")" << endl;
}

bool MyRSUApp::isNeighborStateFresh(const RSUNeighborState& state) {
    double age = simTime().dbl() - state.last_update_time;
    return age <= neighbor_state_ttl;
}

void MyRSUApp::cleanupStaleNeighborStates() {
    auto it = neighbor_rsus.begin();
    int removed = 0;
    
    while (it != neighbor_rsus.end()) {
        if (!isNeighborStateFresh(it->second)) {
            EV_DEBUG << "Removing stale neighbor state for " << it->first << endl;
            it = neighbor_rsus.erase(it);
            removed++;
        } else {
            ++it;
        }
    }
    
    if (removed > 0) {
        EV_DEBUG << "Cleaned up " << removed << " stale neighbor state(s)" << endl;
    }
}

// ============================================================================
// AGENT SUB-TASK DISPATCH
// For each DRL baseline agent, the RSU creates a sub-task with ID
// "{original_task_id}::{agent_name}" and routes it to the agent's chosen
// target. When the sub-task completes the RSU writes per-agent results to
// Redis (task:{orig_id}:results) instead of forwarding to the vehicle.
// ============================================================================
void MyRSUApp::dispatchAgentSubTask(const TaskRecord& record,
                                     const std::string& agent_name,
                                     const std::string& target_type,
                                     const std::string& target_id)
{
    if (!redis_twin || !use_redis) return;

    std::string sub_id     = record.task_id + "::" + agent_name;
    std::string this_rsu   = "RSU_" + std::to_string(rsu_id);

    if (target_type == "RSU") {
        if (target_id == this_rsu || target_id.empty()) {
            // ── Target is THIS RSU: truly execute in the local queue ──────
            if (rsu_processing_count >= rsu_max_concurrent || !canReserveRSUMemory(record.mem_footprint_bytes)) {
                redis_twin->writeTaskResults(record.task_id, agent_name, "FAILED",
                                             record.deadline_seconds, 0.0, "RSU_QUEUE_FULL");
                trackAgentResult(agent_name, "FAILED");
                std::cout << "AGENT_SUBTASK: " << sub_id << " REJECTED (RSU full)" << std::endl;
                return;
            }

            rsu_processing_count++;
            rsu_tasks_admitted++;
            reserveRSUMemory(record.mem_footprint_bytes);

            // Create pending task structure
            PendingRSUTask p;
            p.vehicle_id            = record.vehicle_id;
            p.vehicle_mac           = 0;          // sub-task: no vehicle notification
            p.ingress_rsu_mac       = myId;
            p.mem_footprint_bytes   = record.mem_footprint_bytes;
            p.cpu_cycles            = record.cpu_cycles;
            p.cycles_remaining      = static_cast<double>(record.cpu_cycles);
            p.qos_value             = record.qos_value;
            p.scheduled_at          = simTime().dbl();
            p.last_reschedule_time  = simTime().dbl();

            // Create completion event for this task
            cMessage* doneMsg = new cMessage("rsuTaskComplete");
            doneMsg->setContextPointer(new std::string(sub_id));
            p.completion_event      = doneMsg;

            // Add to pending tasks
            rsuPendingTasks[sub_id] = p;

            // Recompute CPU allocation for all tasks (including this new one)
            // This function internally schedules completion events
            reallocateRSUTasks();

            // Extract computed exec_time for logging/debug
            double exec_time = rsuPendingTasks[sub_id].exec_time_s;

            std::cout << "AGENT_SUBTASK: " << sub_id
                      << " queued on " << this_rsu
                      << " exec=" << exec_time << "s" << std::endl;

        } else {
            // ── Target is a NEIGHBOR RSU: forward via TaskOffloadPacket ───
            auto it = neighbor_rsus.find(target_id);
            if (it == neighbor_rsus.end()) {
                redis_twin->writeSingleResult(record.task_id, "FAILED",
                                              record.deadline_seconds, 0.0, "NEIGHBOR_RSU_UNKNOWN");
                std::cout << "AGENT_SUBTASK: " << sub_id
                          << " FAILED (neighbor " << target_id << " unknown)" << std::endl;
                return;
            }

            LAddress::L2Type neighbor_mac = it->second.rsu_mac;
            // Route hint: ingress=this RSU, processor=neighbor RSU
            // Result will be relayed back to myId (ingress) → handleRSUTaskResultRelay()
            std::string route = std::string("ROUTE|ingress=") + std::to_string(myId)
                              + "|processor=" + std::to_string(neighbor_mac) + "|";

            veins::TaskOffloadPacket* pkt = new veins::TaskOffloadPacket();
            pkt->setTask_id(sub_id.c_str());
            pkt->setOrigin_vehicle_id(record.vehicle_id.c_str());
            pkt->setOrigin_vehicle_mac(myId); // result routes back to this RSU
            pkt->setOffload_time(simTime().dbl());
            pkt->setMem_footprint_bytes(record.mem_footprint_bytes);
            pkt->setCpu_cycles(record.cpu_cycles);
            pkt->setDeadline_seconds(record.deadline_seconds);
            pkt->setQos_value(record.qos_value);
            pkt->setTask_input_data(route.c_str());
            populateWSM(pkt, neighbor_mac);
            sendDown(pkt);

            std::cout << "AGENT_SUBTASK: " << sub_id
                      << " forwarded to " << target_id << std::endl;
        }

    } else if (target_type == "SERVICE_VEHICLE") {
        // ── Target is a service vehicle: relay via TaskOffloadPacket ──────
        auto macIt = vehicle_macs.find(target_id);
        if (macIt == vehicle_macs.end()) {
            redis_twin->writeSingleResult(record.task_id, "FAILED",
                                          record.deadline_seconds, 0.0, "SV_MAC_UNKNOWN");
            std::cout << "AGENT_SUBTASK: " << sub_id
                      << " FAILED (SV " << target_id << " MAC unknown)" << std::endl;
            return;
        }

        LAddress::L2Type sv_mac = macIt->second;
        // Service vehicle route hint: sv=<sv_mac>|anchor=<this_rsu>
        // Result will relay back through this RSU → handleServiceVehicleResultRelay()
        std::string svRoute = std::string("SVROUTE|sv=") + std::to_string(sv_mac)
                            + "|anchor=" + std::to_string(myId) + "|";

        veins::TaskOffloadPacket* pkt = new veins::TaskOffloadPacket();
        pkt->setTask_id(sub_id.c_str());
        pkt->setOrigin_vehicle_id(record.vehicle_id.c_str());
        pkt->setOrigin_vehicle_mac(myId);
        pkt->setOffload_time(simTime().dbl());
        pkt->setMem_footprint_bytes(record.mem_footprint_bytes);
        pkt->setCpu_cycles(record.cpu_cycles);
        pkt->setDeadline_seconds(record.deadline_seconds);
        pkt->setQos_value(record.qos_value);
        pkt->setTask_input_data(svRoute.c_str());
        populateWSM(pkt, sv_mac);
        sendDown(pkt);

        std::cout << "AGENT_SUBTASK: " << sub_id
                  << " relayed to SV " << target_id << std::endl;
        sv_subtask_pending_[sub_id] = simTime();

    } else {
        // Unknown target type → write FAILED immediately
        redis_twin->writeSingleResult(record.task_id, "FAILED",
                                      record.deadline_seconds, 0.0, "UNKNOWN_TARGET");
        std::cout << "AGENT_SUBTASK: " << sub_id
                  << " FAILED (unknown target_type=" << target_type << ")" << std::endl;
    }
}


}  // namespace complex_network
