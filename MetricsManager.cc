#include "MetricsManager.h"
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <cmath>

MetricsManager& MetricsManager::getInstance() {
    static MetricsManager instance;
    return instance;
}

void MetricsManager::recordTaskGenerated(TaskType type) {
    task_metrics[type].generated++;
}

void MetricsManager::recordTaskCompletion(TaskType type,
                                          ExecutionLocation location,
                                          double execution_time_sec,
                                          double e2e_latency_sec,
                                          double energy_consumed_joules,
                                          double deadline_sec,
                                          double actual_completion_time_sec) {
    auto& metrics = task_metrics[type];
    
    // Determine if on-time or late
    bool on_time = (actual_completion_time_sec <= deadline_sec);
    if (on_time) {
        metrics.completed_on_time++;
    } else {
        metrics.completed_late++;
    }
    
    // Energy tracking
    metrics.total_energy_joules += energy_consumed_joules;
    if (location == LOCAL_EXECUTION) {
        metrics.energy_local_sum += energy_consumed_joules;
        metrics.local_execution_count++;
    } else if (location == RSU_OFFLOAD) {
        metrics.energy_offload_sum += energy_consumed_joules;
        metrics.offload_execution_count++;
    }
    
    // Latency tracking
    metrics.total_latency_seconds += e2e_latency_sec;
    metrics.min_latency_seconds = std::min(metrics.min_latency_seconds, e2e_latency_sec);
    metrics.max_latency_seconds = std::max(metrics.max_latency_seconds, e2e_latency_sec);
    latency_history[type].push_back(e2e_latency_sec);
    
    if (location == LOCAL_EXECUTION) {
        metrics.latency_local_sum += e2e_latency_sec;
    } else if (location == RSU_OFFLOAD) {
        metrics.latency_offload_sum += e2e_latency_sec;
    }
    
    // Recompute aggregates
    uint32_t completed = metrics.completed_on_time + metrics.completed_late;
    if (metrics.generated > 0) {
        metrics.completion_rate = (double)completed / metrics.generated;
        metrics.on_time_rate = (double)metrics.completed_on_time / metrics.generated;
        metrics.deadline_miss_rate = (double)(metrics.completed_late + metrics.failed) / metrics.generated;
        metrics.avg_latency_seconds = metrics.total_latency_seconds / completed;
        metrics.avg_energy_joules = metrics.total_energy_joules / completed;
    }
    
    if (completed > 0) {
        metrics.local_execution_fraction = (double)metrics.local_execution_count / completed;
        metrics.offload_execution_fraction = (double)metrics.offload_execution_count / completed;
    }
}

void MetricsManager::recordTaskFailed(TaskType type, double e2e_latency_sec) {
    auto& metrics = task_metrics[type];
    metrics.failed++;
    metrics.total_latency_seconds += e2e_latency_sec;
    metrics.max_latency_seconds = std::max(metrics.max_latency_seconds, e2e_latency_sec);
    latency_history[type].push_back(e2e_latency_sec);
    
    if (metrics.generated > 0) {
        metrics.completion_rate = (double)(metrics.completed_on_time + metrics.completed_late) / metrics.generated;
        metrics.on_time_rate = (double)metrics.completed_on_time / metrics.generated;
        metrics.deadline_miss_rate = (double)(metrics.completed_late + metrics.failed) / metrics.generated;
    }
}

void MetricsManager::recordCPUUtilization(uint64_t utilized_cycles, uint64_t total_available_cycles) {
    total_cpu_cycles_used += utilized_cycles;
    total_cpu_cycles_available += total_available_cycles;
}

void MetricsManager::recordNetworkTransmission(uint32_t data_size_bytes, double transmission_time_sec) {
    total_network_bytes_transmitted += data_size_bytes;
    total_network_transmission_time += transmission_time_sec;
}

const MetricsManager::TaskTypeMetrics& MetricsManager::getTaskMetrics(TaskType type) const {
    static TaskTypeMetrics empty;
    auto it = task_metrics.find(type);
    if (it != task_metrics.end()) {
        return it->second;
    }
    return empty;
}

MetricsManager::SystemMetrics MetricsManager::getSystemMetrics() const {
    SystemMetrics sys;
    
    for (const auto& [type, metrics] : task_metrics) {
        sys.total_energy_all_tasks += metrics.total_energy_joules;
        sys.total_latency_all_tasks += metrics.total_latency_seconds;
        sys.total_tasks_generated += metrics.generated;
        sys.total_tasks_completed += (metrics.completed_on_time + metrics.completed_late);
    }
    
    if (sys.total_tasks_completed > 0) {
        sys.avg_energy_per_task = sys.total_energy_all_tasks / sys.total_tasks_completed;
        sys.avg_latency_per_task = sys.total_latency_all_tasks / sys.total_tasks_completed;
    }
    
    if (total_cpu_cycles_available > 0) {
        sys.cpu_utilization_percent = 100.0 * total_cpu_cycles_used / total_cpu_cycles_available;
    }
    
    if (total_network_transmission_time > 0) {
        // Assume 802.11p channel is shared resource with 100% availability
        sys.network_utilization_percent = std::min(100.0, 
            100.0 * total_network_transmission_time / (simTime().dbl() * 1.0)); // Normalized by sim time
    }
    
    if (sys.total_tasks_generated > 0) {
        sys.system_completion_rate = (double)sys.total_tasks_completed / sys.total_tasks_generated;
        uint32_t failed = 0;
        for (const auto& [type, metrics] : task_metrics) {
            failed += metrics.failed;
        }
        sys.system_deadline_miss_rate = (double)failed / sys.total_tasks_generated;
    }
    
    return sys;
}

