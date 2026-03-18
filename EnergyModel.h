#ifndef ENERGYMODEL_H_
#define ENERGYMODEL_H_

#include <omnetpp.h>
#include "TaskProfile.h"

using namespace omnetpp;
using complex_network::TaskType;
using complex_network::TaskProfileDatabase;

/**
 * Energy Model for Task Execution
 * Based on IEEE TMC 2018 standard cubic energy model (Chen & Huang, 2018)
 * and MEC energy-latency trade-off formulation (Mao et al., IEEE JSAC 2017).
 *
 * Key components:
 * 1. Local execution:  E_loc = κ_vehicle × f² × c
 *    (derivation: P = κ×f³, t = c/f → E = κ×f²×c)
 * 2. Transmission:     E_tx  = P_tx × (d×8 / B)
 * 3. RSU computation:  E_rsu = κ_rsu × f_rsu² × c
 */

namespace EnergyConstants {
    // Effective switching capacitance (Joules / Hz² / cycle)
    // Values from IEEE TMC 2018 (Chen & Huang) and IoV MEC literature:
    //   Vehicle (ARM Cortex-A57 / Jetson Nano-class):  κ_v ≈ 10e-28
    //   RSU edge server (x86 server-class CPU):        κ_rsu ≈ 5e-28
    // These give physically realistic local execution energies in the
    // 0.1 mJ – 10 mJ range for 10^8 – 10^9 cycle tasks at ~1 GHz.
    constexpr double KAPPA_VEHICLE = 10e-28;    // J / (Hz² · cycle)  — vehicle
    constexpr double KAPPA_RSU    = 5e-28;     // J / (Hz² · cycle)  — RSU server

    // Transmit power (Watts)  —  802.11p OCB: ~30 dBm = 1 W
    constexpr double TX_POWER  = 1.0;   // W
    // Receive / idle power
    constexpr double RX_POWER  = 0.8;   // W
    constexpr double IDLE_POWER = 0.2;  // W

    // Reference CPU frequencies (Hz)
    constexpr double FREQ_NOMINAL = 1.0e9;  // 1.0 GHz
    constexpr double FREQ_MAX     = 1.5e9;  // 1.5 GHz  (burst)
    constexpr double FREQ_MIN     = 0.5e9;  // 0.5 GHz  (power-save)
}

/**
 * Encapsulates energy calculation logic for all 6 task types
 */
class EnergyCalculator {
public:
    EnergyCalculator();
    virtual ~EnergyCalculator();
    
    /**
     * Calculate energy for local task execution (IEEE TMC 2018 cubic model)
     * E_loc = κ_vehicle × f² × cpu_cycles
     *
     * @param cpu_cycles        task CPU cycles
     * @param data_size_bytes   unused — kept for API compatibility
     * @param execution_time_sec unused — kept for API compatibility
     * @param freq_allocated_hz operating CPU frequency (Hz); defaults to FREQ_NOMINAL
     * @param freq_actual_hz    unused — kept for API compatibility
     * @return energy consumed in Joules
     */
    double calcLocalExecutionEnergy(uint64_t cpu_cycles, 
                                   uint32_t data_size_bytes,
                                   double execution_time_sec,
                                   double freq_allocated_hz = EnergyConstants::FREQ_NOMINAL,
                                   double freq_actual_hz = EnergyConstants::FREQ_NOMINAL);
    
    /**
     * Calculate transmission energy (wireless transmission to RSU)
     * E_tx = P_tx × t_tx where t_tx = data_size / transmission_rate
     * 
     * @param data_size_bytes: task payload size
     * @param transmission_rate_bps: link data rate in bits/second
     * @return energy consumed in Joules
     */
    double calcTransmissionEnergy(uint32_t data_size_bytes,
                                 double transmission_rate_bps);
    
    /**
     * Calculate RSU computation energy (IEEE TMC 2018 cubic model)
     * E_rsu = κ_rsu × f_rsu² × cpu_cycles
     *
     * @param cpu_cycles        task CPU cycles
     * @param data_size_bytes   unused — kept for API compatibility
     * @param execution_time_sec unused — kept for API compatibility
     * @param freq_allocated_hz RSU operating frequency (Hz); defaults to FREQ_NOMINAL
     * @param freq_actual_hz    unused — kept for API compatibility
     * @return energy consumed in Joules
     */
    double calcRSUComputationEnergy(uint64_t cpu_cycles,
                                   uint32_t data_size_bytes,
                                   double execution_time_sec,
                                   double freq_allocated_hz = EnergyConstants::FREQ_NOMINAL,
                                   double freq_actual_hz = EnergyConstants::FREQ_NOMINAL);
    
    /**
     * Total offload energy (transmission + RSU computation)
     * E_offload = E_tx + E_rsu
     */
    double calcOffloadEnergy(uint32_t data_size_bytes,
                            double transmission_rate_bps,
                            uint64_t cpu_cycles,
                            double rsu_execution_time_sec);
    
    /**
     * Idle/listening energy (vehicle waiting or receiving)
     * E_idle = P_idle × waiting_time
     */
    double calcIdleEnergy(double waiting_time_sec,
                         double idle_power = EnergyConstants::IDLE_POWER);
    
    /**
     * Per-task-type energy estimation (convenience method)
     * Looks up task characteristics and estimates energy for both local and offloaded execution
     */
    struct TaskEnergyEstimate {
        TaskType type;
        double energy_local_joules;
        double energy_offload_joules;
        double energy_savings_ratio;           // energy_offload / energy_local
    };
    
    TaskEnergyEstimate estimateTaskEnergy(TaskType type,
                                         double transmission_rate_bps = 6e6); // 6 Mbps default
    
private:
    /**
     * IEEE TMC 2018 cubic energy formula:
     *   E = κ × f² × cpu_cycles
     *
     * Derivation:
     *   Dynamic CMOS power:  P = κ × f³
     *   Execution time:      t = cpu_cycles / f
     *   Energy:              E = P × t = κ × f² × cpu_cycles
     *
     * @param kappa      effective switching capacitance (J / Hz² / cycle)
     * @param freq_hz    operating CPU frequency (Hz)
     * @param cpu_cycles total CPU cycles for the task
     */
    double energyFormula(double kappa, double freq_hz, uint64_t cpu_cycles);
};

#endif /* ENERGYMODEL_H_ */
