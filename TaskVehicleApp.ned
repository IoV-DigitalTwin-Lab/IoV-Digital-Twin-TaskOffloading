import org.car2x.veins.modules.application.ieee80211p.DemoBaseApplLayer;

simple TaskVehicleApp extends DemoBaseApplLayer
{
    parameters:
        @class(complex_network::TaskVehicleApp);
        @display("i=block/app");
        
        // ==================== CPU Parameters ====================
        double localCpuFreqHz = default(3e9);               // 3 GHz local CPU
        
        // ==================== Task Generation Parameters ====================
        double taskCycles_min = default(1e8);               // 100M cycles min
        double taskCycles_max = default(1e9);               // 1B cycles max
        double taskSize_min = default(1e5);                 // 100 KB min
        double taskSize_max = default(1e6);                 // 1 MB max
        double taskDelay_min @unit(s) = default(0.1s);      // 100ms min delay
        double taskDelay_max @unit(s) = default(1.0s);      // 1s max delay
        double taskGenerationInterval @unit(s) = default(2s); // Generate task every 2s
        
        // ==================== Communication Parameters ====================
        double transmitPowerW = default(0.1);               // 100 mW = 0.1 W
        double v2vCommRange @unit(m) = default(300m);       // V2V range
        double v2iCommRange @unit(m) = default(500m);       // V2I range (RSU)
        
        // ==================== Energy Parameters ====================
        double energyIdle_mW = default(50);                 // 50 mW idle
        double energyTx_mW = default(300);                  // 300 mW transmit
        double energyRx_mW = default(150);                  // 150 mW receive
        double energyCompute_mW_per_GHz = default(800);     // 800 mW per GHz compute
        
        // ==================== Intervals ====================
        double heartbeatInterval @unit(s) = default(1s);
        double discoveryInterval @unit(s) = default(0.5s);
        double energyUpdateInterval @unit(s) = default(0.1s);
}
