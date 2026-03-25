
#include "TaskOffloadingDecision.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <omnetpp.h>

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

GateBDecisionResult TaskOffloadingDecisionMaker::makeDecisionDetailed(const DecisionContext& context) {
    GateBDecisionResult result;
    result.decision = makeDecision(context);
    result.reason = last_decision_reason;

    // Step 1 compatibility mapping only; true Gate B classification is added
    // in later steps when formulas and stage rules are implemented.
    switch (result.decision) {
        case OffloadingDecision::OFFLOAD_TO_RSU:
        case OffloadingDecision::OFFLOAD_TO_SERVICE_VEHICLE:
            result.classification = GateBClassification::MUST_OFFLOAD;
            break;
        case OffloadingDecision::EXECUTE_LOCALLY:
            result.classification = GateBClassification::MUST_LOCAL;
            break;
        case OffloadingDecision::REJECT_TASK:
            result.classification = GateBClassification::INFEASIBLE;
            break;
    }

    // Keep numerical fields neutral until Gate B math is wired in.
    result.t_local_seconds = 0.0;
    result.t_offload_seconds = 0.0;
    result.remaining_deadline_seconds = context.deadline_seconds;
    result.cost_local = 0.0;
    result.cost_offload = 0.0;

    return result;
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
// HeuristicDecisionMaker - Simple 3-rule placeholder (pre-DRL phase)
//
// Rule 1 — OFFLOAD_TO_RSU  : RSU reachable AND RSU has free capacity
// Rule 2 — EXECUTE_LOCALLY : local queue below ceiling AND CPU not saturated
// Rule 3 — REJECT_TASK     : neither option is viable
//
// The RSU itself enforces a hard admission limit, so Rule 1 is optimistic but
// safe: occasional over-estimates are caught by the RSU before queuing.
// ============================================================================

HeuristicDecisionMaker::HeuristicDecisionMaker()
    : TaskOffloadingDecisionMaker() {
}

OffloadingDecision HeuristicDecisionMaker::makeDecision(const DecisionContext& context) {
    // Rule 1: prefer RSU when reachable and not at capacity
    bool rsu_reachable  = context.rsu_available;
    bool rsu_has_space  = context.rsu_processing_count < context.rsu_max_concurrent;

    if (rsu_reachable && rsu_has_space) {
        last_decision_reason = "RSU reachable and has capacity";
        decisions_offload++;
        std::cout << "[OFFLOAD_DECISION] OFFLOAD_TO_RSU — " << last_decision_reason << std::endl;
        return OffloadingDecision::OFFLOAD_TO_RSU;
    }

    // Rule 2: fall back to local execution if vehicle has headroom
    bool local_has_space = context.local_queue_length < 10;
    bool cpu_not_full    = context.local_cpu_utilization < 0.95;

    if (local_has_space && cpu_not_full) {
        last_decision_reason = rsu_reachable
            ? "RSU at capacity, falling back to local"
            : "RSU unreachable, executing locally";
        decisions_local++;
        std::cout << "[OFFLOAD_DECISION] EXECUTE_LOCALLY — " << last_decision_reason << std::endl;
        return OffloadingDecision::EXECUTE_LOCALLY;
    }

    // Rule 3: both paths exhausted
    last_decision_reason = "Local queue full and RSU unavailable/at-capacity";
    decisions_reject++;
    std::cout << "[OFFLOAD_DECISION] REJECT_TASK — " << last_decision_reason << std::endl;
    return OffloadingDecision::REJECT_TASK;
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
