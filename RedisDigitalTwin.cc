#include "RedisDigitalTwin.h"
#include <cmath>
#include <sstream>
#include <algorithm>

namespace complex_network {

RedisDigitalTwin::RedisDigitalTwin(const std::string& host, int port, int db)
    : redis_host(host), redis_port(port), redis_db(db) {
    connect();
}

RedisDigitalTwin::~RedisDigitalTwin() {
    disconnect();
}

bool RedisDigitalTwin::connect() {
    struct timeval timeout = { 1, 500000 }; // 1.5 seconds
    redis_ctx = redisConnectWithTimeout(redis_host.c_str(), redis_port, timeout);
    
    if (redis_ctx == nullptr || redis_ctx->err) {
        if (redis_ctx) {
            EV_ERROR << "Redis connection error: " << redis_ctx->errstr << std::endl;
            redisFree(redis_ctx);
            redis_ctx = nullptr;
        } else {
            EV_ERROR << "Redis connection error: can't allocate context" << std::endl;
        }
        is_connected = false;
        return false;
    }
    
    // Select dedicated logical Redis DB for this RSU instance.
    redisReply* select_reply = (redisReply*)redisCommand(redis_ctx, "SELECT %d", redis_db);
    if (!select_reply || select_reply->type == REDIS_REPLY_ERROR) {
        if (select_reply && select_reply->str) {
            EV_ERROR << "Redis SELECT error (db=" << redis_db << "): " << select_reply->str << std::endl;
        } else {
            EV_ERROR << "Redis SELECT error (db=" << redis_db << ")" << std::endl;
        }
        if (select_reply) freeReplyObject(select_reply);
        redisFree(redis_ctx);
        redis_ctx = nullptr;
        is_connected = false;
        return false;
    }
    freeReplyObject(select_reply);

    EV_INFO << "Connected to Redis at " << redis_host << ":" << redis_port
            << " (db=" << redis_db << ")" << std::endl;
    is_connected = true;
    return true;
}

void RedisDigitalTwin::disconnect() {
    if (redis_ctx) {
        redisFree(redis_ctx);
        redis_ctx = nullptr;
    }
    is_connected = false;
}

void RedisDigitalTwin::updateVehicleState(
    const std::string& vehicle_id,
    double pos_x, double pos_y, double speed, double heading,
    double cpu_available, double cpu_utilization,
    double mem_available, double mem_utilization,
    int queue_length, int processing_count,
    double sim_time,
    double acceleration)
{
    if (!redis_ctx || !is_connected) return;

    std::string key = "vehicle:" + vehicle_id + ":state";

    // Use HSET to store as hash
    redisReply* reply = (redisReply*)redisCommand(redis_ctx,
        "HMSET %s pos_x %f pos_y %f speed %f heading %f "
        "cpu_available %f cpu_utilization %f "
        "mem_available %f mem_utilization %f "
        "queue_length %d processing_count %d "
        "last_update %f acceleration %f",
        key.c_str(), pos_x, pos_y, speed, heading,
        cpu_available, cpu_utilization,
        mem_available, mem_utilization,
        queue_length, processing_count,
        sim_time, acceleration
    );
    
    if (reply) {
        if (reply->type == REDIS_REPLY_ERROR) {
            EV_ERROR << "Redis HMSET error: " << reply->str << std::endl;
        }
        freeReplyObject(reply);
    }
    
    // Set TTL (auto-expire after vehicle_ttl seconds of no updates)
    reply = (redisReply*)redisCommand(redis_ctx, "EXPIRE %s %d", key.c_str(), vehicle_ttl);
    if (reply) freeReplyObject(reply);
    
    // Update service vehicle index with CPU score
    double cpu_score = cpu_available * (1.0 - cpu_utilization);
    updateServiceVehicle(vehicle_id, cpu_score);
}

std::map<std::string, std::string> RedisDigitalTwin::getVehicleState(const std::string& vehicle_id) {
    std::map<std::string, std::string> state;
    if (!redis_ctx || !is_connected) return state;
    
    std::string key = "vehicle:" + vehicle_id + ":state";
    redisReply* reply = (redisReply*)redisCommand(redis_ctx, "HGETALL %s", key.c_str());
    
    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; i += 2) {
            if (i + 1 < reply->elements) {
                std::string field = reply->element[i]->str;
                std::string value = reply->element[i+1]->str;
                state[field] = value;
            }
        }
    }
    
    if (reply) freeReplyObject(reply);
    return state;
}

