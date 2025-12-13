#include "TaskOffloadingDecision.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace complex_network {

// ============================================================================
// TaskOffloadingDecisionMaker - Base Class
// ============================================================================

TaskOffloadingDecisionMaker::TaskOffloadingDecisionMaker()
    : decisions_local(0), decisions_offload(0), decisions_reject(0),
      successful_local(0), failed_local(0), 
      successful_offload(0), failed_offload(0) {
}

TaskOffloadingDecisionMaker::~TaskOffloadingDecisionMaker() {
}

OffloadingDecision TaskOffloadingDecisionMaker::makeDecision(const DecisionContext& context) {
    // Base implementation - always offload
    // Subclasses should override this
    return OffloadingDecision::OFFLOAD_TO_RSU;
}

void TaskOffloadingDecisionMaker::provideFeedback(const std::string& task_id,
                                                   OffloadingDecision decision,
                                                   bool success,
                                                   double execution_time) {
    // Track success/failure
    if (decision == OffloadingDecision::EXECUTE_LOCALLY) {
        if (success) {
            successful_local++;
        } else {
            failed_local++;
        }
    } else if (decision == OffloadingDecision::OFFLOAD_TO_RSU) {
        if (success) {
            successful_offload++;
        } else {
            failed_offload++;
        }
    }
}

std::map<std::string, double> TaskOffloadingDecisionMaker::getStatistics() const {
    std::map<std::string, double> stats;
    
    stats["decisions_local"] = decisions_local;
    stats["decisions_offload"] = decisions_offload;
    stats["decisions_reject"] = decisions_reject;
    
    stats["successful_local"] = successful_local;
    stats["failed_local"] = failed_local;
    stats["successful_offload"] = successful_offload;
    stats["failed_offload"] = failed_offload;
    
    int total_local = successful_local + failed_local;
    int total_offload = successful_offload + failed_offload;
    
    stats["local_success_rate"] = total_local > 0 ? 
        (double)successful_local / total_local : 0.0;
    stats["offload_success_rate"] = total_offload > 0 ? 
        (double)successful_offload / total_offload : 0.0;
    
    return stats;
}

bool TaskOffloadingDecisionMaker::canExecuteLocally(const DecisionContext& context) {
    // Check if vehicle has enough resources
    if (context.local_cpu_available <= 0) return false;
    if (context.local_mem_available <= 0) return false;
    
    // Check if queue is too long
    if (context.local_queue_length > 10) return false;
    
    return true;
}

double TaskOffloadingDecisionMaker::estimateLocalExecutionTime(const DecisionContext& context) {
    // Simple estimation: cycles / CPU speed
    // Account for queue wait time
    double processing_time = context.cpu_cycles_required / (context.local_cpu_available * 1e9);
    double queue_wait_time = context.local_queue_length * 0.5; // Assume 0.5s per queued task
    
    return processing_time + queue_wait_time;
}

double TaskOffloadingDecisionMaker::estimateOffloadingTime(const DecisionContext& context) {
    // Transmission time + processing at RSU (assumed faster)
    double transmission_time = context.estimated_transmission_time;
    double rsu_processing_time = context.cpu_cycles_required / (4.0 * 1e9); // Assume RSU has 4 GHz
    
    return transmission_time + rsu_processing_time;
}

// ============================================================================
// HeuristicDecisionMaker - Simple Rule-Based Strategy
// ============================================================================

HeuristicDecisionMaker::HeuristicDecisionMaker()
    : TaskOffloadingDecisionMaker(),
      local_cpu_threshold(0.5),        // Need at least 0.5 GHz available
      queue_length_threshold(5),        // Max 5 tasks in queue
      rssi_threshold(-85.0),           // Min -85 dBm for offloading
      max_rsu_distance(1000.0),        // Max 1000m to RSU
      critical_qos_threshold(0.7) {    // QoS > 0.7 is critical
}

