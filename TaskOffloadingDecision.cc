
#include "TaskOffloadingDecision.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <sstream>
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
    } else if (decision == OffloadingDecision::OFFLOAD_TO_RSU ||
               decision == OffloadingDecision::OFFLOAD_TO_SERVICE_VEHICLE) {
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
    double queue_wait_time = std::max(0.0, context.local_queue_wait_seconds);
    
    return processing_time + queue_wait_time;
}

double TaskOffloadingDecisionMaker::estimateOffloadingTime(const DecisionContext& context) {
    // Transmission time + processing at RSU (assumed faster)
    double transmission_time = context.estimated_transmission_time;
    double rsu_processing_time = context.cpu_cycles_required / EnergyConstants::FREQ_RSU_NOMINAL; // Use nominal RSU CPU from EnergyModel
    
    return transmission_time + rsu_processing_time;
}

// ============================================================================
// HeuristicDecisionMaker - Gate B feasibility classification (Step 3)
// ============================================================================

HeuristicDecisionMaker::HeuristicDecisionMaker()
    : TaskOffloadingDecisionMaker(),
      gate_alpha(0.6),
      gate_beta(0.4),
      gate_safety_margin_sec(0.05),
      k1(4.0 / 3.0),
      k2(200.0),
      p_compute_w(25.0),
      p_tx_w(2.0),
      p_rx_w(0.5) {
}

void HeuristicDecisionMaker::setGateWeights(double alpha, double beta) {
    if (alpha >= 0.0 && beta >= 0.0 && (alpha + beta) > 0.0) {
        double sum = alpha + beta;
        gate_alpha = alpha / sum;
        gate_beta = beta / sum;
    }
}

void HeuristicDecisionMaker::setGateSafetyMarginSeconds(double margin_sec) {
    gate_safety_margin_sec = std::max(0.0, margin_sec);
}

void HeuristicDecisionMaker::setStageThresholds(double k1_value, double k2_value) {
    k1 = std::max(0.0, k1_value);
    k2 = std::max(0.0, k2_value);
}

void HeuristicDecisionMaker::setEnergyPowers(double p_compute, double p_tx, double p_rx) {
    p_compute_w = std::max(0.0, p_compute);
    p_tx_w = std::max(0.0, p_tx);
    p_rx_w = std::max(0.0, p_rx);
}