void RedisDigitalTwin::updateServiceVehicle(const std::string& vehicle_id, double cpu_score) {
    if (!redis_ctx || !is_connected) return;
    
    // Add to sorted set (higher score = more available CPU)
    redisReply* reply = (redisReply*)redisCommand(redis_ctx,
        "ZADD service_vehicles:available %f %s",
        cpu_score, vehicle_id.c_str()
    );
    
    if (reply) {
        if (reply->type == REDIS_REPLY_ERROR) {
            EV_ERROR << "Redis ZADD error: " << reply->str << std::endl;
        }
        freeReplyObject(reply);
    }
}

void RedisDigitalTwin::removeServiceVehicle(const std::string& vehicle_id) {
    if (!redis_ctx || !is_connected) return;
    
    redisReply* reply = (redisReply*)redisCommand(redis_ctx,
        "ZREM service_vehicles:available %s", vehicle_id.c_str()
    );
    
    if (reply) freeReplyObject(reply);
}

std::vector<std::string> RedisDigitalTwin::getTopServiceVehicles(int count) {
    std::vector<std::string> vehicles;
    if (!redis_ctx || !is_connected) return vehicles;
    
    // Get top N by score (highest CPU available)
    redisReply* reply = (redisReply*)redisCommand(redis_ctx,
        "ZREVRANGE service_vehicles:available 0 %d", count - 1
    );
    
    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; i++) {
            vehicles.push_back(reply->element[i]->str);
        }
    }
    
    if (reply) freeReplyObject(reply);
    return vehicles;
}

void RedisDigitalTwin::createTask(const std::string& task_id, const std::string& vehicle_id,
                                  double created_time, double deadline,
                                  const std::string& task_type_name,
                                  bool is_offloadable,
                                  bool is_safety_critical,
                                  int priority_level)
{
    if (!redis_ctx || !is_connected) return;
    
    std::string key = "task:" + task_id + ":state";
    
    redisReply* reply = (redisReply*)redisCommand(redis_ctx,
        "HMSET %s vehicle_id %s status PENDING created_time %f deadline %f "
        "task_type %s is_offloadable %d is_safety_critical %d priority_level %d",
        key.c_str(), vehicle_id.c_str(), created_time, deadline,
        task_type_name.empty() ? "UNKNOWN" : task_type_name.c_str(),
        (int)is_offloadable, (int)is_safety_critical, priority_level
    );
    
    if (reply) {
        if (reply->type == REDIS_REPLY_ERROR) {
            EV_ERROR << "Redis task create error: " << reply->str << std::endl;
        }
        freeReplyObject(reply);
    }
    
    // Set TTL (expire after task_ttl)
    reply = (redisReply*)redisCommand(redis_ctx, "EXPIRE %s %d", key.c_str(), task_ttl);
    if (reply) freeReplyObject(reply);
}

void RedisDigitalTwin::updateTaskStatus(const std::string& task_id, const std::string& status,
                                       const std::string& decision_type, 
                                       const std::string& target_id)
{
    if (!redis_ctx || !is_connected) return;
    
    std::string key = "task:" + task_id + ":state";
    
    redisReply* reply;
    if (decision_type.empty()) {
        reply = (redisReply*)redisCommand(redis_ctx,
            "HSET %s status %s", key.c_str(), status.c_str()
        );
    } else {
        reply = (redisReply*)redisCommand(redis_ctx,
            "HMSET %s status %s decision_type %s target_id %s",
            key.c_str(), status.c_str(), decision_type.c_str(), target_id.c_str()
        );
    }
    
    if (reply) {
        if (reply->type == REDIS_REPLY_ERROR) {
            EV_ERROR << "Redis task update error: " << reply->str << std::endl;
        }
        freeReplyObject(reply);
    }
}