OffloadingDecision HeuristicDecisionMaker::makeDecision(const DecisionContext& context) {
    std::cout << "\n🎯 OFFLOAD_DECISION: Evaluating task offloading options..." << std::endl;
    std::cout << "   Task: size=" << context.task_size_kb << "KB, cycles=" 
              << (context.cpu_cycles_required / 1e9) << "G, deadline=" 
              << context.deadline_seconds << "s, QoS=" << context.qos_value << std::endl;
    std::cout << "   Vehicle: CPU=" << context.local_cpu_available << "GHz ("
              << (context.local_cpu_utilization * 100) << "% used), queue=" 
              << context.local_queue_length << ", processing=" 
              << context.local_processing_count << std::endl;
    std::cout << "   Network: RSU available=" << (context.rsu_available ? "YES" : "NO")
              << ", distance=" << context.rsu_distance << "m, RSSI=" 
              << context.estimated_rsu_rssi << "dBm" << std::endl;
    
    // Check if RSU is available and reachable
    bool network_ok = hasGoodNetwork(context);
    bool local_ok = hasLocalCapacity(context);
    bool is_critical = isTaskCritical(context);
    
    // Calculate scores for both options
    double local_score = calculateLocalScore(context);
    double offload_score = calculateOffloadScore(context);
    
    std::cout << "   Scores: local=" << local_score << ", offload=" << offload_score << std::endl;
    
    OffloadingDecision decision;
    
    // Decision logic
    if (!network_ok && !local_ok) {
        // Cannot handle task anywhere
        decision = OffloadingDecision::REJECT_TASK;
        last_decision_reason = "No capacity locally and RSU unreachable";
        decisions_reject++;
        std::cout << "   ❌ DECISION: REJECT (no capacity anywhere)" << std::endl;
    }
    else if (!network_ok) {
        // Must execute locally (no RSU available)
        decision = OffloadingDecision::EXECUTE_LOCALLY;
        last_decision_reason = "RSU unreachable, executing locally";
        decisions_local++;
        std::cout << "   💻 DECISION: EXECUTE_LOCALLY (no RSU available)" << std::endl;
    }
    else if (!local_ok) {
        // Must offload (no local capacity)
        decision = OffloadingDecision::OFFLOAD_TO_RSU;
        last_decision_reason = "Insufficient local resources, offloading to RSU";
        decisions_offload++;
        std::cout << "   📤 DECISION: OFFLOAD_TO_RSU (insufficient local resources)" << std::endl;
    }
    else if (is_critical && network_ok) {
        // Critical tasks prefer RSU (more reliable/faster)
        decision = OffloadingDecision::OFFLOAD_TO_RSU;
        last_decision_reason = "Critical task with good network, offloading for reliability";
        decisions_offload++;
        std::cout << "   📤 DECISION: OFFLOAD_TO_RSU (critical task, use RSU reliability)" << std::endl;
    }
    else if (offload_score > local_score * 1.2) {
        // Offloading is significantly better
        decision = OffloadingDecision::OFFLOAD_TO_RSU;
        last_decision_reason = "Offloading score significantly better than local";
        decisions_offload++;
        std::cout << "   📤 DECISION: OFFLOAD_TO_RSU (better performance expected)" << std::endl;
    }
    else {
        // Default to local execution
        decision = OffloadingDecision::EXECUTE_LOCALLY;
        last_decision_reason = "Local execution preferred (sufficient resources, save bandwidth)";
        decisions_local++;
        std::cout << "   💻 DECISION: EXECUTE_LOCALLY (sufficient resources)" << std::endl;
    }
    
    std::cout << "   Reason: " << last_decision_reason << std::endl;
    
    return decision;
}

bool HeuristicDecisionMaker::isTaskCritical(const DecisionContext& context) {
    // Critical if high QoS requirement or tight deadline
    return context.qos_value >= critical_qos_threshold || 
           context.deadline_seconds < 2.0;
}

bool HeuristicDecisionMaker::hasLocalCapacity(const DecisionContext& context) {
    // Check if vehicle can handle the task
    if (context.local_cpu_available < local_cpu_threshold) return false;
    if (context.local_queue_length >= queue_length_threshold) return false;
    if (context.local_cpu_utilization > 0.9) return false; // CPU overloaded
    
    // Check if task can meet deadline locally
    double estimated_time = estimateLocalExecutionTime(context);
    if (estimated_time > context.deadline_seconds * 0.8) return false; // Need 80% margin
    
    return true;
}

