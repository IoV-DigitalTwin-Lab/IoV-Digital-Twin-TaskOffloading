/**
 * EnergyModel.cc — IEEE TMC 2018-standard energy computation for MEC task offloading
 *
 * Reference:
 *   X. Chen and L. Huang, "Green and Mobility-Aware Caching in the Sky,"
 *   IEEE Transactions on Mobile Computing, vol. 17, no. 12, 2018.
 *   (and companion MEC energy-latency trade-off: Mao et al., IEEE JSAC 2017)
 *
 * Standard cubic model:
 *   E_loc  = κ × f³ × (c / f) = κ × f² × c          [local compute]
 *         where f = CPU frequency (Hz), c = CPU cycles
 *   E_tx   = P_tx × (d × 8 / B)                      [wireless TX]
 *         where d = payload bytes, B = link bandwidth (bps)
 *   E_rsu  = κ_rsu × f_rsu² × c                      [RSU compute]
 *
 * This replaces the previous RADiT "frequency-slack" formula
 * E = κ × (f_alloc − f_actual)² × d × c
 * which collapses to near-zero when f_alloc ≈ f_actual.
 */

#include "EnergyModel.h"
#include <cmath>

EnergyCalculator::EnergyCalculator() {}
EnergyCalculator::~EnergyCalculator() {}

// ============================================================================
// Core IEEE cubic formula: E = κ × f² × cpu_cycles
// Derivation: power P = κ × f³  (dynamic CMOS power)
//             time  t = cpu_cycles / f
//             energy E = P × t = κ × f³ × (cpu_cycles / f) = κ × f² × cpu_cycles
// ============================================================================
double EnergyCalculator::energyFormula(double kappa,
                                       double freq_hz,
                                       uint64_t cpu_cycles)
{
    if (cpu_cycles == 0 || freq_hz <= 0.0) return 0.0;
    return kappa * freq_hz * freq_hz * static_cast<double>(cpu_cycles);
}

// ============================================================================
// LOCAL execution  E_loc = κ_v × f_v² × c
// ============================================================================
double EnergyCalculator::calcLocalExecutionEnergy(uint64_t cpu_cycles,
                                                   uint32_t /*data_size_bytes*/,
                                                   double   /*execution_time_sec*/,
                                                   double   freq_allocated_hz,
                                                   double   /*freq_actual_hz*/)
{
    // Use freq_allocated_hz as the operating frequency (f_v).
    // data_size and execution_time are unused in the cubic model — kept for
    // API compatibility only.
    return energyFormula(EnergyConstants::KAPPA_VEHICLE,
                         freq_allocated_hz,
                         cpu_cycles);
}

// ============================================================================
// WIRELESS TRANSMISSION  E_tx = P_tx × (data_bytes × 8 / bandwidth_bps)
// ============================================================================
double EnergyCalculator::calcTransmissionEnergy(uint32_t data_size_bytes,
                                                double   transmission_rate_bps)
{
    if (transmission_rate_bps <= 0.0 || data_size_bytes == 0) return 0.0;
    double transmission_time_sec = (data_size_bytes * 8.0) / transmission_rate_bps;
    return EnergyConstants::TX_POWER * transmission_time_sec;
}

// ============================================================================
// RSU COMPUTE  E_rsu = κ_rsu × f_rsu² × c
// ============================================================================
double EnergyCalculator::calcRSUComputationEnergy(uint64_t cpu_cycles,
                                                   uint32_t /*data_size_bytes*/,
                                                   double   /*execution_time_sec*/,
                                                   double   freq_allocated_hz,
                                                   double   /*freq_actual_hz*/)
{
    return energyFormula(EnergyConstants::KAPPA_RSU,
                         freq_allocated_hz,
                         cpu_cycles);
}

// ============================================================================
// TOTAL OFFLOAD  E_offload = E_tx + E_rsu
// ============================================================================
double EnergyCalculator::calcOffloadEnergy(uint32_t data_size_bytes,
                                           double   transmission_rate_bps,
                                           uint64_t cpu_cycles,
                                           double   /*rsu_execution_time_sec*/)
{
    double energy_tx  = calcTransmissionEnergy(data_size_bytes, transmission_rate_bps);
    // RSU frequency: use FREQ_NOMINAL (server-class CPU at 1 GHz equivalent).
    // κ_rsu is lower than κ_vehicle to reflect better thermal management.
    double energy_rsu = calcRSUComputationEnergy(cpu_cycles,
                                                  data_size_bytes,
                                                  0.0,
                                                  EnergyConstants::FREQ_NOMINAL,
                                                  EnergyConstants::FREQ_NOMINAL);
    return energy_tx + energy_rsu;
}

// ============================================================================
// IDLE  E_idle = P_idle × t_idle
// ============================================================================
double EnergyCalculator::calcIdleEnergy(double waiting_time_sec, double idle_power)
{
    return idle_power * waiting_time_sec;
}

// ============================================================================
// PER-TASK-TYPE ENERGY ESTIMATE (uses TaskProfileDatabase)
// ============================================================================
EnergyCalculator::TaskEnergyEstimate
EnergyCalculator::estimateTaskEnergy(TaskType type, double transmission_rate_bps)
{
    const auto& profile = TaskProfileDatabase::getInstance().getProfile(type);

    TaskEnergyEstimate estimate;
    estimate.type = type;

    // Local execution at FREQ_NOMINAL
    estimate.energy_local_joules = calcLocalExecutionEnergy(
        profile.computation.cpu_cycles,
        profile.computation.input_size_bytes,
        0.0,
        EnergyConstants::FREQ_NOMINAL,
        EnergyConstants::FREQ_NOMINAL
    );

    if (profile.offloading.is_offloadable) {
        estimate.energy_offload_joules = calcOffloadEnergy(
            profile.computation.input_size_bytes,
            transmission_rate_bps,
            profile.computation.cpu_cycles,
            0.0
        );
    } else {
        estimate.energy_offload_joules = estimate.energy_local_joules;
    }

    if (estimate.energy_local_joules > 0) {
        estimate.energy_savings_ratio =
            estimate.energy_offload_joules / estimate.energy_local_joules;
    } else {
        estimate.energy_savings_ratio = 1.0;
    }

    return estimate;
}

// ============================================================================
// Convenience: print energy profile for all task types (for analysis/logging)
// ============================================================================
void printTaskEnergyProfile() {
    EnergyCalculator calc;
    const char* task_names[] = {
        "LOCAL_OBJECT_DETECTION",
        "COOPERATIVE_PERCEPTION",
        "ROUTE_OPTIMIZATION",
        "FLEET_TRAFFIC_FORECAST",
        "VOICE_COMMAND_PROCESSING",
        "SENSOR_HEALTH_CHECK"
    };

    printf("\n=== TASK ENERGY PROFILE (IEEE TMC 2018 cubic model) ===\n");
    printf("%-30s | %16s | %18s | %12s\n",
           "Task Type", "Local Energy (J)", "Offload Energy (J)", "Savings Ratio");
    printf("%s\n", std::string(82, '-').c_str());

    for (int i = 0; i < 6; i++) {
        auto est = calc.estimateTaskEnergy(static_cast<TaskType>(i), 6e6);
        printf("%-30s | %16.3e | %18.3e | %.2f\n",
               task_names[i],
               est.energy_local_joules,
               est.energy_offload_joules,
               est.energy_savings_ratio);
    }
    printf("\nNote: Savings Ratio = E_offload / E_local (< 1.0 = offload is better)\n");
}