void RedisDigitalTwin::deleteTask(const std::string& task_id) {
    if (!redis_ctx || !is_connected) return;
    
    std::string key = "task:" + task_id + ":state";
    redisReply* reply = (redisReply*)redisCommand(redis_ctx, "DEL %s", key.c_str());
    if (reply) freeReplyObject(reply);
}

void RedisDigitalTwin::updateTaskCompletion(const std::string& task_id,
                                           const std::string& status,
                                           const std::string& decision_type,
                                           const std::string& processor_id,
                                           double processing_time,
                                           double total_latency,
                                           double completion_time)
{
    if (!redis_ctx || !is_connected) return;

    std::string key = "task:" + task_id + ":state";

    redisReply* reply = (redisReply*)redisCommand(redis_ctx,
        "HMSET %s status %s decision_type %s processor_id %s "
        "processing_time %f total_latency %f completion_time %f",
        key.c_str(), status.c_str(), decision_type.c_str(), processor_id.c_str(),
        processing_time, total_latency, completion_time
    );

    if (reply) {
        if (reply->type == REDIS_REPLY_ERROR) {
            EV_ERROR << "Redis task completion update error: " << reply->str << std::endl;
        }
        freeReplyObject(reply);
    }
}

std::map<std::string, std::string> RedisDigitalTwin::getTaskState(const std::string& task_id) {
    std::map<std::string, std::string> state;
    if (!redis_ctx || !is_connected) return state;
    
    std::string key = "task:" + task_id + ":state";
    redisReply* reply = (redisReply*)redisCommand(redis_ctx, "HGETALL %s", key.c_str());
    
    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; i += 2) {
            if (i + 1 < reply->elements) {
                state[reply->element[i]->str] = reply->element[i+1]->str;
            }
        }
    }
    
    if (reply) freeReplyObject(reply);
    return state;
}

void RedisDigitalTwin::pushOffloadingRequest(const std::string& task_id,
                                            const std::string& vehicle_id,
                                            const std::string& rsu_id,
                                            double task_size_bytes,
                                            double cpu_cycles,
                                            double deadline_seconds,
                                            double qos_value,
                                            double request_time,
                                            const std::string& task_type_name,
                                            uint64_t input_size_bytes,
                                            uint64_t output_size_bytes,
                                            bool is_offloadable,
                                            bool is_safety_critical,
                                            int priority_level)
{
    if (!redis_ctx || !is_connected) return;
    
    // Store request data in hash
    std::string request_key = "task:" + task_id + ":request";
    redisReply* reply = (redisReply*)redisCommand(redis_ctx,
        "HMSET %s "
        "task_id %s "
        "vehicle_id %s "
        "rsu_id %s "
        "mem_footprint_bytes %.0f "
        "cpu_cycles %.0f "
        "deadline_seconds %.2f "
        "qos_value %.2f "
        "request_time %.6f "
        "task_type %s "
        "input_size_bytes %llu "
        "output_size_bytes %llu "
        "is_offloadable %d "
        "is_safety_critical %d "
        "priority_level %d",
        request_key.c_str(),
        task_id.c_str(),
        vehicle_id.c_str(),
        rsu_id.c_str(),
        task_size_bytes,
        cpu_cycles,
        deadline_seconds,
        qos_value,
        request_time,
        task_type_name.empty() ? "UNKNOWN" : task_type_name.c_str(),
        (unsigned long long)input_size_bytes,
        (unsigned long long)output_size_bytes,
        (int)is_offloadable,
        (int)is_safety_critical,
        priority_level
    );
    
    if (reply) {
        if (reply->type == REDIS_REPLY_ERROR) {
            EV_ERROR << "Redis request hash error: " << reply->str << std::endl;
        }
        freeReplyObject(reply);
    }
    
    // Set TTL on request data
    reply = (redisReply*)redisCommand(redis_ctx, "EXPIRE %s %d", request_key.c_str(), task_ttl);
    if (reply) freeReplyObject(reply);
    
    // Push task_id to offloading request queue (for ML model to pop)
    reply = (redisReply*)redisCommand(redis_ctx,
        "RPUSH offloading_requests:queue %s", task_id.c_str()
    );
    
    if (reply) {
        if (reply->type == REDIS_REPLY_ERROR) {
            EV_ERROR << "Redis queue push error: " << reply->str << std::endl;
        } else {
            EV_INFO << "✓ Pushed offloading request to Redis queue: " << task_id << std::endl;
        }
        freeReplyObject(reply);
    }
}