MetricsManager::EnergyBreakdown MetricsManager::getEnergyBreakdown(TaskType type) const {
    EnergyBreakdown breakdown;
    auto it = task_metrics.find(type);
    if (it != task_metrics.end()) {
        const auto& metrics = it->second;
        breakdown.local_total_joules = metrics.energy_local_sum;
        breakdown.offload_total_joules = metrics.energy_offload_sum;
        
        if (metrics.local_execution_count > 0) {
            breakdown.local_avg_joules = metrics.energy_local_sum / metrics.local_execution_count;
        }
        if (metrics.offload_execution_count > 0) {
            breakdown.offload_avg_joules = metrics.energy_offload_sum / metrics.offload_execution_count;
        }
        
        breakdown.savings_per_offload = breakdown.local_avg_joules - breakdown.offload_avg_joules;
    }
    return breakdown;
}

MetricsManager::LatencyPercentiles MetricsManager::getLatencyPercentiles(TaskType type) const {
    LatencyPercentiles percentiles;
    auto it = latency_history.find(type);
    if (it != latency_history.end() && !it->second.empty()) {
        percentiles.p50_seconds = percentile(it->second, 0.50);
        percentiles.p95_seconds = percentile(it->second, 0.95);
        percentiles.p99_seconds = percentile(it->second, 0.99);
    }
    return percentiles;
}

double MetricsManager::percentile(const std::vector<double>& data, double p) const {
    if (data.empty()) return 0.0;
    
    std::vector<double> sorted_data = data;
    std::sort(sorted_data.begin(), sorted_data.end());
    
    size_t index = (size_t)std::ceil(p * sorted_data.size()) - 1;
    index = std::min(index, sorted_data.size() - 1);
    
    return sorted_data[index];
}

std::string MetricsManager::taskTypeName(TaskType type) const {
    switch (type) {
        case TaskType::LOCAL_OBJECT_DETECTION:
            return "LOCAL_OBJECT_DETECTION";
        case TaskType::COOPERATIVE_PERCEPTION:
            return "COOPERATIVE_PERCEPTION";
        case TaskType::ROUTE_OPTIMIZATION:
            return "ROUTE_OPTIMIZATION";
        case TaskType::FLEET_TRAFFIC_FORECAST:
            return "FLEET_TRAFFIC_FORECAST";
        case TaskType::VOICE_COMMAND_PROCESSING:
            return "VOICE_COMMAND_PROCESSING";
        case TaskType::SENSOR_HEALTH_CHECK:
            return "SENSOR_HEALTH_CHECK";
        default:
            return "UNKNOWN";
    }
}

void MetricsManager::printReport() const {
    EV << "\n========== COMPREHENSIVE METRICS REPORT ==========" << std::endl;
    printTaskMetricsTable();
    std::cout << std::endl;
    printSystemSummary();
}

void MetricsManager::printTaskMetricsTable() const {
    std::cout << "\n--- PER-TASK METRICS ---" << std::endl;
    std::cout << std::setw(30) << "Task Type"
              << std::setw(12) << "Generated"
              << std::setw(12) << "On-Time"
              << std::setw(12) << "Late"
              << std::setw(12) << "Failed"
              << std::setw(12) << "Completion%"
              << std::endl;
    std::cout << std::string(90, '-') << std::endl;
    
    for (const auto& [type, metrics] : task_metrics) {
        std::cout << std::setw(30) << taskTypeName(type)
                  << std::setw(12) << metrics.generated
                  << std::setw(12) << metrics.completed_on_time
                  << std::setw(12) << metrics.completed_late
                  << std::setw(12) << metrics.failed
                  << std::setw(11) << std::fixed << std::setprecision(1)
                  << (metrics.completion_rate * 100) << "%"
                  << std::endl;
    }
    
    std::cout << "\n--- ENERGY METRICS ---" << std::endl;
    std::cout << std::setw(30) << "Task Type"
              << std::setw(18) << "Avg Energy (J)"
              << std::setw(18) << "Local (J)"
              << std::setw(18) << "Offload (J)"
              << std::setw(15) << "Offload Frac"
              << std::endl;
    std::cout << std::string(100, '-') << std::endl;
    
    for (const auto& [type, metrics] : task_metrics) {
        auto breakdown = getEnergyBreakdown(type);
        std::cout << std::setw(30) << taskTypeName(type)
                  << std::setw(17) << std::scientific << std::setprecision(2)
                  << metrics.avg_energy_joules
                  << std::setw(17) << breakdown.local_avg_joules
                  << std::setw(17) << breakdown.offload_avg_joules
                  << std::setw(14) << std::fixed << std::setprecision(2)
                  << (metrics.offload_execution_fraction * 100) << "%"
                  << std::endl;
    }
    
    std::cout << "\n--- LATENCY METRICS ---" << std::endl;
    std::cout << std::setw(30) << "Task Type"
              << std::setw(15) << "Avg (ms)"
              << std::setw(15) << "Min (ms)"
              << std::setw(15) << "Max (ms)"
              << std::setw(15) << "P50 (ms)"
              << std::setw(15) << "P95 (ms)"
              << std::setw(15) << "P99 (ms)"
              << std::endl;
    std::cout << std::string(120, '-') << std::endl;
    
    for (const auto& [type, metrics] : task_metrics) {
        auto percentiles = getLatencyPercentiles(type);
        std::cout << std::setw(30) << taskTypeName(type)
                  << std::setw(14) << std::fixed << std::setprecision(2)
                  << (metrics.avg_latency_seconds * 1000)
                  << std::setw(14) << (metrics.min_latency_seconds * 1000)
                  << std::setw(14) << (metrics.max_latency_seconds * 1000)
                  << std::setw(14) << (percentiles.p50_seconds * 1000)
                  << std::setw(14) << (percentiles.p95_seconds * 1000)
                  << std::setw(14) << (percentiles.p99_seconds * 1000)
                  << std::endl;
    }
}

