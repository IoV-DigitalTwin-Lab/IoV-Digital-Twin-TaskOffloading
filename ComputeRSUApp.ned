import org.car2x.veins.base.modules.IBaseApplLayer;

simple ComputeRSUApp like IBaseApplLayer
{
    parameters:
        @class(complex_network::ComputeRSUApp);
        @display("i=block/app");
        
        // ==================== CPU & Compute Parameters ====================
        double totalCpuFreqHz = default(20e9);              // 20 GHz total CPU (2x vehicle)
        
        // ==================== Communication Parameters ====================
        double transmitPowerW = default(1.0);               // 1W transmit power (10x vehicle)
        double totalBandwidthHz = default(20e6);            // 20 MHz total bandwidth
        
        // ==================== Energy Parameters ====================
        double energyIdle_mW = default(500);                // 500 mW idle (RSUs have more power)
        double energyTx_mW = default(2000);                 // 2W transmit
        double energyRx_mW = default(800);                  // 800 mW receive
        double energyCompute_mW_per_GHz = default(1500);    // 1.5W per GHz compute
        
        // ==================== Intervals ====================
        double heartbeatInterval @unit(s) = default(1s);
        double energyUpdateInterval @unit(s) = default(0.1s);
        
        // ==================== Legacy Parameters (from DemoBaseApplLayer) ====================
        int headerLength @unit(bit) = default(80bit);
        bool sendBeacons = default(false);
        bool dataOnSch = default(false);
        double beaconInterval @unit(s) = default(1s);
        int beaconUserPriority = default(7);
        int dataUserPriority = default(5);
        int beaconLengthBits @unit(bit) = default(256bit);
        int dataLengthBits @unit(bit) = default(1024bit);
        bool avoidBeaconSynchronization = default(true);
        double maxOffset @unit(s) = default(0.005s);
        bool sendWSA = default(false);
        int currentOfferedServiceId = default(42);
        double wsaInterval @unit(s) = default(1s);
        
    gates:
        input lowerLayerIn;
        output lowerLayerOut;
        input lowerControlIn;
        output lowerControlOut;
}
