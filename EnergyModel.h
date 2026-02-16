#ifndef ENERGYMODEL_H_
#define ENERGYMODEL_H_

#include <omnetpp.h>
#include "TaskProfile.h"

using namespace omnetpp;
using complex_network::TaskType;
using complex_network::TaskProfileDatabase;

/**
 * Energy Model for Task Execution
 * Based on RADiT paper energy consumption formulation
 * 
 * Key components:
 * 1. Local execution: E = κ_vehicle × (f_alloc - f_actual)² × d × c
 * 2. Transmission: E = P_tx × t_tx
 * 3. RSU computation: E = κ_rsu × (f_alloc - f_actual)² × d × c
 */

namespace EnergyConstants {
    // Switching capacitance coefficient (Joules per (Hz × bits × cycles))
    // Vehicle (Jetson Nano-class): κ_v ≈ 5e-27
    // RSU (High-performance server): κ_m ≈ 2e-27 (more efficient CPU)
    constexpr double KAPPA_VEHICLE = 5e-27;      // C² per V (vehicle)
    constexpr double KAPPA_RSU = 2e-27;          // C² per V (RSU server)
    
    // Transmit power (Watts)
    // 802.11p V2X: ~30dBm = 1W typical
    constexpr double TX_POWER = 1.0;             // Watts (1W for 802.11p)
    
    // Receive power (listening to channel)
    constexpr double RX_POWER = 0.8;             // Watts
    
    // Idle power (device on but not computing)
    constexpr double IDLE_POWER = 0.2;           // Watts
    
    // Frequency scaling (affects energy quadratically in RADiT model)
    // Typical: 1.0 GHz to 1.5 GHz for Jetson Nano
    constexpr double FREQ_NOMINAL = 1.0e9;       // Hz (1.0 GHz)
    constexpr double FREQ_MAX = 1.5e9;           // Hz (1.5 GHz, boost)
    constexpr double FREQ_MIN = 0.5e9;           // Hz (0.5 GHz, power-save)
}

/**
 * Encapsulates energy calculation logic for all 6 task types
 */
class EnergyCalculator {
public:
    EnergyCalculator();
    virtual ~EnergyCalculator();
    
    /**
     * Calculate energy for local task execution
     * E_loc = κ_vehicle × (f_alloc - f_actual)² × d × c
     * 
     * @param cpu_cycles: task CPU cycles (from TaskProfile)
     * @param data_size_bytes: task input data in bytes
     * @param execution_time_sec: actual execution time
     * @param freq_allocated_hz: CPU frequency allocated (typically FREQ_NOMINAL)
     * @param freq_actual_hz: actual CPU frequency (may differ due to thermal/power constraints)
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
     * Calculate RSU computation energy
     * E_rsu = κ_rsu × (f_alloc - f_actual)² × d × c
     * 
     * @param cpu_cycles: task CPU cycles
     * @param data_size_bytes: task input data
     * @param execution_time_sec: execution time at RSU
     * @param freq_allocated_hz: RSU CPU frequency
     * @param freq_actual_hz: actual RSU frequency
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
     * Raw energy formula from RADiT:
     * E = κ × (f_alloc - f_actual)² × data_size × cpu_cycles
     * 
     * Rearranged for computation energy and execution time
     */
    double energyFormula(double kappa,
                        double freq_allocated,
                        double freq_actual,
                        uint64_t cpu_cycles,
                        uint32_t data_size_bytes);
};

#endif /* ENERGYMODEL_H_ */
