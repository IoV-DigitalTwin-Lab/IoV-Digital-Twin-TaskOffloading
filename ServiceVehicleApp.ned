import org.car2x.veins.modules.application.ieee80211p.DemoBaseApplLayer;

simple ServiceVehicleApp extends DemoBaseApplLayer
{
    parameters:
        @class(complex_network::ServiceVehicleApp);
        @display("i=block/app");
        
        // ==================== CPU & Compute Parameters ====================
        double totalCpuFreqHz = default(10e9);              // 10 GHz total CPU
        
        // ==================== Communication Parameters ====================
        double transmitPowerW = default(0.1);               // 100 mW = 0.1 W
        double availableBandwidthHz = default(10e6);        // 10 MHz
        double v2vCommRange @unit(m) = default(300m);       // V2V range
        
        // ==================== Energy Parameters ====================
        double energyIdle_mW = default(100);                // 100 mW idle
        double energyTx_mW = default(500);                  // 500 mW transmit
        double energyRx_mW = default(200);                  // 200 mW receive
        double energyCompute_mW_per_GHz = default(1000);    // 1W per GHz compute
        
        // ==================== Intervals ====================
        double heartbeatInterval @unit(s) = default(1s);
        double discoveryInterval @unit(s) = default(0.5s);
        double energyUpdateInterval @unit(s) = default(0.1s);
}
