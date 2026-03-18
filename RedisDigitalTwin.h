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
    int vehicle_ttl = 300;  // 5 minutes
    int task_ttl = 1800;   // 20 minutes
    bool is_connected = false;

public:
    RedisDigitalTwin(const std::string& host = "127.0.0.1", int port = 6379);
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

    /**
     * Write the task:id:result key that the DRL agent (IoVRedisEnv._wait_for_result)
     * polls to unblock after a task completes.
     *
     * @param task_id         task identifier
     * @param success         true = completed on-time, false = failed / late
     * @param total_latency   end-to-end latency in seconds (completion_time - request_time)
     * @param completion_time absolute simulation time of task completion
     * @param request_time    simulation time when the vehicle first sent the request
     * @param energy_joules   actual energy consumed (Joules), from EnergyCalculator
     */
    void pushTaskResult(const std::string& task_id,
                        bool success,
                        double total_latency,
                        double completion_time,
                        double request_time,
                        double energy_joules);

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

    /**
     * Write static RSU capacity fields into rsu:{id}:resources once at simulation startup.
     * These fields are permanent for the lifetime of the RSU:
     *   cpu_total        — total CPU capacity (GHz)
     *   memory_total     — total memory capacity (GB)
     *   bandwidth        — 802.11p channel bandwidth (Mbps)
     *   pos_x, pos_y     — fixed geographic position
     *   max_concurrent   — max simultaneous tasks
     *
     * The Python IoVRedisEnv._fetch_rsu_state() reads cpu_total and pos_x/pos_y
     * from the same rsu:{id}:resources key. Without this call, cpu_total is
     * missing and the utilization formula (1 - cpu_avail / cpu_total) always
     * returns 0, making state features degenerate.
     */
    void writeRSUStaticFields(const std::string& rsu_id,
                              double cpu_total_ghz,
                              double memory_total_gb,
                              double bandwidth_mbps,
                              double pos_x, double pos_y,
                              int max_concurrent_tasks);

    // ── RSU-to-RSU Task Forwarding (Point 6) ────────────────────────────────
    //
    // Uses Redis as the transport layer — no new .msg types needed.
    // Forwarded tasks are stored in  rsu:{target}:forwarded_task:{task_id}  (hash)
    // and the task_id is LPUSHed to   rsu:{target}:forwarded_queue          (list).
    // The receiving RSU polls its queue in its periodic rsu_status_update_timer.

    void forwardTaskToNeighborRSU(const std::string& target_rsu_id,
                                  const std::string& task_id,
                                  const std::string& vehicle_id,
                                  int64_t vehicle_mac,
                                  uint64_t cpu_cycles,
                                  uint64_t task_size_bytes,
                                  double deadline_seconds,
                                  double qos_value,
                                  double request_time,
                                  const std::string& source_rsu_id);

    /** RPOP one task_id from rsu:{my_rsu_id}:forwarded_queue. Returns "" if empty. */
    std::string pollForwardedTask(const std::string& my_rsu_id);

    /** Fetch+delete the forwarded task metadata hash; returns field->value map. */
    std::map<std::string, std::string> fetchForwardedTaskData(
        const std::string& my_rsu_id, const std::string& task_id);

    /**
     * Scan all_rsu_ids (excluding my_rsu_id) and return the one with the
     * highest cpu_available and queue_length < max_concurrent.  Returns ""
     * if no suitable neighbor RSU is found in Redis.
     */
    std::string getBestNeighborRSU(const std::vector<std::string>& all_rsu_ids,
                                   const std::string& my_rsu_id);
    // ────────────────────────────────────────────────────────────────────────

    // ── Service Migration (Point 7) ──────────────────────────────────────────
    /**
     * Store a completed task result for a vehicle that may have left RSU coverage.
     * Key: vehicle:{vehicle_id}:pending_result:{task_id}  (hash, TTL 120s)
     *
     * Any RSU where the vehicle reappears can read this key and deliver the
     * result via a TaskResultMessage.  This is the simplest correct form of
     * service migration: the result is buffered in Redis rather than dropped.
     *
     * @param ttl_seconds  How long to keep the result (default: 120s)
     */
    void storePendingResult(const std::string& vehicle_id,
                            const std::string& task_id,
                            bool success,
                            double processing_time,
                            double completion_time,
                            int ttl_seconds = 120);

    /**
     * Retrieve and delete a pending result for a vehicle (called when a
     * vehicle attaches to this RSU and the RSU scans for buffered results).
     * Returns empty map if no pending result exists.
     */
    std::map<std::string, std::string> fetchAndDeletePendingResult(
        const std::string& vehicle_id, const std::string& task_id);

    /**
     * Scan Redis for all pending result keys for a given vehicle:
     * KEYS vehicle:{vehicle_id}:pending_result:*
     * Returns list of task_ids with pending results.
     */
    std::vector<std::string> listPendingResults(const std::string& vehicle_id);

    /**
     * Proactive service migration: RSU-A found the vehicle is now with RSU-B,
     * so it pushes the completed result directly into RSU-B's migrated_queue.
     * RSU-B picks it up in its periodic timer and delivers immediately.
     * Key: rsu:{target_rsu}:migrated:{task_id}  Queue: rsu:{target_rsu}:migrated_queue
     */
    void storeMigratedResult(const std::string& target_rsu_id,
                             const std::string& vehicle_id,
                             const std::string& task_id,
                             bool success,
                             double processing_time,
                             double completion_time,
                             int ttl_seconds = 60);

    /**
     * RPOP one entry from rsu:{my_rsu}:migrated_queue, fetch + delete the hash.
     * Returns a map with at least: task_id, vehicle_id, success, processing_time.
     * Returns empty map if queue is empty.
     */
    std::map<std::string, std::string> pollAndFetchMigratedResult(
        const std::string& my_rsu_id);
    // ─────────────────────────────────────────────────────────────────

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
};

} // namespace

#endif