GateBDecisionResult HeuristicDecisionMaker::makeDecisionDetailed(const DecisionContext& context) {
    GateBDecisionResult result;

    const double remaining_deadline = std::max(0.0, context.deadline_seconds - gate_safety_margin_sec);
    const double local_cpu_hz = std::max(1e6, context.local_cpu_available * 1e9);

    // Step 3 formulas from README_GATES (with currently available context fields)
    const double t_local = std::max(0.0, context.cpu_cycles_required) / local_cpu_hz;
    const double t_tx = std::max(0.0, context.estimated_transmission_time);
    const double rsu_cpu_hz = std::max(1e6, context.rsu_cpu_available_ghz * 1e9);
    const double t_edge = std::max(0.0, context.cpu_cycles_required) / rsu_cpu_hz;

    // Estimate downlink receive time similarly to uplink transmission model:
    // data transfer + propagation + fixed MAC overhead.
    const double downlink_mbps = std::max(0.001, context.estimated_downlink_bandwidth_mbps);
    const double output_size_bytes = std::max(0.0, context.output_size_kb) * 1024.0;
    const double t_rx_data = (output_size_bytes * 8.0) / (downlink_mbps * 1e6);
    const double t_rx_propagation = std::max(0.0, context.rsu_distance) / 3e8;
    const double t_rx_overhead = 0.001;
    const double t_rx = t_rx_data + t_rx_propagation + t_rx_overhead;

    const double t_offload = t_tx + t_edge + t_rx;

    result.t_local_seconds = t_local;
    result.t_offload_seconds = t_offload;
    result.remaining_deadline_seconds = remaining_deadline;

    // ---------------------------------------------------------------------
    // Step 5: Stage-2 override (higher priority than Stage-1)
    // If the task requires edge acceleration or cooperative processing,
    // force RSU execution regardless of Stage-1 local/offload tags.
    // ---------------------------------------------------------------------
    if (context.gpu_required_tag || context.cooperation_required_tag) {
        result.classification = GateBClassification::MUST_OFFLOAD;
        result.decision = OffloadingDecision::OFFLOAD_TO_RSU;

        std::ostringstream reason;
        if (context.gpu_required_tag && context.cooperation_required_tag) {
            reason << "STAGE2_FORCE_RSU_GPU_AND_COOP "
                   << "(gpu_required_tag=true, cooperation_required_tag=true)";
        } else if (context.gpu_required_tag) {
            reason << "STAGE2_FORCE_RSU_GPU "
                   << "(gpu_required_tag=true)";
        } else {
            reason << "STAGE2_FORCE_RSU_COOP "
                   << "(cooperation_required_tag=true)";
        }
        result.reason = reason.str();

        last_decision_reason = result.reason;
        std::cout << "[GATE_B] " << result.reason
                  << " (overrides Stage-1)"
                  << std::endl;
        return result;
    }

    // ---------------------------------------------------------------------
    // Step 4: Stage-1 forced-rule checks (executed before feasibility branch)
    // ---------------------------------------------------------------------
    const double task_size_bytes = std::max(1.0, context.task_size_kb * 1024.0);
    const double available_cycles_in_budget = local_cpu_hz * remaining_deadline;
    const bool local_capacity_ok = available_cycles_in_budget >= (k1 * context.cpu_cycles_required);
    const bool compute_data_ok = context.cpu_cycles_required < (k2 * task_size_bytes);
    const bool must_local_by_threshold = local_capacity_ok && compute_data_ok;

    const double queue_wait_sec = std::max(0.0, context.local_queue_wait_seconds);
    const bool must_offload_by_queue = (queue_wait_sec + t_local) >= remaining_deadline;

    if (context.must_local_tag || must_local_by_threshold) {
        result.classification = GateBClassification::MUST_LOCAL;
        result.decision = OffloadingDecision::EXECUTE_LOCALLY;
        std::ostringstream reason;
        if (context.must_local_tag) {
            reason << "STAGE1_FORCE_LOCAL_TAG "
                   << "(must_local_tag=true)";
        } else {
            reason << "STAGE1_FORCE_LOCAL_K1K2 "
                   << "(local_capacity_ok=true, compute_data_ok=true, k1=" << k1
                   << ", k2=" << k2 << ")";
        }
        result.reason = reason.str();
        last_decision_reason = result.reason;
        std::cout << "[GATE_B] " << result.reason
                  << " cap_ok=" << (local_capacity_ok ? "1" : "0")
                  << " comp_data_ok=" << (compute_data_ok ? "1" : "0")
                  << std::endl;
        return result;
    }

    if (context.must_offload_tag || must_offload_by_queue) {
        result.classification = GateBClassification::MUST_OFFLOAD;
        result.decision = OffloadingDecision::OFFLOAD_TO_RSU;
        std::ostringstream reason;
        if (context.must_offload_tag) {
            reason << "STAGE1_FORCE_OFFLOAD_TAG "
                   << "(must_offload_tag=true)";
        } else {
            reason << "STAGE1_FORCE_OFFLOAD_QUEUE_PRESSURE "
                   << "(queue_wait_s=" << queue_wait_sec
                   << ", local_exec_s=" << t_local
                   << ", remaining_deadline_s=" << remaining_deadline << ")";
        }
        result.reason = reason.str();
        last_decision_reason = result.reason;
        std::cout << "[GATE_B] " << result.reason
                  << " queue_wait=" << queue_wait_sec
                  << " t_local=" << t_local
                  << " rem=" << remaining_deadline
                  << std::endl;
        return result;
    }

    const bool can_do_locally = (remaining_deadline > 0.0) && (t_local < remaining_deadline);
    const bool can_offload = context.rsu_available && (remaining_deadline > 0.0) && (t_offload < remaining_deadline);

    if (!can_do_locally && can_offload) {
        result.classification = GateBClassification::MUST_OFFLOAD;
        result.decision = OffloadingDecision::OFFLOAD_TO_RSU;
        result.reason = "GATE_B_LOCAL_MISS_OFFLOAD_OK (t_local>=remaining_deadline, t_offload<remaining_deadline)";
    } else if (can_do_locally && !can_offload) {
        result.classification = GateBClassification::MUST_LOCAL;
        result.decision = OffloadingDecision::EXECUTE_LOCALLY;
        result.reason = "GATE_B_LOCAL_OK_OFFLOAD_MISS (t_local<remaining_deadline, t_offload>=remaining_deadline)";
    } else if (!can_do_locally && !can_offload) {
        result.classification = GateBClassification::INFEASIBLE;
        result.decision = OffloadingDecision::REJECT_TASK;
        result.reason = "GATE_B_BOTH_MISS_DEADLINE (t_local>=remaining_deadline, t_offload>=remaining_deadline)";
    } else {
        result.classification = GateBClassification::BOTH_FEASIBLE;

        // Cost function for BOTH_FEASIBLE
        const double e_local = p_compute_w * t_local;
        const double e_offload = (p_tx_w * t_tx) + (p_rx_w * t_rx);
        const double e_ref = std::max(1e-9, std::max(e_local, e_offload));
        const double d_ref = std::max(1e-9, remaining_deadline);

        result.cost_local = gate_alpha * (t_local / d_ref) + gate_beta * (e_local / e_ref);
        result.cost_offload = gate_alpha * (t_offload / d_ref) + gate_beta * (e_offload / e_ref);

        result.decision = (result.cost_offload < result.cost_local)
            ? OffloadingDecision::OFFLOAD_TO_RSU
            : OffloadingDecision::EXECUTE_LOCALLY;
    }

    if (result.reason.empty()) {
        if (result.classification == GateBClassification::BOTH_FEASIBLE) {
            result.reason = (result.decision == OffloadingDecision::EXECUTE_LOCALLY)
                ? "GATE_B_BOTH_FEASIBLE_LOCAL_SELECTED (cost_local<=cost_offload)"
                : "GATE_B_BOTH_FEASIBLE_OFFLOAD_SELECTED (cost_offload<cost_local)";
        } else if (result.classification == GateBClassification::MUST_OFFLOAD) {
            result.reason = "GATE_B_MUST_OFFLOAD (offload path required by constraints)";
        } else if (result.classification == GateBClassification::MUST_LOCAL) {
            result.reason = "GATE_B_MUST_LOCAL (local path required by constraints)";
        } else {
            result.reason = "GATE_B_INFEASIBLE (neither local nor offload meets deadline)";
        }
    }
    last_decision_reason = result.reason;

    std::cout << "[GATE_B] " << result.reason << std::endl;
    return result;
}

OffloadingDecision HeuristicDecisionMaker::makeDecision(const DecisionContext& context) {
    GateBDecisionResult result = makeDecisionDetailed(context);
    switch (result.decision) {
        case OffloadingDecision::EXECUTE_LOCALLY:
            decisions_local++;
            break;
        case OffloadingDecision::OFFLOAD_TO_RSU:
        case OffloadingDecision::OFFLOAD_TO_SERVICE_VEHICLE:
            decisions_offload++;
            break;
        case OffloadingDecision::REJECT_TASK:
            decisions_reject++;
            break;
    }
    std::cout << "[OFFLOAD_DECISION] "
              << (result.decision == OffloadingDecision::EXECUTE_LOCALLY ? "EXECUTE_LOCALLY" :
                  result.decision == OffloadingDecision::OFFLOAD_TO_RSU ? "OFFLOAD_TO_RSU" :
                  result.decision == OffloadingDecision::OFFLOAD_TO_SERVICE_VEHICLE ? "OFFLOAD_TO_SERVICE_VEHICLE" :
                  "REJECT_TASK")
              << " — " << last_decision_reason << std::endl;
    return result.decision;
}

} // namespace complex_network