std::map<std::string, std::string> RedisDigitalTwin::getDecision(const std::string& task_id) {
    std::map<std::string, std::string> decision;
    if (!redis_ctx || !is_connected) return decision;

    std::string decision_key = "task:" + task_id + ":decision";
    redisReply* reply = (redisReply*)redisCommand(redis_ctx, "HGETALL %s", decision_key.c_str());

    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; i += 2) {
            if (i + 1 < reply->elements) {
                decision[reply->element[i]->str] = reply->element[i+1]->str;
            }
        }
    }

    if (reply) freeReplyObject(reply);
    return decision;
}

std::map<std::string, std::string> RedisDigitalTwin::getMultiAgentDecisions(const std::string& task_id) {
    std::map<std::string, std::string> decisions;
    if (!redis_ctx || !is_connected) return decisions;

    std::string key = "task:" + task_id + ":decisions";
    redisReply* reply = (redisReply*)redisCommand(redis_ctx, "HGETALL %s", key.c_str());

    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; i += 2) {
            if (i + 1 < reply->elements) {
                decisions[reply->element[i]->str] = reply->element[i+1]->str;
            }
        }
    }

    if (reply) freeReplyObject(reply);
    return decisions;
}

void RedisDigitalTwin::writeTaskResults(const std::string& task_id,
                                        const std::string& agent_name,
                                        const std::string& status,
                                        double total_latency,
                                        double energy_joules,
                                        const std::string& fail_reason)
{
    if (!redis_ctx || !is_connected) return;

    // Ensure fail_reason is never blank for failures — default to UNKNOWN
    const std::string& effective_reason = (!fail_reason.empty()) ? fail_reason
        : (status != "COMPLETED_ON_TIME" ? "UNKNOWN" : "NONE");

    std::string key = "task:" + task_id + ":results";

    // Write four per-agent fields atomically: status, latency, energy, fail_reason
    std::string status_field  = agent_name + "_status";
    std::string latency_field = agent_name + "_latency";
    std::string energy_field  = agent_name + "_energy";
    std::string reason_field  = agent_name + "_reason";

    redisReply* reply = (redisReply*)redisCommand(redis_ctx,
        "HSET %s %s %s %s %f %s %f %s %s",
        key.c_str(),
        status_field.c_str(),  status.c_str(),
        latency_field.c_str(), total_latency,
        energy_field.c_str(),  energy_joules,
        reason_field.c_str(),  effective_reason.c_str()
    );

    if (reply) {
        if (reply->type == REDIS_REPLY_ERROR) {
            EV_ERROR << "Redis writeTaskResults error: " << reply->str << std::endl;
        }
        freeReplyObject(reply);
    }

    // TTL: 5 minutes — long enough for DRL to poll, short enough to avoid stale data
    reply = (redisReply*)redisCommand(redis_ctx, "EXPIRE %s 300", key.c_str());
    if (reply) freeReplyObject(reply);

    EV_INFO << "✓ writeTaskResults: task=" << task_id << " agent=" << agent_name
            << " status=" << status << " reason=" << effective_reason
            << " latency=" << total_latency << "s energy=" << energy_joules << "J" << std::endl;
}

// ── Single-agent API implementations ─────────────────────────────────────────

std::map<std::string, std::string> RedisDigitalTwin::getSingleDecision(const std::string& task_id)
{
    std::map<std::string, std::string> decision;
    if (!redis_ctx || !is_connected) return decision;

    std::string key = "task:" + task_id + ":decision";
    redisReply* reply = (redisReply*)redisCommand(redis_ctx, "HGETALL %s", key.c_str());

    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i + 1 < reply->elements; i += 2) {
            if (reply->element[i]->str && reply->element[i+1]->str) {
                decision[reply->element[i]->str] = reply->element[i+1]->str;
            }
        }
    }
    if (reply) freeReplyObject(reply);
    return decision;
}

