<<<<<<< HEAD
import org.car2x.veins.base.modules.IBaseApplLayer;

simple SingleMessageVehicleApp like IBaseApplLayer
{
    parameters:
        @class(complex_network::SingleMessageVehicleApp);
        @display("i=block/app");

        // Mandatory parameters from IBaseApplLayer
        int headerLength @unit(bit) = default(80bit);
        int beaconLengthBits @unit(bit) = default(256bit);
        int dataLengthBits @unit(bit) = default(1024bit);
        bool avoidBeaconSynchronization = default(true);
        double maxOffset @unit(s) = default(0.005s);
        bool sendWSA = default(false);
        int currentOfferedServiceId = default(42);
        bool sendBeacons = default(false);
    	bool dataOnSch = default(false);
    	
    	
    	double beaconInterval @unit(s) = default(1s);
        int beaconUserPriority = default(7);
        int dataUserPriority = default(5);
        double wsaInterval @unit(s) = default(1s);  // WSA interval parameter


    gates:
        input lowerLayerIn;
        output lowerLayerOut;
        input lowerControlIn;
        output lowerControlOut;
}
=======
import org.car2x.veins.base.modules.IBaseApplLayer;

simple SingleMessageVehicleApp like IBaseApplLayer
{
    parameters:
        @class(complex_network::SingleMessageVehicleApp);
        @display("i=block/app");

        // Mandatory parameters from IBaseApplLayer
        int headerLength @unit(bit) = default(80bit);
        int beaconLengthBits @unit(bit) = default(256bit);
        int dataLengthBits @unit(bit) = default(1024bit);
        bool avoidBeaconSynchronization = default(true);
        double maxOffset @unit(s) = default(0.005s);
        bool sendWSA = default(false);
        int currentOfferedServiceId = default(42);
        bool sendBeacons = default(false);
    	bool dataOnSch = default(false);
    	
    	// Enhanced parameters for true unicast
    	double beaconInterval @unit(s) = default(1s);
        int beaconUserPriority = default(7);
        int dataUserPriority = default(5);
        double wsaInterval @unit(s) = default(1s);
        
        // Unicast specific parameters
        bool enableTrueUnicast = default(true);
        double unicastTimeout @unit(s) = default(5s);    // Timeout for unicast acknowledgment
        int maxRetransmissions = default(3);              // Max retries for failed unicast
        
        // Statistics collection
        @signal[unicastSent](type="long");
        @signal[unicastReceived](type="long");
        @signal[unicastLatency](type="double");
        @statistic[unicastSentStat](title="Unicast Messages Sent"; source="unicastSent"; record=count,vector);
        @statistic[unicastReceivedStat](title="Unicast Messages Received"; source="unicastReceived"; record=count,vector);
        @statistic[unicastLatencyStat](title="Unicast Message Latency"; source="unicastLatency"; record=histogram,vector; unit=s);

    gates:
        input lowerLayerIn;
        output lowerLayerOut;
        input lowerControlIn;
        output lowerControlOut;
}
>>>>>>> e5db28b (Added Vehicle Parameters)
