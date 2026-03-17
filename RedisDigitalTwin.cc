#include "RedisDigitalTwin.h"
#include <cmath>
#include <sstream>
#include <algorithm>

namespace complex_network {

RedisDigitalTwin::RedisDigitalTwin(const std::string& host, int port)
    : redis_host(host), redis_port(port) {
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
    
    EV_INFO << "Connected to Redis at " << redis_host << ":" << redis_port << std::endl;
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
    double sim_time)
{
    if (!redis_ctx || !is_connected) return;
    
    std::string key = "vehicle:" + vehicle_id + ":state";
    
    // Use HSET to store as hash
    redisReply* reply = (redisReply*)redisCommand(redis_ctx,
        "HMSET %s pos_x %f pos_y %f speed %f heading %f "
        "cpu_available %f cpu_utilization %f "
        "mem_available %f mem_utilization %f "
        "queue_length %d processing_count %d "
        "last_update %f",
        key.c_str(), pos_x, pos_y, speed, heading,
        cpu_available, cpu_utilization,
        mem_available, mem_utilization,
        queue_length, processing_count,
        sim_time
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

void RedisDigitalTwin::pushTaskResult(const std::string& task_id,
                                      bool success,
                                      double total_latency,
                                      double completion_time,
                                      double request_time,
                                      double energy_joules)
{
    if (!redis_ctx || !is_connected) return;

    // ── Key schema (matches IoVRedisEnv._wait_for_multi_results): ──────────
    //   task:{id}:results  →  hash with fields:
    //     {agent}_status   = "COMPLETED_ON_TIME" | "FAILED"
    //     {agent}_latency  = float seconds
    //     {agent}_energy   = float Joules
    // ───────────────────────────────────────────────────────────────────────
    std::string results_key  = "task:" + task_id + ":results";
    std::string decisions_key = "task:" + task_id + ":decisions";

    // Read which agents wrote decisions for this task
    redisReply* agents_reply = (redisReply*)redisCommand(
        redis_ctx, "HGET %s agents", decisions_key.c_str());

    // Default to "ddqn" if no decisions key is present yet
    std::vector<std::string> agents = {"ddqn"};
    if (agents_reply && agents_reply->type == REDIS_REPLY_STRING) {
        // agents field is a comma-separated list: "ddqn,random"
        std::string agents_str(agents_reply->str);
        agents.clear();
        size_t pos = 0;
        while (pos < agents_str.size()) {
            size_t comma = agents_str.find(',', pos);
            if (comma == std::string::npos) comma = agents_str.size();
            std::string a = agents_str.substr(pos, comma - pos);
            if (!a.empty()) agents.push_back(a);
            pos = comma + 1;
        }
    }
    if (agents_reply) freeReplyObject(agents_reply);

    const char* status_str = success ? "COMPLETED_ON_TIME" : "FAILED";

    // Build one HMSET command covering all agents in a single round-trip
    // Max agents in practice ≤ 5 so building the command inline is fine.
    for (const auto& agent : agents) {
        std::string f_status  = agent + "_status";
        std::string f_latency = agent + "_latency";
        std::string f_energy  = agent + "_energy";

        redisReply* reply = (redisReply*)redisCommand(redis_ctx,
            "HMSET %s %s %s %s %f %s %f",
            results_key.c_str(),
            f_status.c_str(),  status_str,
            f_latency.c_str(), total_latency,
            f_energy.c_str(),  energy_joules
        );
        if (reply) {
            if (reply->type == REDIS_REPLY_ERROR) {
                EV_ERROR << "Redis pushTaskResult (" << agent << "): "
                         << reply->str << std::endl;
            }
            freeReplyObject(reply);
        }
    }

    EV_INFO << "✓ Redis task:results written for " << task_id
            << " agents=[";
    for (size_t i = 0; i < agents.size(); ++i) {
        EV_INFO << agents[i] << (i + 1 < agents.size() ? "," : "");
    }
    EV_INFO << "] status=" << status_str
            << " latency=" << total_latency
            << "s energy=" << energy_joules << "J" << std::endl;

    // Auto-expire so stale results don't accumulate
    redisReply* ex = (redisReply*)redisCommand(
        redis_ctx, "EXPIRE %s %d", results_key.c_str(), task_ttl);
    if (ex) freeReplyObject(ex);
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

void RedisDigitalTwin::updateRSUResources(const std::string& rsu_id,
                                         double cpu_available, double memory_available,
                                         int queue_length, int processing_count,
                                         double sim_time,
                                         double pos_x, double pos_y)
{
    if (!redis_ctx || !is_connected) return;
    
    std::string key = "rsu:" + rsu_id + ":resources";
    
    redisReply* reply = (redisReply*)redisCommand(redis_ctx,
        "HMSET %s cpu_available %f memory_available %f "
        "queue_length %d processing_count %d update_time %f pos_x %f pos_y %f",
        key.c_str(), cpu_available, memory_available,
        queue_length, processing_count, sim_time, pos_x, pos_y
    );
    
    if (reply) {
        if (reply->type == REDIS_REPLY_ERROR) {
            EV_ERROR << "Redis RSU update error: " << reply->str << std::endl;
        }
        freeReplyObject(reply);
    }
}

void RedisDigitalTwin::writeRSUStaticFields(const std::string& rsu_id,
                                            double cpu_total_ghz,
                                            double memory_total_gb,
                                            double bandwidth_mbps,
                                            double pos_x, double pos_y,
                                            int max_concurrent_tasks)
{
    if (!redis_ctx || !is_connected) return;

    // Write into the same key that updateRSUResources() and
    // IoVRedisEnv._fetch_rsu_state() both use: rsu:{id}:resources
    // These fields are static so we do NOT set a TTL (they survive restarts).
    std::string key = "rsu:" + rsu_id + ":resources";

    redisReply* reply = (redisReply*)redisCommand(redis_ctx,
        "HMSET %s "
        "cpu_total %f "
        "memory_total %f "
        "bandwidth %f "
        "pos_x %f "
        "pos_y %f "
        "max_concurrent %d",
        key.c_str(),
        cpu_total_ghz,
        memory_total_gb,
        bandwidth_mbps,
        pos_x, pos_y,
        max_concurrent_tasks
    );

    if (reply) {
        if (reply->type == REDIS_REPLY_ERROR) {
            EV_ERROR << "Redis writeRSUStaticFields error: " << reply->str << std::endl;
        } else {
            EV_INFO << "✓ RSU static fields written for " << rsu_id
                    << " cpu_total=" << cpu_total_ghz << "GHz"
                    << " mem_total=" << memory_total_gb << "GB"
                    << " bw=" << bandwidth_mbps << "Mbps"
                    << " pos=(" << pos_x << "," << pos_y << ")" << std::endl;
            std::cout << "REDIS_RSU_STATIC: " << rsu_id
                      << " cpu=" << cpu_total_ghz << "GHz"
                      << " mem=" << memory_total_gb << "GB"
                      << " bw=" << bandwidth_mbps << "Mbps" << std::endl;
        }
        freeReplyObject(reply);
    }
}

// ============================================================================
// RSU-to-RSU Task Forwarding  (Point 6)
// ============================================================================

void RedisDigitalTwin::forwardTaskToNeighborRSU(
    const std::string& target_rsu_id,
    const std::string& task_id,
    const std::string& vehicle_id,
    uint64_t cpu_cycles,
    uint64_t task_size_bytes,
    double deadline_seconds,
    double qos_value,
    double request_time,
    const std::string& source_rsu_id)
{
    if (!redis_ctx || !is_connected) return;

    // 1. Write task metadata hash
    std::string data_key = "rsu:" + target_rsu_id + ":forwarded_task:" + task_id;
    redisReply* r1 = (redisReply*)redisCommand(redis_ctx,
        "HMSET %s "
        "task_id %s "
        "vehicle_id %s "
        "cpu_cycles %llu "
        "task_size_bytes %llu "
        "deadline_seconds %f "
        "qos_value %f "
        "request_time %f "
        "source_rsu_id %s",
        data_key.c_str(),
        task_id.c_str(),
        vehicle_id.c_str(),
        (unsigned long long)cpu_cycles,
        (unsigned long long)task_size_bytes,
        deadline_seconds,
        qos_value,
        request_time,
        source_rsu_id.c_str()
    );
    if (r1) {
        if (r1->type == REDIS_REPLY_ERROR)
            EV_ERROR << "forwardTask HMSET error: " << r1->str << std::endl;
        freeReplyObject(r1);
    }
    // Expire after 2x the typical deadline to avoid accumulation
    redisReply* ex = (redisReply*)redisCommand(
        redis_ctx, "EXPIRE %s %d", data_key.c_str(), 60);
    if (ex) freeReplyObject(ex);

    // 2. Push task_id onto the target RSU's forwarded queue
    std::string queue_key = "rsu:" + target_rsu_id + ":forwarded_queue";
    redisReply* r2 = (redisReply*)redisCommand(
        redis_ctx, "LPUSH %s %s", queue_key.c_str(), task_id.c_str());
    if (r2) {
        if (r2->type == REDIS_REPLY_ERROR)
            EV_ERROR << "forwardTask LPUSH error: " << r2->str << std::endl;
        else
            EV_INFO << "✓ Task " << task_id << " forwarded to " << target_rsu_id << std::endl;
        freeReplyObject(r2);
    }
    std::cout << "RSU_FORWARD: task=" << task_id
              << " src=" << source_rsu_id
              << " dst=" << target_rsu_id << std::endl;
}

std::string RedisDigitalTwin::pollForwardedTask(const std::string& my_rsu_id)
{
    if (!redis_ctx || !is_connected) return "";
    std::string queue_key = "rsu:" + my_rsu_id + ":forwarded_queue";
    redisReply* r = (redisReply*)redisCommand(
        redis_ctx, "RPOP %s", queue_key.c_str());
    std::string task_id;
    if (r) {
        if (r->type == REDIS_REPLY_STRING)
            task_id = r->str;
        freeReplyObject(r);
    }
    return task_id;
}

std::map<std::string, std::string>
RedisDigitalTwin::fetchForwardedTaskData(
    const std::string& my_rsu_id, const std::string& task_id)
{
    std::map<std::string, std::string> result;
    if (!redis_ctx || !is_connected) return result;
    std::string key = "rsu:" + my_rsu_id + ":forwarded_task:" + task_id;
    redisReply* r = (redisReply*)redisCommand(redis_ctx, "HGETALL %s", key.c_str());
    if (r && r->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i + 1 < r->elements; i += 2)
            result[r->element[i]->str] = r->element[i + 1]->str;
    }
    if (r) freeReplyObject(r);
    // Delete after reading (one-shot delivery)
    if (!result.empty()) {
        redisReply* del = (redisReply*)redisCommand(
            redis_ctx, "DEL %s", key.c_str());
        if (del) freeReplyObject(del);
    }
    return result;
}

std::string RedisDigitalTwin::getBestNeighborRSU(
    const std::vector<std::string>& all_rsu_ids,
    const std::string& my_rsu_id)
{
    if (!redis_ctx || !is_connected) return "";

    std::string best_id;
    double best_cpu = -1.0;

    for (const auto& rsu_id : all_rsu_ids) {
        if (rsu_id == my_rsu_id) continue;
        std::string key = "rsu:" + rsu_id + ":resources";

        // Fetch cpu_available, queue_length, max_concurrent
        auto getField = [&](const char* field) -> double {
            redisReply* r = (redisReply*)redisCommand(
                redis_ctx, "HGET %s %s", key.c_str(), field);
            double val = 0.0;
            if (r && r->type == REDIS_REPLY_STRING)
                try { val = std::stod(r->str); } catch (...) {}
            if (r) freeReplyObject(r);
            return val;
        };

        double cpu_avail    = getField("cpu_available");
        double queue_len    = getField("queue_length");
        double max_conc     = getField("max_concurrent");
        if (max_conc <= 0) max_conc = 10;  // safe default

        // Must have CPU headroom and queue space
        if (cpu_avail > 0.0 && queue_len < max_conc) {
            if (cpu_avail > best_cpu) {
                best_cpu = cpu_avail;
                best_id  = rsu_id;
            }
        }
    }
    return best_id;
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

} // namespace