void RedisDigitalTwin::writeSingleResult(const std::string& task_id,
                                         const std::string& status,
                                         double total_latency,
                                         double energy_joules,
                                         const std::string& fail_reason)
{
    if (!redis_ctx || !is_connected) return;

    const std::string effective_reason = (!fail_reason.empty()) ? fail_reason
        : (status != "COMPLETED_ON_TIME" ? "UNKNOWN" : "NONE");

    std::string key = "task:" + task_id + ":result";

    redisReply* reply = (redisReply*)redisCommand(redis_ctx,
        "HSET %s status %s latency %f energy %f reason %s",
        key.c_str(),
        status.c_str(),
        total_latency,
        energy_joules,
        effective_reason.c_str()
    );
    if (reply) {
        if (reply->type == REDIS_REPLY_ERROR)
            EV_ERROR << "Redis writeSingleResult error: " << reply->str << std::endl;
        freeReplyObject(reply);
    }

    reply = (redisReply*)redisCommand(redis_ctx, "EXPIRE %s 300", key.c_str());
    if (reply) freeReplyObject(reply);
}

void RedisDigitalTwin::writeLocalResult(const std::string& task_id,
                                         const std::string& task_type_name,
                                         double qos_value,
                                         double deadline_s,
                                         const std::string& status,
                                         double total_latency,
                                         double energy_joules,
                                         const std::string& fail_reason)
{
    if (!redis_ctx || !is_connected) return;

    const std::string effective_reason = (!fail_reason.empty()) ? fail_reason
        : (status != "COMPLETED_ON_TIME" ? "UNKNOWN" : "NONE");

    std::string key = "task:" + task_id + ":local_result";

    // Embed task metadata so Python can read everything from one hash (no request hash needed)
    redisReply* reply = (redisReply*)redisCommand(redis_ctx,
        "HSET %s task_type %s qos_value %f deadline_s %f "
        "status %s latency %f energy %f reason %s",
        key.c_str(),
        task_type_name.empty() ? "UNKNOWN" : task_type_name.c_str(),
        qos_value,
        deadline_s,
        status.c_str(),
        total_latency,
        energy_joules,
        effective_reason.c_str()
    );
    if (reply) {
        if (reply->type == REDIS_REPLY_ERROR)
            EV_ERROR << "Redis writeLocalResult error: " << reply->str << std::endl;
        freeReplyObject(reply);
    }

    reply = (redisReply*)redisCommand(redis_ctx, "EXPIRE %s 300", key.c_str());
    if (reply) freeReplyObject(reply);

    // Push task_id so Python can consume results via LPOP on local_results:queue
    reply = (redisReply*)redisCommand(redis_ctx, "RPUSH local_results:queue %s", task_id.c_str());
    if (reply) freeReplyObject(reply);
}

void RedisDigitalTwin::writeSimConfig(const std::string& offload_mode)
{
    if (!redis_ctx || !is_connected) return;

    redisReply* reply = (redisReply*)redisCommand(redis_ctx,
        "SET sim:offload_mode %s", offload_mode.c_str());
    if (reply) freeReplyObject(reply);
}

// ── End single-agent API ──────────────────────────────────────────────────────

void RedisDigitalTwin::updateRSUResources(const std::string& rsu_id,
                                         double cpu_available, double memory_available,
                                         int queue_length, int processing_count,
                                         double sim_time,
                                         double pos_x, double pos_y,
                                         double cpu_utilization)
{
    if (!redis_ctx || !is_connected) return;

    std::string key = "rsu:" + rsu_id + ":resources";

    // Write cpu_utilization directly so DRL can read it without needing cpu_total
    redisReply* reply = (redisReply*)redisCommand(redis_ctx,
        "HMSET %s cpu_available %f memory_available %f "
        "queue_length %d processing_count %d update_time %f "
        "pos_x %f pos_y %f cpu_utilization %f",
        key.c_str(), cpu_available, memory_available,
        queue_length, processing_count, sim_time,
        pos_x, pos_y, cpu_utilization
    );

    if (reply) {
        if (reply->type == REDIS_REPLY_ERROR) {
            EV_ERROR << "Redis RSU update error: " << reply->str << std::endl;
        }
        freeReplyObject(reply);
    }
}

