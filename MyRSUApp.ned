
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
        bool terminalVerboseLogs = default(false);                  // print high-volume terminal logs when true
        
        // Redis digital twin integration
        bool useRedis = default(true);      // Enable Redis for real-time state sync
        string redisHost = default("127.0.0.1");  // Redis server host
        int redisPort = default(6379);       // Redis server port
        int redisDb = default(-1);           // Redis logical DB (default: RSU index)
        bool enablePostgres = default(true); // Enable PostgreSQL persistence for this RSU app instance

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
        int maxRedirectHops = default(2);                             // Max redirect hops for task offloading
        double rsuCoverageRadius_m @unit(m) = default(300m);          // Coverage radius for self-range ownership
        
        // Overload Detection Parameters
        double overloadQueueThreshold = default(0.8);                 // Queue load threshold (80% of max)
        double overloadCpuHeadroom = default(0.1);                    // Min CPU headroom to maintain (10%)
        int maxConcurrentProcessing = default(10);                    // Max concurrent tasks being processed

        // Queue Management
        int rsuQueueCapacity = default(5);                           // Max tasks in waiting queue before hard rejection

        // Offloading Decision Parameters
        bool mlModelEnabled = default(false);                         // Enable ML-based decision (DRL)
        string decisionStrategy = default("LOAD_AWARE");              // Strategy: LOAD_AWARE, ROUND_ROBIN, DRL
        string offloadMode = default("heuristic");                    // "heuristic" | "allOffload" | "allLocal"

        // Direct V2V RSSI model parameters. These are configured centrally in
        // omnetpp.ini and read once during initialization.
        double directLinkTxPower_mW;
        double directLinkCarrierFrequency_Hz;
        double directLinkAntennaHeight_m;
        double directLinkRssiThreshold_dBm;

        double offloadLinkBandwidthHz = default(10e6);                // Link bandwidth for SINR->rate model
        double offloadRateEfficiency = default(0.8);                  // PHY/MAC efficiency factor
        double offloadRateCapBps = default(-1);                       // <=0 means uncapped
        double offloadResponseRatio = default(0.2);                   // Response size ratio to request size


        // Secondary DT context export (motion + channel context only)
        bool secondaryLinkContextExport = default(false);
        string secondaryRunId = default("dt2_default");
        double secondaryLinkSampleInterval @unit(s) = default(0.1s);
        int secondaryRedisMaxSeriesLen = default(6000);
        string secondaryPredictorMode = default("placeholder");
        bool secondaryUseExternalPredictions = default(true);
        double secondaryPredictionHorizon @unit(s) = default(0.5s);
        double secondaryPredictionStep @unit(s) = default(0.02s);
        double secondaryCycleInterval @unit(s) = default(0.1s);
        double secondaryCandidateRadius @unit(m) = default(1500m);
        bool secondaryQPublishEnabled = default(true);
        double secondarySinrThresholdDb = default(3);
        double secondaryInterferenceMw = default(0.09);
        int secondaryQTTLSec = default(2);
        int secondaryQStreamMaxLen = default(200000);
        bool secondaryNakagamiEnabled = default(true);
        double secondaryNakagamiM = default(1.0);
        double secondaryNakagamiCellSize @unit(m) = default(5m);

    gates:
        input lowerLayerIn;
        output lowerLayerOut;
        input lowerControlIn;
        output lowerControlOut;
}