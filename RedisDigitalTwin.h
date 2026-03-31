#ifndef __REDIS_DIGITAL_TWIN_H_
#define __REDIS_DIGITAL_TWIN_H_

#include <hiredis/hiredis.h>
#include <string>
#include <map>
#include <vector>
#include <omnetpp.h>

using namespace omnetpp;

namespace complex_network {

class RedisDigitalTwin {
private:
    redisContext* redis_ctx = nullptr;
    std::string redis_host = "127.0.0.1";
    int redis_port = 6379;
    int redis_db = 0;
    int vehicle_ttl = 300;  // 5 minutes
    int task_ttl = 1800;   // 20 minutes
    bool is_connected = false;

public:
    RedisDigitalTwin(const std::string& host = "127.0.0.1", int port = 6379, int db = 0);
    ~RedisDigitalTwin();
    
    bool connect();
    void disconnect();
    bool isConnected() const { return is_connected; }
    
    // Vehicle State Management
    void updateVehicleState(const std::string& vehicle_id, 
                           double pos_x, double pos_y, double speed, double heading,
                           double cpu_available, double cpu_utilization,
                           double mem_available, double mem_utilization,
                           int queue_length, int processing_count,
                           double sim_time);
    
    std::map<std::string, std::string> getVehicleState(const std::string& vehicle_id);
    
    // Service Vehicle Index (for fast selection)
    void updateServiceVehicle(const std::string& vehicle_id, double cpu_score);
    std::vector<std::string> getTopServiceVehicles(int count);
    void removeServiceVehicle(const std::string& vehicle_id);
    
    // Task State Tracking
    void createTask(const std::string& task_id, const std::string& vehicle_id,
                   double created_time, double deadline,
                   const std::string& task_type_name = "",
                   bool is_offloadable = true,
                   bool is_safety_critical = false,
                   int priority_level = 2);
    void updateTaskStatus(const std::string& task_id, const std::string& status,
                         const std::string& decision_type = "", 
                         const std::string& target_id = "");

    // Detailed task completion statistics
    void updateTaskCompletion(const std::string& task_id,
                              const std::string& status,
                              const std::string& decision_type,
                              const std::string& processor_id,
                              double processing_time,
                              double total_latency,
                              double completion_time);
    void deleteTask(const std::string& task_id);
    std::map<std::string, std::string> getTaskState(const std::string& task_id);
    
    // Offloading Request Queue (for ML model)
    void pushOffloadingRequest(const std::string& task_id,
                              const std::string& vehicle_id,
                              const std::string& rsu_id,
                              double task_size_bytes,
                              double cpu_cycles,
                              double deadline_seconds,
                              double qos_value,
                              double request_time,
                              const std::string& task_type_name = "",
                              uint64_t input_size_bytes = 0,
                              uint64_t output_size_bytes = 0,
                              bool is_offloadable = true,
                              bool is_safety_critical = false,
                              int priority_level = 2);
    
    // Get ML decision from Redis (legacy single-agent key)
    std::map<std::string, std::string> getDecision(const std::string& task_id);

    // Get all agents' decisions from task:{task_id}:decisions (multi-agent key written by DRL)
    std::map<std::string, std::string> getMultiAgentDecisions(const std::string& task_id);

    // Write per-agent execution result to task:{task_id}:results (read by DRL for reward calculation)
    // fail_reason values: "NONE" | "DEADLINE_MISSED" | "RSU_QUEUE_FULL" |
    //                     "SV_MAC_UNKNOWN" | "NEIGHBOR_RSU_UNKNOWN" | "UNKNOWN_TARGET" | "EXECUTION_FAILED"
    void writeTaskResults(const std::string& task_id,
                          const std::string& agent_name,
                          const std::string& status,
                          double total_latency,
                          double energy_joules,
                          const std::string& fail_reason = "NONE");

    // RSU Resource State — cpu_utilization written directly so DRL does not need cpu_total
    void updateRSUResources(const std::string& rsu_id,
                           double cpu_available, double memory_available,
                           int queue_length, int processing_count,
                           double sim_time,
                           double pos_x, double pos_y,
                           double cpu_utilization = 0.0);
    
    std::map<std::string, std::string> getRSUState(const std::string& rsu_id);
    
    // Batch query for ML decision
    struct VehicleSnapshot {
        std::string vehicle_id;
        double pos_x, pos_y, speed, heading;
        double cpu_available, cpu_utilization;
        double mem_available, mem_utilization;
        int queue_length, processing_count;
        double last_update;
    };
    
    std::vector<VehicleSnapshot> getNearbyVehicles(double center_x, double center_y, 
                                                    double range_meters);
    
    std::vector<VehicleSnapshot> getAllVehicles();
    
    // Cleanup
    void cleanupExpiredData();
    void flushAll(); // For testing/reset
    
    // Statistics
    int getActiveVehicleCount();
    int getActiveTaskCount();

    // Secondary DT exports (motion + channel context, no SINR math)
    void updateSecondaryProgress(const std::string& run_id,
                                 double sim_time,
                                 double sample_interval_s);
    void pushSecondaryVehicleSample(const std::string& run_id,
                                    const std::string& vehicle_id,
                                    double sim_time,
                                    double pos_x,
                                    double pos_y,
                                    double speed,
                                    double heading,
                                    double acceleration,
                                    int max_series_len = 6000);
    void pushSecondaryV2RsuLinkSample(const std::string& run_id,
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
                                      int max_series_len = 6000);
    void pushSecondaryV2vLinkSample(const std::string& run_id,
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
                                    int max_series_len = 6000);
};

} // namespace

#endif