std::map<std::string, std::string> RedisDigitalTwin::getRSUState(const std::string& rsu_id) {
    std::map<std::string, std::string> state;
    if (!redis_ctx || !is_connected) return state;
    
    std::string key = "rsu:" + rsu_id + ":resources";
    redisReply* reply = (redisReply*)redisCommand(redis_ctx, "HGETALL %s", key.c_str());
    
    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; i += 2) {
            if (i + 1 < reply->elements) {
                state[reply->element[i]->str] = reply->element[i+1]->str;
            }
        }
    }
    
    if (reply) freeReplyObject(reply);
    return state;
}

std::vector<RedisDigitalTwin::VehicleSnapshot> 
RedisDigitalTwin::getNearbyVehicles(double center_x, double center_y, double range_meters)
{
    std::vector<VehicleSnapshot> vehicles;
    if (!redis_ctx || !is_connected) return vehicles;
    
    // Get all vehicle keys
    redisReply* reply = (redisReply*)redisCommand(redis_ctx, "KEYS vehicle:*:state");
    
    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; i++) {
            std::string key = reply->element[i]->str;
            
            // Extract vehicle_id from "vehicle:XXX:state"
            size_t first_colon = key.find(':');
            size_t second_colon = key.find(':', first_colon + 1);
            if (first_colon != std::string::npos && second_colon != std::string::npos) {
                std::string vehicle_id = key.substr(first_colon + 1, second_colon - first_colon - 1);
                auto state = getVehicleState(vehicle_id);
                
                if (!state.empty() && state.count("pos_x") && state.count("pos_y")) {
                    try {
                        double pos_x = std::stod(state["pos_x"]);
                        double pos_y = std::stod(state["pos_y"]);
                        double dist = std::sqrt((pos_x - center_x)*(pos_x - center_x) + 
                                               (pos_y - center_y)*(pos_y - center_y));
                        
                        if (dist <= range_meters) {
                            VehicleSnapshot snap;
                            snap.vehicle_id = vehicle_id;
                            snap.pos_x = pos_x;
                            snap.pos_y = pos_y;
                            snap.speed = std::stod(state["speed"]);
                            snap.heading = std::stod(state["heading"]);
                            snap.cpu_available = std::stod(state["cpu_available"]);
                            snap.cpu_utilization = std::stod(state["cpu_utilization"]);
                            snap.mem_available = std::stod(state["mem_available"]);
                            snap.mem_utilization = std::stod(state["mem_utilization"]);
                            snap.queue_length = std::stoi(state["queue_length"]);
                            snap.processing_count = std::stoi(state["processing_count"]);
                            snap.last_update = std::stod(state["last_update"]);
                            vehicles.push_back(snap);
                        }
                    } catch (const std::exception& e) {
                        EV_ERROR << "Error parsing vehicle state: " << e.what() << std::endl;
                    }
                }
            }
        }
    }
    
    if (reply) freeReplyObject(reply);
    return vehicles;
}

std::vector<RedisDigitalTwin::VehicleSnapshot> 
RedisDigitalTwin::getAllVehicles()
{
    std::vector<VehicleSnapshot> vehicles;
    if (!redis_ctx || !is_connected) return vehicles;
    
    // Get all vehicle keys
    redisReply* reply = (redisReply*)redisCommand(redis_ctx, "KEYS vehicle:*:state");
    
    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; i++) {
            std::string key = reply->element[i]->str;
            
            // Extract vehicle_id from "vehicle:XXX:state"
            size_t first_colon = key.find(':');
            size_t second_colon = key.find(':', first_colon + 1);
            if (first_colon != std::string::npos && second_colon != std::string::npos) {
                std::string vehicle_id = key.substr(first_colon + 1, second_colon - first_colon - 1);
                auto state = getVehicleState(vehicle_id);
                
                if (!state.empty()) {
                    try {
                        VehicleSnapshot snap;
                        snap.vehicle_id = vehicle_id;
                        snap.pos_x = std::stod(state["pos_x"]);
                        snap.pos_y = std::stod(state["pos_y"]);
                        snap.speed = std::stod(state["speed"]);
                        snap.heading = std::stod(state["heading"]);
                        snap.cpu_available = std::stod(state["cpu_available"]);
                        snap.cpu_utilization = std::stod(state["cpu_utilization"]);
                        snap.mem_available = std::stod(state["mem_available"]);
                        snap.mem_utilization = std::stod(state["mem_utilization"]);
                        snap.queue_length = std::stoi(state["queue_length"]);
                        snap.processing_count = std::stoi(state["processing_count"]);
                        snap.last_update = std::stod(state["last_update"]);
                        vehicles.push_back(snap);
                    } catch (const std::exception& e) {
                        EV_ERROR << "Error parsing vehicle state: " << e.what() << std::endl;
                    }
                }
            }
        }
    }
    
    if (reply) freeReplyObject(reply);
    return vehicles;
}