bool HeuristicDecisionMaker::hasGoodNetwork(const DecisionContext& context) {
    if (!context.rsu_available) return false;
    if (context.rsu_distance > max_rsu_distance) return false;
    if (context.estimated_rsu_rssi < rssi_threshold) return false;
    
    return true;
}

double HeuristicDecisionMaker::calculateLocalScore(const DecisionContext& context) {
    double score = 0.0;
    
    // CPU availability factor (0-1)
    double cpu_factor = std::min(1.0, context.local_cpu_available / 2.0);
    score += cpu_factor * 0.4;
    
    // Queue length factor (fewer queued = better)
    double queue_factor = std::max(0.0, 1.0 - context.local_queue_length / 10.0);
    score += queue_factor * 0.3;
    
    // Deadline feasibility
    double estimated_time = estimateLocalExecutionTime(context);
    double deadline_factor = estimated_time < context.deadline_seconds ? 
        (context.deadline_seconds - estimated_time) / context.deadline_seconds : 0.0;
    score += deadline_factor * 0.3;
    
    return score;
}

double HeuristicDecisionMaker::calculateOffloadScore(const DecisionContext& context) {
    if (!hasGoodNetwork(context)) return 0.0;
    
    double score = 0.0;
    
    // Network quality factor (RSSI-based)
    double rssi_factor = (context.estimated_rsu_rssi - rssi_threshold) / 
                         (-40.0 - rssi_threshold); // Normalize -85 to -40 dBm
    rssi_factor = std::max(0.0, std::min(1.0, rssi_factor));
    score += rssi_factor * 0.4;
    
    // Distance factor (closer = better)
    double distance_factor = std::max(0.0, 1.0 - context.rsu_distance / max_rsu_distance);
    score += distance_factor * 0.3;
    
    // Deadline feasibility for offloading
    double estimated_time = estimateOffloadingTime(context);
    double deadline_factor = estimated_time < context.deadline_seconds ?
        (context.deadline_seconds - estimated_time) / context.deadline_seconds : 0.0;
    score += deadline_factor * 0.3;
    
    return score;
}

// ============================================================================
// DRLDecisionMaker - Placeholder for Deep Reinforcement Learning
// ============================================================================

DRLDecisionMaker::DRLDecisionMaker()
    : TaskOffloadingDecisionMaker(),
      exploration_rate(0.1),
      learning_rate(0.001),
      use_pretrained_model(false) {
    std::cout << "DRL Decision Maker initialized (placeholder - not yet implemented)" << std::endl;
}

OffloadingDecision DRLDecisionMaker::makeDecision(const DecisionContext& context) {
    // TODO: Implement DRL-based decision making
    // For now, fall back to simple heuristic
    std::cout << "⚠️  DRL not implemented yet, using simple heuristic fallback" << std::endl;
    
    // Simple fallback: offload if RSU available and CPU busy
    if (context.rsu_available && context.local_cpu_utilization > 0.7) {
        decisions_offload++;
        return OffloadingDecision::OFFLOAD_TO_RSU;
    } else {
        decisions_local++;
        return OffloadingDecision::EXECUTE_LOCALLY;
    }
}

void DRLDecisionMaker::provideFeedback(const std::string& task_id,
                                       OffloadingDecision decision,
                                       bool success,
                                       double execution_time) {
    TaskOffloadingDecisionMaker::provideFeedback(task_id, decision, success, execution_time);
    
    // TODO: Store experience in replay buffer
    // TODO: Trigger training if buffer is full
}

void DRLDecisionMaker::loadModel(const std::string& model_path) {
    // TODO: Load pre-trained DRL model
    std::cout << "TODO: Load DRL model from " << model_path << std::endl;
}

void DRLDecisionMaker::saveModel(const std::string& model_path) {
    // TODO: Save trained DRL model
    std::cout << "TODO: Save DRL model to " << model_path << std::endl;
}

void DRLDecisionMaker::setLearningRate(double rate) {
    learning_rate = rate;
}

void DRLDecisionMaker::setExplorationRate(double rate) {
    exploration_rate = rate;
}

} // namespace complex_network
