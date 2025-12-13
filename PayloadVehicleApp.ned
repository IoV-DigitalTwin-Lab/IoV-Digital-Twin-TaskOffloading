import org.car2x.veins.modules.application.ieee80211p.DemoBaseApplLayer;

simple PayloadVehicleApp extends DemoBaseApplLayer
{
    parameters:
        @class(complex_network::PayloadVehicleApp);
        @display("i=block/app");

        // ---- Vehicle (persistent) state ----
        double initFlocHz;                        // local CPU capacity (Hz)
        double initTxPower_mW;                    // radio TX power budget (mW)
        double initBattery_mAh;                   // battery capacity (mAh)
        double initMemory_MB;                     // memory capacity (MB)

        // ---- Heartbeat (continuous vehicle telemetry) ----
        double heartbeatIntervalS @unit(s) = default(0.5s);  // e.g., 2 Hz

        // ============================================================================
        // TASK PROCESSING SYSTEM PARAMETERS (Digital Twin Design)
        // ============================================================================
        
        // Task Generation Parameters
        double task_arrival_mean @unit(s) = default(2.0s);    // Mean inter-arrival time (Poisson)
        double task_size_min = default(1e5);                  // 100 KB minimum
        double task_size_max = default(1.5e6);                // 1.5 MB maximum
        double cpu_per_byte_min = default(2e3);               // 2K cycles/byte
        double cpu_per_byte_max = default(5e4);               // 50K cycles/byte
        double deadline_min = default(0.15);                  // Minimum deadline (seconds)
        double deadline_max = default(0.35);                  // Maximum deadline (seconds)
        
        // Resource Capacity
        double cpu_total = default(5e9);                      // Total CPU capacity (Hz)
        double cpu_reservation_ratio = default(0.3);          // Reserved for safety (20-40%)
        double memory_total = default(2e9);                   // Total memory (bytes)
        
        // Task Queue Parameters
        int max_queue_size = default(50);                     // Maximum pending tasks
        int max_concurrent_tasks = default(4);                // Maximum parallel execution
        double min_cpu_guarantee = default(0.1);              // Minimum CPU fraction per task
        
        // ============================================================================
        // TASK OFFLOADING SYSTEM PARAMETERS
        // ============================================================================
        
        // Offloading Control
        bool offloadingEnabled = default(true);               // Enable ML-based offloading
        double rsuDecisionTimeout @unit(s) = default(1.0s);   // Timeout for RSU decision response
        double offloadedTaskTimeout @unit(s) = default(5.0s); // Timeout for task execution result
        
        // Service Vehicle Capability (can process tasks for other vehicles)
        bool serviceVehicleEnabled = default(false);          // Enable service vehicle mode
        int maxConcurrentServiceTasks = default(3);           // Max tasks to process for others
        double serviceCpuReservation = default(0.3);          // CPU reserved for service (30%)
        double serviceMemoryReservation = default(512e6);     // Memory reserved for service (512 MB)
        
        // ============================================================================
        
        // ---- Legacy task parameters (kept for compatibility) ----
        double taskGenIntervalS @unit(s) = default(2.0s);     // mean cadence for task arrivals
        double paramJitterFactor = default(0.0);              // optional slow drift of Floc/TxP (0 = off)
        double taskSizeB_min = default(1e5);
        double taskSizeB_max = default(1.5e5);
        double cpuPerByte_min = default(2e6);
        double cpuPerByte_max = default(4e6);
        double deadlineS_min @unit(s) = default(0.15s);
        double deadlineS_max @unit(s) = default(0.35s);
        int    divs_min = default(1);
        int    divs_max = default(4);
}