void RedisDigitalTwin::cleanupExpiredData() {
    // Redis TTL handles this automatically
    // This method can be used for custom cleanup logic if needed
}

void RedisDigitalTwin::flushAll() {
    if (!redis_ctx || !is_connected) return;
    
    redisReply* reply = (redisReply*)redisCommand(redis_ctx, "FLUSHALL");
    if (reply) {
        EV_INFO << "Redis database flushed" << std::endl;
        freeReplyObject(reply);
    }
}

int RedisDigitalTwin::getActiveVehicleCount() {
    if (!redis_ctx || !is_connected) return 0;
    
    redisReply* reply = (redisReply*)redisCommand(redis_ctx, "KEYS vehicle:*:state");
    int count = 0;
    
    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        count = reply->elements;
    }
    
    if (reply) freeReplyObject(reply);
    return count;
}

int RedisDigitalTwin::getActiveTaskCount() {
    if (!redis_ctx || !is_connected) return 0;
    
    redisReply* reply = (redisReply*)redisCommand(redis_ctx, "KEYS task:*:state");
    int count = 0;
    
    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        count = reply->elements;
    }
    
    if (reply) freeReplyObject(reply);
    return count;
}

void RedisDigitalTwin::updateSecondaryProgress(const std::string& run_id,
                                               double sim_time,
                                               double sample_interval_s) {
    if (!redis_ctx || !is_connected) return;

    std::string key = "dt2:progress:" + run_id;
    redisReply* reply = (redisReply*)redisCommand(
        redis_ctx,
        "HMSET %s sim_time %f sample_interval_s %f updated_at %f",
        key.c_str(), sim_time, sample_interval_s, sim_time
    );

    if (reply) {
        if (reply->type == REDIS_REPLY_ERROR) {
            EV_ERROR << "Redis dt2 progress update error: " << reply->str << std::endl;
        }
        freeReplyObject(reply);
    }
}

void RedisDigitalTwin::pushSecondaryVehicleSample(const std::string& run_id,
                                                  const std::string& vehicle_id,
                                                  double sim_time,
                                                  double pos_x,
                                                  double pos_y,
                                                  double speed,
                                                  double heading,
                                                  double acceleration,
                                                  int max_series_len) {
    if (!redis_ctx || !is_connected) return;

    std::string latest_key = "dt2:vehicle:" + run_id + ":" + vehicle_id + ":latest";
    redisReply* reply = (redisReply*)redisCommand(
        redis_ctx,
        "HMSET %s sim_time %f pos_x %f pos_y %f speed %f heading %f acceleration %f",
        latest_key.c_str(), sim_time, pos_x, pos_y, speed, heading, acceleration
    );
    if (reply) {
        if (reply->type == REDIS_REPLY_ERROR) {
            EV_ERROR << "Redis dt2 vehicle latest HMSET error: " << reply->str << std::endl;
        }
        freeReplyObject(reply);
    }

    std::string stream_key = "dt2:vehicle:" + run_id + ":" + vehicle_id + ":samples";
    reply = (redisReply*)redisCommand(
        redis_ctx,
        "XADD %s MAXLEN ~ %d * sim_time %f pos_x %f pos_y %f speed %f heading %f acceleration %f",
        stream_key.c_str(), std::max(1, max_series_len),
        sim_time, pos_x, pos_y, speed, heading, acceleration
    );
    if (reply) {
        if (reply->type == REDIS_REPLY_ERROR) {
            EV_ERROR << "Redis dt2 vehicle XADD error: " << reply->str << std::endl;
        }
        freeReplyObject(reply);
    }
}