void MetricsManager::printSystemSummary() const {
    auto sys = getSystemMetrics();
    
    std::cout << "\n========== SYSTEM-WIDE SUMMARY ==========" << std::endl;
    std::cout << "Total Tasks Generated:      " << sys.total_tasks_generated << std::endl;
    std::cout << "Total Tasks Completed:      " << sys.total_tasks_completed << std::endl;
    std::cout << "System Completion Rate:     " << std::fixed << std::setprecision(2)
              << (sys.system_completion_rate * 100) << "%" << std::endl;
    std::cout << "System Deadline Miss Rate:  " << std::fixed << std::setprecision(2)
              << (sys.system_deadline_miss_rate * 100) << "%" << std::endl;
    
    std::cout << "\nEnergy Statistics:" << std::endl;
    std::cout << "  Total Energy (all tasks):  " << std::scientific << std::setprecision(3)
              << sys.total_energy_all_tasks << " J" << std::endl;
    std::cout << "  Avg Energy per Task:       " << std::fixed << std::setprecision(3)
              << sys.avg_energy_per_task << " J" << std::endl;
    
    std::cout << "\nLatency Statistics:" << std::endl;
    std::cout << "  Total Latency (all tasks): " << std::fixed << std::setprecision(2)
              << sys.total_latency_all_tasks << " s" << std::endl;
    std::cout << "  Avg Latency per Task:      " << std::fixed << std::setprecision(4)
              << sys.avg_latency_per_task << " s" << std::endl;
    
    std::cout << "\nResource Utilization:" << std::endl;
    std::cout << "  CPU Utilization:           " << std::fixed << std::setprecision(2)
              << sys.cpu_utilization_percent << "%" << std::endl;
    std::cout << "  Network Utilization:       " << std::fixed << std::setprecision(2)
              << sys.network_utilization_percent << "%" << std::endl;
}

void MetricsManager::exportToCSV(const std::string& filename) const {
    std::ofstream csv(filename);
    
    csv << "Task Type,Generated,On-Time,Late,Failed,Completion Rate (%),";
    csv << "Avg Energy (J),Local Energy (J),Offload Energy (J),Offload Fraction (%),";
    csv << "Avg Latency (ms),Min Latency (ms),Max Latency (ms),";
    csv << "P50 Latency (ms),P95 Latency (ms),P99 Latency (ms)" << std::endl;
    
    for (const auto& [type, metrics] : task_metrics) {
        auto breakdown = getEnergyBreakdown(type);
        auto percentiles = getLatencyPercentiles(type);
        
        csv << taskTypeName(type) << ","
            << metrics.generated << ","
            << metrics.completed_on_time << ","
            << metrics.completed_late << ","
            << metrics.failed << ","
            << std::fixed << std::setprecision(2) << (metrics.completion_rate * 100) << ","
            << std::scientific << std::setprecision(3) << metrics.avg_energy_joules << ","
            << breakdown.local_avg_joules << ","
            << breakdown.offload_avg_joules << ","
            << std::fixed << std::setprecision(2) << (metrics.offload_execution_fraction * 100) << ","
            << (metrics.avg_latency_seconds * 1000) << ","
            << (metrics.min_latency_seconds * 1000) << ","
            << (metrics.max_latency_seconds * 1000) << ","
            << (percentiles.p50_seconds * 1000) << ","
            << (percentiles.p95_seconds * 1000) << ","
            << (percentiles.p99_seconds * 1000) << std::endl;
    }
    
    csv.close();
    EV << "Metrics exported to: " << filename << std::endl;
}

void MetricsManager::reset() {
    task_metrics.clear();
    latency_history.clear();
    total_cpu_cycles_used = 0;
    total_cpu_cycles_available = 0;
    total_network_bytes_transmitted = 0;
    total_network_transmission_time = 0.0;
}
