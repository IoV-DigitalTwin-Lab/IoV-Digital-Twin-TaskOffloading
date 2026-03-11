#ifndef METRICSMANAGER_H_
#define METRICSMANAGER_H_

#include <omnetpp.h>
#include <map>
#include <vector>
#include "TaskProfile.h"
#include "EnergyModel.h"

using namespace omnetpp;
using complex_network::TaskType;
using complex_network::TaskProfileDatabase;

/**
 * MetricsManager - Centralized tracking of three key metrics:
 * 1. Energy Consumption (Joules) - per task type, per execution location
 * 2. Latency (seconds) - end-to-end from generation to completion
 * 3. Task Completion Rate (%) - completed on-time vs total
 */

class MetricsManager {
public:
    // Singleton access
    static MetricsManager& getInstance();
    
    /**
     * Enum for execution location tracking
     */
    enum ExecutionLocation {
        LOCAL_EXECUTION = 0,
        RSU_OFFLOAD = 1,
        FAILED = 2
    };
    
    /**
     * Per-task-type per-execution-location metrics
     */
    struct TaskTypeMetrics {
        // Completion counts
        uint32_t generated = 0;          // Total generated
        uint32_t completed_on_time = 0;  // Completed before deadline
        uint32_t completed_late = 0;     // Completed after deadline but not failed
        uint32_t failed = 0;             // Failed (deadline missed entirely)
        
        // Energy tracking (in Joules)
        double total_energy_joules = 0.0;
        double avg_energy_joules = 0.0;
        double energy_local_sum = 0.0;
        double energy_offload_sum = 0.0;
        uint32_t local_execution_count = 0;
        uint32_t offload_execution_count = 0;
        
        // Latency tracking (in seconds)
        double total_latency_seconds = 0.0;
        double avg_latency_seconds = 0.0;
        double min_latency_seconds = 1e9;
        double max_latency_seconds = 0.0;
        double latency_local_sum = 0.0;
        double latency_offload_sum = 0.0;
        
        // Computed rates
        double completion_rate = 0.0;      // (on_time + late) / generated
        double on_time_rate = 0.0;         // on_time / generated
        double deadline_miss_rate = 0.0;   // (late + failed) / generated
        
        // Offloading statistics
        double local_execution_fraction = 0.0;   // local_count / total_count
        double offload_execution_fraction = 0.0; // offload_count / total_count
    };
    
    /**
     * System-wide aggregated metrics
     */
    struct SystemMetrics {
        double total_energy_all_tasks = 0.0;         // Sum across all tasks
        double avg_energy_per_task = 0.0;
        double total_latency_all_tasks = 0.0;        // Sum across all tasks
        double avg_latency_per_task = 0.0;
        double cpu_utilization_percent = 0.0;        // (active_time / total_time)
        double network_utilization_percent = 0.0;    // (transmission_time / total_time)
        double system_completion_rate = 0.0;         // Overall completion rate
        double system_deadline_miss_rate = 0.0;
        uint32_t total_tasks_generated = 0;
        uint32_t total_tasks_completed = 0;
    };
    
    // ========== Recording Methods ==========
    
    /**
     * Record task generation event
     */
    void recordTaskGenerated(TaskType type);
    
    /**
     * Record task completion with all metrics
     * 
     * @param type: task type (from TaskProfile)
     * @param location: where it executed (LOCAL or RSU)
     * @param execution_time_sec: time from start to completion on executor
     * @param e2e_latency_sec: time from generation to completion (end-to-end)
     * @param energy_consumed_joules: energy consumed during execution
     * @param deadline_sec: task deadline
     * @param actual_completion_time_sec: simTime() when task finished
     */
    void recordTaskCompletion(TaskType type,
                             ExecutionLocation location,
                             double execution_time_sec,
                             double e2e_latency_sec,
                             double energy_consumed_joules,
                             double deadline_sec,
                             double actual_completion_time_sec);
    
    /**
     * Record task failure/timeout
     */
    void recordTaskFailed(TaskType type, double e2e_latency_sec);
    
    /**
     * Record CPU utilization snapshot
     * @param utilized_cycles: CPU cycles used in this period
     * @param total_available_cycles: total CPU cycles available
     */
    void recordCPUUtilization(uint64_t utilized_cycles, uint64_t total_available_cycles);
    
    /**
     * Record network transmission
     * @param data_size_bytes: size of data transmitted
     * @param transmission_time_sec: time for transmission
     */
    void recordNetworkTransmission(uint32_t data_size_bytes, double transmission_time_sec);
    
    // ========== Query Methods ==========
    
    /**
     * Get metrics for a specific task type
     */
    const TaskTypeMetrics& getTaskMetrics(TaskType type) const;
    
    /**
     * Get system-wide aggregated metrics
     */
    SystemMetrics getSystemMetrics() const;
    
    /**
     * Get energy breakdown for a task type
     */
    struct EnergyBreakdown {
        double local_total_joules = 0.0;
        double offload_total_joules = 0.0;
        double local_avg_joules = 0.0;
        double offload_avg_joules = 0.0;
        double savings_per_offload = 0.0;  // local_avg - offload_avg
    };
    EnergyBreakdown getEnergyBreakdown(TaskType type) const;
    
    /**
     * Get latency percentiles for a task type
     */
    struct LatencyPercentiles {
        double p50_seconds = 0.0;
        double p95_seconds = 0.0;
        double p99_seconds = 0.0;
    };
    LatencyPercentiles getLatencyPercentiles(TaskType type) const;
    
    // ========== Logging & Export ==========
    
    /**
     * Print comprehensive metrics report to stdout
     */
    void printReport() const;
    
    /**
     * Print per-task metrics table
     */
    void printTaskMetricsTable() const;
    
    /**
     * Print system-wide summary
     */
    void printSystemSummary() const;
    
    /**
     * Export metrics to CSV file
     * @param filename: output CSV path
     */
    void exportToCSV(const std::string& filename) const;
    
    /**
     * Clear all metrics (e.g., for multi-run experiments)
     */
    void reset();
    
private:
    MetricsManager() = default;
    
    // Per-task-type metrics
    std::map<TaskType, TaskTypeMetrics> task_metrics;
    
    // Latency history for percentile calculation
    std::map<TaskType, std::vector<double>> latency_history;
    
    // System-level accumulators
    uint64_t total_cpu_cycles_used = 0;
    uint64_t total_cpu_cycles_available = 0;
    uint64_t total_network_bytes_transmitted = 0;
    double total_network_transmission_time = 0.0;
    
    // Helper: format task type name
    std::string taskTypeName(TaskType type) const;
    
    // Helper: calculate percentile from history
    double percentile(const std::vector<double>& data, double p) const;
};

#endif /* METRICSMANAGER_H_ */