void RedisDigitalTwin::pushSecondaryV2RsuLinkSample(const std::string& run_id,
                                                    const std::string& tx_vehicle_id,
                                                    const std::string& rsu_id,
                                                    double sim_time,
                                                    double tx_x,
                                                    double tx_y,
                                                    double rx_x,
                                                    double rx_y,
                                                    double distance_m,
                                                    double relative_speed,
                                                    double tx_heading,
                                                    int max_series_len) {
    if (!redis_ctx || !is_connected) return;

    std::string stream_key = "dt2:link:v2rsu:" + run_id + ":" + tx_vehicle_id + ":" + rsu_id + ":samples";
    redisReply* reply = (redisReply*)redisCommand(
        redis_ctx,
        "XADD %s MAXLEN ~ %d * sim_time %f tx_x %f tx_y %f rx_x %f rx_y %f distance_m %f relative_speed %f tx_heading %f",
        stream_key.c_str(), std::max(1, max_series_len),
        sim_time, tx_x, tx_y, rx_x, rx_y, distance_m, relative_speed, tx_heading
    );
    if (reply) {
        if (reply->type == REDIS_REPLY_ERROR) {
            EV_ERROR << "Redis dt2 v2rsu XADD error: " << reply->str << std::endl;
        }
        freeReplyObject(reply);
    }
}

void RedisDigitalTwin::pushSecondaryV2vLinkSample(const std::string& run_id,
                                                  const std::string& tx_vehicle_id,
                                                  const std::string& rx_vehicle_id,
                                                  double sim_time,
                                                  double tx_x,
                                                  double tx_y,
                                                  double rx_x,
                                                  double rx_y,
                                                  double distance_m,
                                                  double relative_speed,
                                                  double tx_heading,
                                                  double rx_heading,
                                                  int max_series_len) {
    if (!redis_ctx || !is_connected) return;

    std::string stream_key = "dt2:link:v2v:" + run_id + ":" + tx_vehicle_id + ":" + rx_vehicle_id + ":samples";
    redisReply* reply = (redisReply*)redisCommand(
        redis_ctx,
        "XADD %s MAXLEN ~ %d * sim_time %f tx_x %f tx_y %f rx_x %f rx_y %f distance_m %f relative_speed %f tx_heading %f rx_heading %f",
        stream_key.c_str(), std::max(1, max_series_len),
        sim_time, tx_x, tx_y, rx_x, rx_y, distance_m, relative_speed, tx_heading, rx_heading
    );
    if (reply) {
        if (reply->type == REDIS_REPLY_ERROR) {
            EV_ERROR << "Redis dt2 v2v XADD error: " << reply->str << std::endl;
        }
        freeReplyObject(reply);
    }
}

// ============================================================================
// Task Lifecycle Event Stream Writer
// ============================================================================
// This writes to a Redis stream for dashboard real-time visualization
// Stream key: task_lifecycle_events
// Each entry: {task_id, event_type, event_time, source, target, details}

void RedisDigitalTwin::appendTaskLifecycleEvent(
    const std::string& task_id,
    const std::string& event_type,
    double event_time,
    const std::string& source_entity,
    const std::string& target_entity,
    const std::string& details)
{
    if (!redis_ctx || !is_connected) return;

    // Use XADD to append to stream
    // Format: XADD key * field1 value1 field2 value2 ...
    redisReply* reply = (redisReply*)redisCommand(redis_ctx,
        "XADD task_lifecycle_events * "
        "task_id %s event_type %s event_time %f source_entity %s target_entity %s details %s",
        task_id.c_str(),
        event_type.c_str(),
        event_time,
        source_entity.c_str(),
        target_entity.c_str(),
        details.c_str()
    );

    if (reply) {
        if (reply->type == REDIS_REPLY_ERROR) {
            EV_ERROR << "Redis appendTaskLifecycleEvent error: " << reply->str << std::endl;
        } else {
            EV_INFO << "✓ Lifecycle event written: " << task_id << " " << event_type << endl;
        }
        freeReplyObject(reply);
    }
}

} // namespace
