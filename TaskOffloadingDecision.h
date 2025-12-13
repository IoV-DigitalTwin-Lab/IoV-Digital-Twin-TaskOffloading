#pragma once

#include <string>
#include <map>

namespace complex_network {

// Forward declaration
class Task;

/**
 * Offloading decision result
 */
enum class OffloadingDecision {
    EXECUTE_LOCALLY,               // Process on vehicle
    OFFLOAD_TO_RSU,                // Send to RSU for processing
    OFFLOAD_TO_SERVICE_VEHICLE,    // Send to another vehicle for processing
    REJECT_TASK                    // Cannot handle (insufficient resources)
};

/**
 * Context information for making offloading decisions
 */
struct DecisionContext {
    // Task characteristics
    double task_size_kb;
    double cpu_cycles_required;
    double qos_value;
    double deadline_seconds;
    
    // Vehicle state
    double local_cpu_available;      // Available CPU (GHz)
    double local_cpu_utilization;    // Current CPU usage (0-1)
    double local_mem_available;      // Available memory (MB)
    int local_queue_length;          // Tasks in queue
    int local_processing_count;      // Tasks being processed
    
    // Network/RSU state
    bool rsu_available;              // Is RSU reachable?
    double rsu_distance;             // Distance to nearest RSU (m)
    double estimated_rsu_rssi;       // Signal strength (dBm)
    double estimated_transmission_time; // Time to send task data (s)
    
    // Performance history
    double local_success_rate;       // Historical success rate for local exec
    double offload_success_rate;     // Historical success rate for offloading
    
    // Current time
    double current_time;
};

/**
 * Base class for task offloading decision strategies
 * Can be extended with different algorithms (heuristic, DRL, etc.)
 */
class TaskOffloadingDecisionMaker {
public:
    TaskOffloadingDecisionMaker();
    virtual ~TaskOffloadingDecisionMaker();
    
    /**
     * Make offloading decision for a given task
     * @param context Decision context with task and system state
     * @return Offloading decision (local, offload, or reject)
     */
    virtual OffloadingDecision makeDecision(const DecisionContext& context);
    
    /**
     * Update decision maker with feedback (for learning algorithms)
     * @param task_id Task identifier
     * @param decision Decision that was made
     * @param success Whether the task completed successfully
     * @param execution_time Actual execution time
     */
    virtual void provideFeedback(const std::string& task_id, 
                                  OffloadingDecision decision,
                                  bool success,
                                  double execution_time);
    
    /**
     * Get statistics about decisions made
     */
    virtual std::map<std::string, double> getStatistics() const;
    
    /**
     * Get human-readable reason for last decision
     */
    virtual std::string getLastDecisionReason() const { return last_decision_reason; }

protected:
    // Statistics tracking
    int decisions_local;
    int decisions_offload;
    int decisions_reject;
    
    int successful_local;
    int failed_local;
    int successful_offload;
    int failed_offload;
    
    std::string last_decision_reason;
    
    // Helper methods
    bool canExecuteLocally(const DecisionContext& context);
    double estimateLocalExecutionTime(const DecisionContext& context);
    double estimateOffloadingTime(const DecisionContext& context);
};

/**
 * Simple heuristic-based decision maker
 * Uses thresholds and simple rules for offloading decisions
 */
class HeuristicDecisionMaker : public TaskOffloadingDecisionMaker {
public:
    HeuristicDecisionMaker();
    
    OffloadingDecision makeDecision(const DecisionContext& context) override;
    
    // Configuration parameters
    void setLocalCpuThreshold(double threshold) { local_cpu_threshold = threshold; }
    void setQueueLengthThreshold(int threshold) { queue_length_threshold = threshold; }
    void setRssiThreshold(double threshold) { rssi_threshold = threshold; }
    void setMaxRsuDistance(double distance) { max_rsu_distance = distance; }
    
private:
    // Thresholds for decision making
    double local_cpu_threshold;      // Min CPU required for local exec (GHz)
    int queue_length_threshold;      // Max queue length for local exec
    double rssi_threshold;           // Min RSSI for offloading (dBm)
    double max_rsu_distance;         // Max distance for offloading (m)
    double critical_qos_threshold;   // QoS threshold for critical tasks
    
    // Decision logic helpers
    bool isTaskCritical(const DecisionContext& context);
    bool hasLocalCapacity(const DecisionContext& context);
    bool hasGoodNetwork(const DecisionContext& context);
    double calculateLocalScore(const DecisionContext& context);
    double calculateOffloadScore(const DecisionContext& context);
};

/**
 * DRL-based decision maker (placeholder for future implementation)
 * Will use Deep Reinforcement Learning for intelligent offloading
 */
class DRLDecisionMaker : public TaskOffloadingDecisionMaker {
public:
    DRLDecisionMaker();
    
    OffloadingDecision makeDecision(const DecisionContext& context) override;
    void provideFeedback(const std::string& task_id, 
                        OffloadingDecision decision,
                        bool success,
                        double execution_time) override;
    
    // DRL-specific methods (to be implemented)
    void loadModel(const std::string& model_path);
    void saveModel(const std::string& model_path);
    void setLearningRate(double rate);
    void setExplorationRate(double rate);
    
private:
    // DRL state representation
    // TODO: Implement neural network model
    // TODO: Implement experience replay buffer
    // TODO: Implement training logic
    
    double exploration_rate;
    double learning_rate;
    bool use_pretrained_model;
};

} // namespace complex_network
