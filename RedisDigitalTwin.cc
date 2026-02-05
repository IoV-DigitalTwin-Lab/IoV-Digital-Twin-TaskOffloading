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
                                  double created_time, double deadline)
{
    if (!redis_ctx || !is_connected) return;
    
    std::string key = "task:" + task_id + ":state";
    
    redisReply* reply = (redisReply*)redisCommand(redis_ctx,
        "HMSET %s vehicle_id %s status PENDING created_time %f deadline %f",
        key.c_str(), vehicle_id.c_str(), created_time, deadline
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

void RedisDigitalTwin::updateRSUResources(const std::string& rsu_id,
                                         double cpu_available, double memory_available,
                                         int queue_length, int processing_count,
                                         double sim_time)
{
    if (!redis_ctx || !is_connected) return;
    
    std::string key = "rsu:" + rsu_id + ":resources";
    
    redisReply* reply = (redisReply*)redisCommand(redis_ctx,
        "HMSET %s cpu_available %f memory_available %f "
        "queue_length %d processing_count %d update_time %f",
        key.c_str(), cpu_available, memory_available,
        queue_length, processing_count, sim_time
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

} // namespace
