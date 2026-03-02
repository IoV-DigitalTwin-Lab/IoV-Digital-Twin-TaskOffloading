
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
    
    gates:
        input lowerLayerIn;
        output lowerLayerOut;
        input lowerControlIn;
        output lowerControlOut;
}