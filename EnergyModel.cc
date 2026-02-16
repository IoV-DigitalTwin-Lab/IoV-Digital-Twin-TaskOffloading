#include "EnergyModel.h"
#include <cmath>

EnergyCalculator::EnergyCalculator() {
}

EnergyCalculator::~EnergyCalculator() {
}

double EnergyCalculator::energyFormula(double kappa,
                                       double freq_allocated,
                                       double freq_actual,
                                       uint64_t cpu_cycles,
                                       uint32_t data_size_bytes) {
    // RADiT formula: E = κ × (f_alloc - f_actual)² × d × c
    // Simplified for cases where f_alloc ≈ f_actual:
    // E ≈ κ × f² × d × c (dominant term)
    
    if (data_size_bytes == 0 || cpu_cycles == 0) {
        return 0.0;
    }
    
    double freq_diff = freq_allocated - freq_actual;
    double energy = kappa * freq_diff * freq_diff * cpu_cycles * data_size_bytes;
    
    return energy;
}

double EnergyCalculator::calcLocalExecutionEnergy(uint64_t cpu_cycles,
                                                  uint32_t data_size_bytes,
                                                  double execution_time_sec,
                                                  double freq_allocated_hz,
                                                  double freq_actual_hz) {
    // Local execution on vehicle
    // E_loc = κ_vehicle × (f_alloc - f_actual)² × d × c
    
    return energyFormula(EnergyConstants::KAPPA_VEHICLE,
                        freq_allocated_hz,
                        freq_actual_hz,
                        cpu_cycles,
                        data_size_bytes);
}

double EnergyCalculator::calcTransmissionEnergy(uint32_t data_size_bytes,
                                               double transmission_rate_bps) {
    // E_tx = P_tx × t_tx
    // where t_tx = data_size_bytes × 8 / transmission_rate_bps
    
    if (transmission_rate_bps == 0 || data_size_bytes == 0) {
        return 0.0;
    }
    
    double transmission_time_sec = (data_size_bytes * 8.0) / transmission_rate_bps;
    double energy_tx = EnergyConstants::TX_POWER * transmission_time_sec;
    
    return energy_tx;
}

double EnergyCalculator::calcRSUComputationEnergy(uint64_t cpu_cycles,
                                                 uint32_t data_size_bytes,
                                                 double execution_time_sec,
                                                 double freq_allocated_hz,
                                                 double freq_actual_hz) {
    // RSU computation energy
    // E_rsu = κ_rsu × (f_alloc - f_actual)² × d × c
    // RSU typically has lower κ due to better cooling and power efficiency
    
    return energyFormula(EnergyConstants::KAPPA_RSU,
                        freq_allocated_hz,
                        freq_actual_hz,
                        cpu_cycles,
                        data_size_bytes);
}

double EnergyCalculator::calcOffloadEnergy(uint32_t data_size_bytes,
                                          double transmission_rate_bps,
                                          uint64_t cpu_cycles,
                                          double rsu_execution_time_sec) {
    // Total offload energy = transmission + RSU computation
    double energy_tx = calcTransmissionEnergy(data_size_bytes, transmission_rate_bps);
    double energy_rsu = calcRSUComputationEnergy(cpu_cycles, data_size_bytes, rsu_execution_time_sec);
    
    return energy_tx + energy_rsu;
}

double EnergyCalculator::calcIdleEnergy(double waiting_time_sec,
                                       double idle_power) {
    // E_idle = P_idle × t_idle
    return idle_power * waiting_time_sec;
}

EnergyCalculator::TaskEnergyEstimate EnergyCalculator::estimateTaskEnergy(TaskType type,
                                                                           double transmission_rate_bps) {
    const auto& profile = TaskProfileDatabase::getInstance().getProfile(type);
    
    TaskEnergyEstimate estimate;
    estimate.type = type;
    
    // Local execution: assume standard CPU frequency
    estimate.energy_local_joules = calcLocalExecutionEnergy(
        profile.computation.cpu_cycles,
        profile.computation.input_size_bytes,
        0.0,  // execution_time not used in formula
        EnergyConstants::FREQ_NOMINAL,
        EnergyConstants::FREQ_NOMINAL
    );
    
    // Offload execution (if offloadable)
    if (profile.offloading.is_offloadable) {
        // Estimate RSU execution time: time = cycles / frequency
        double rsu_exec_time = (double)profile.computation.cpu_cycles / EnergyConstants::FREQ_NOMINAL;
        
        estimate.energy_offload_joules = calcOffloadEnergy(
            profile.computation.input_size_bytes,
            transmission_rate_bps,
            profile.computation.cpu_cycles,
            rsu_exec_time
        );
    } else {
        // Non-offloadable tasks have same energy everywhere
        estimate.energy_offload_joules = estimate.energy_local_joules;
    }
    
    // Energy savings ratio (lower = more savings)
    if (estimate.energy_local_joules > 0) {
        estimate.energy_savings_ratio = estimate.energy_offload_joules / estimate.energy_local_joules;
    } else {
        estimate.energy_savings_ratio = 1.0;
    }
    
    return estimate;
}

// ============================================================================
// Convenience functions for direct use in simulation
// ============================================================================

/**
 * Estimate energy cost for all 6 task types
 * Useful for analysis and logging
 */
void printTaskEnergyProfile() {
    EnergyCalculator calc;
    std::string task_names[] = {
        "LOCAL_OBJECT_DETECTION",
        "COOPERATIVE_PERCEPTION",
        "ROUTE_OPTIMIZATION",
        "FLEET_TRAFFIC_FORECAST",
        "VOICE_COMMAND_PROCESSING",
        "SENSOR_HEALTH_CHECK"
    };
    
    std::cout << "\n=== TASK ENERGY PROFILE ===" << std::endl;
    std::cout << "Task Type | Local Energy (J) | Offload Energy (J) | Savings Ratio" << std::endl;
    std::cout << "-------------------------------------------------------------------" << std::endl;
    
    for (int i = 0; i < 6; i++) {
        auto estimate = calc.estimateTaskEnergy(static_cast<TaskType>(i), 6e6);
        
        printf("%-30s | %16.3e | %18.3e | %.2f\n",
               task_names[i].c_str(),
               estimate.energy_local_joules,
               estimate.energy_offload_joules,
               estimate.energy_savings_ratio);
    }
    std::cout << "\nNote: Savings Ratio = E_offload / E_local (< 1.0 = offload is better)" << std::endl;
}
