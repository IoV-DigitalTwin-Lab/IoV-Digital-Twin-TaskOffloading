
import org.car2x.veins.base.modules.IBaseApplLayer;

//
// MyRSUApp - Custom RSU Application
//
simple MyRSUApp like IBaseApplLayer
{
    parameters:
        @display("i=block/app");
        @class(complex_network::MyRSUApp);
        
        // Basic parameters
        int headerLength @unit(bit) = default(80bit);
        bool sendBeacons = default(false);
        bool dataOnSch = default(false);
        double beaconInterval @unit(s) = default(1s);
        int beaconUserPriority = default(7);
        int dataUserPriority = default(5);
        
        // Additional required parameters for DemoBaseApplLayer
        int beaconLengthBits @unit(bit) = default(256bit);
        int dataLengthBits @unit(bit) = default(1024bit);
        bool avoidBeaconSynchronization = default(true);
        double maxOffset @unit(s) = default(0.005s);
        bool sendWSA = default(false);
    double wsaInterval @unit(s) = default(1s);
        int currentOfferedServiceId = default(42);
        
        // Redis digital twin integration
        bool useRedis = default(true);      // Enable Redis for real-time state sync
        string redisHost = default("127.0.0.1");  // Redis server host
        int redisPort = default(6379);       // Redis server port

        // Edge server resource parameters
        double edgeCPU_GHz;              // CPU capacity in GHz
        double edgeMemory_GB;            // Memory capacity in GB  
        int maxVehicles;                 // Max concurrent vehicles
        int rsuMaxConcurrent = default(6); // Hard cap on simultaneous tasks
        double processingDelay_ms;       // Base processing delay in ms
        
        // RSU-to-RSU Communication (Neighbor Discovery)
        double rsuStatusBroadcastInterval @unit(s) = default(0.5s);  // Interval for periodic status broadcast
        double neighborStateTtl @unit(s) = default(1.5s);            // Time-to-live for neighbor state cache
        int maxVehiclesInBroadcast = default(20);                     // Max vehicle IDs to include in broadcast
        
        // Overload Detection Parameters
        double overloadQueueThreshold = default(0.8);                 // Queue load threshold (80% of max)
        double overloadCpuHeadroom = default(0.1);                    // Min CPU headroom to maintain (10%)
        int maxConcurrentProcessing = default(10);                    // Max concurrent tasks being processed
        
        // Offloading Decision Parameters
        bool mlModelEnabled = default(false);                         // Enable ML-based decision (DRL)
        string decisionStrategy = default("LOAD_AWARE");              // Strategy: LOAD_AWARE, ROUND_ROBIN, DRL
    gates:
        input lowerLayerIn;
        output lowerLayerOut;
        input lowerControlIn;
        output lowerControlOut;
}