#include "EnergyModel.h"
#include <algorithm>
#include <cmath>

EnergyCalculator::EnergyCalculator() {
}

EnergyCalculator::~EnergyCalculator() {
}

double EnergyCalculator::energyFormula(double kappa,
                                       double frequency_hz,
                                       uint64_t cpu_cycles) {
    // Compute-energy model: E = kappa * f * cycles
    if (frequency_hz <= 0.0 || cpu_cycles == 0) {
        return 0.0;
    }

    return kappa * frequency_hz * static_cast<double>(cpu_cycles);
}

double EnergyCalculator::calcLocalExecutionEnergy(uint64_t cpu_cycles,
                                                  uint32_t data_size_bytes,
                                                  double execution_time_sec,
                                                  double freq_allocated_hz,
                                                  double freq_actual_hz) {
    // Local execution on vehicle: E_loc = kappa_vehicle * f * cycles
    (void)data_size_bytes;
    (void)execution_time_sec;
    double effective_freq_hz = (freq_actual_hz > 0.0) ? freq_actual_hz : freq_allocated_hz;
    return energyFormula(EnergyConstants::KAPPA_VEHICLE,
                         effective_freq_hz,
                         cpu_cycles);
}

double EnergyCalculator::calcTransmissionEnergy(uint32_t data_size_bytes,
                                               double transmission_rate_bps) {
    // E_tx = P_tx × t_tx
    // where t_tx = data_size_bytes × 8 / transmission_rate_bps
    
    if (data_size_bytes == 0) {
        return 0.0;
    }

    // Floor the link rate to avoid pathological/overflow transmission energies.
    double safe_rate_bps = std::max(transmission_rate_bps, EnergyConstants::MIN_TX_RATE_BPS);
    double transmission_time_sec = (data_size_bytes * 8.0) / safe_rate_bps;
    double energy_tx = EnergyConstants::TX_POWER * transmission_time_sec;
    
    return energy_tx;
}

double EnergyCalculator::calcRSUComputationEnergy(uint64_t cpu_cycles,
                                                 uint32_t data_size_bytes,
                                                 double execution_time_sec,
                                                 double freq_allocated_hz,
                                                 double freq_actual_hz) {
    // RSU computation energy: E_rsu = kappa_rsu * f * cycles
    (void)data_size_bytes;
    (void)execution_time_sec;
    double effective_freq_hz = (freq_actual_hz > 0.0) ? freq_actual_hz : freq_allocated_hz;
    return energyFormula(EnergyConstants::KAPPA_RSU,
                         effective_freq_hz,
                         cpu_cycles);
}

double EnergyCalculator::calcOffloadEnergy(uint32_t data_size_bytes,
                                          double transmission_rate_bps,
                                          uint64_t cpu_cycles,
                                          double rsu_execution_time_sec) {
    // Total offload energy = transmission + RSU computation
    double energy_tx = calcTransmissionEnergy(data_size_bytes, transmission_rate_bps);
    double energy_rsu = calcRSUComputationEnergy(
        cpu_cycles,
        data_size_bytes,
        rsu_execution_time_sec,
        EnergyConstants::FREQ_RSU_NOMINAL,
        EnergyConstants::FREQ_RSU_NOMINAL
    );
    
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
    
    // Local execution at vehicle nominal frequency
    estimate.energy_local_joules = calcLocalExecutionEnergy(
        profile.computation.cpu_cycles,
        profile.computation.input_size_bytes,
        0.0,
        EnergyConstants::FREQ_NOMINAL,
        EnergyConstants::FREQ_NOMINAL
    );
    
    // Offload execution (if offloadable)
    if (profile.offloading.is_offloadable) {
        // RSU is modeled as faster than vehicle.
        double rsu_exec_time = static_cast<double>(profile.computation.cpu_cycles)
                     / EnergyConstants::FREQ_RSU_NOMINAL;
        
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
    const auto& task_types = TaskProfileDatabase::getAllTaskTypes();
    
    std::cout << "\n=== TASK ENERGY PROFILE ===" << std::endl;
    std::cout << "Task Type | Local Energy (J) | Offload Energy (J) | Savings Ratio" << std::endl;
    std::cout << "-------------------------------------------------------------------" << std::endl;
    
    for (const auto& type : task_types) {
        auto estimate = calc.estimateTaskEnergy(type, 6e6);
        std::string task_name = TaskProfileDatabase::getTaskTypeName(type);
        
        printf("%-30s | %16.3e | %18.3e | %.2f\n",
               task_name.c_str(),
               estimate.energy_local_joules,
               estimate.energy_offload_joules,
               estimate.energy_savings_ratio);
    }
    std::cout << "\nNote: Savings Ratio = E_offload / E_local (< 1.0 = offload is better)" << std::endl;
}
