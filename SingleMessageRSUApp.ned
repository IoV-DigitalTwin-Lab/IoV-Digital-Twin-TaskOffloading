import org.car2x.veins.base.modules.IBaseApplLayer;

simple SingleMessageRSUApp like IBaseApplLayer
{
    parameters:
        @display("i=block/app");
        @class(complex_network::SingleMessageRSUApp);
        
        // Message parameters
        string targetVehicle = default(""); // Module path of the target vehicle
        int headerLength @unit(bit) = default(80bit);
        int dataLengthBits @unit(bit) = default(256bit);
        bool sendOnce = default(true);      // Send only once
        double sendTime @unit(s) = default(1s); // Time to send the message
        
        // Basic parameters (from MyRSUApp)
        bool sendBeacons = default(false);
        bool dataOnSch = default(false);
        double beaconInterval @unit(s) = default(1s);
        int beaconUserPriority = default(7);
        int dataUserPriority = default(5);
        
        // Additional required parameters for DemoBaseApplLayer
        int beaconLengthBits @unit(bit) = default(256bit);
        bool avoidBeaconSynchronization = default(true);
        double maxOffset @unit(s) = default(0.005s);
        bool sendWSA = default(false);
        int currentOfferedServiceId = default(42);
        double wsaInterval @unit(s) = default(1s);  // WSA interval parameter
        
    gates:
        input lowerLayerIn;
        output lowerLayerOut;
        input lowerControlIn;
        output lowerControlOut;
}