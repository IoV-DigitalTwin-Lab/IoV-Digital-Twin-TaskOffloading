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
                           double sim_time,
                           double acceleration,
                           double source_timestamp);
    
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
    
    // Get ML decision from Redis
    std::map<std::string, std::string> getDecision(const std::string& task_id);
    
    // RSU Resource State
    void updateRSUResources(const std::string& rsu_id,
                           double cpu_available, double memory_available,
                           int queue_length, int processing_count,
                           double sim_time,
                           double pos_x, double pos_y);
    
    std::map<std::string, std::string> getRSUState(const std::string& rsu_id);
    
    // Batch query for ML decision
    struct VehicleSnapshot {
        std::string vehicle_id;
        double pos_x, pos_y, speed, heading;
        double acceleration;
        double cpu_available, cpu_utilization;
        double mem_available, mem_utilization;
        int queue_length, processing_count;
        double source_timestamp;
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

    // Secondary Q matrix exports (directional per-step SINR entries)
    void updateSecondaryQCycle(const std::string& run_id,
                               uint64_t cycle_index,
                               double sim_time,
                               double horizon_s,
                               double step_s,
                               double sinr_threshold_db,
                               int trajectory_count,
                               int entry_count,
                               int ttl_seconds);
    void pushSecondaryQEntry(const std::string& run_id,
                             uint64_t cycle_index,
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
                             double sinr_db,
                             int max_series_len = 200000);

    struct PredictedTrajectoryPoint {
        uint64_t cycle_id = 0;
        std::string vehicle_id;
        int step_index = 0;
        double predicted_time = 0.0;
        double pos_x = 0.0;
        double pos_y = 0.0;
        double speed = 0.0;
        double heading = 0.0;
        double acceleration = 0.0;
    };

    // External predictor -> secondary simulation exchange
    void updateSecondaryPredictionCycle(const std::string& run_id,
                                        uint64_t cycle_id,
                                        double generated_at,
                                        double horizon_s,
                                        double step_s,
                                        int trajectory_count,
                                        int ttl_seconds);
    void pushSecondaryPredictedPoint(const std::string& run_id,
                                     uint64_t cycle_id,
                                     const std::string& vehicle_id,
                                     int step_index,
                                     double predicted_time,
                                     double pos_x,
                                     double pos_y,
                                     double speed,
                                     double heading,
                                     double acceleration,
                                     int max_series_len = 200000);
    int64_t getLatestSecondaryPredictionCycle(const std::string& run_id);
    std::vector<PredictedTrajectoryPoint> getSecondaryPredictedPoints(const std::string& run_id,
                                                                      uint64_t cycle_id);
};

} // namespace

#endif
