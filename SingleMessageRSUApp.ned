import org.car2x.veins.base.modules.IBaseApplLayer;

simple SingleMessageRSUApp like IBaseApplLayer
{
    parameters:
        // Controls the icon used in the graphical network editor (like a block symbol for applications).
        @display("i=block/app");
        // Tells OMNeT++ which C++ class implements this NED module
        @class(complex_network::SingleMessageRSUApp);
        
        // Message parameters
        // This means no specific target vehicle is set by default, so the RSU may fall back to broadcast messages if no target is provided.
        string targetVehicle = default(""); // Module path of the target vehicle
        // Number of bits reserved for the protocol header.
        int headerLength @unit(bit) = default(80bit);
        // Number of bits for the payload (actual message content).
        int dataLengthBits @unit(bit) = default(256bit);
        // If true, the RSU will send the message only once.
        bool sendOnce = default(true);      // Send only once
        double sendTime @unit(s) = default(1s); // Time to send the message
        
        // Basic parameters (from MyRSUApp)
        bool sendBeacons = default(false);
        // Whether data messages are sent on a scheduled channel (SCH) or not.
        bool dataOnSch = default(false);
        // Time between successive beacon messages.
        double beaconInterval @unit(s) = default(1s);
        // Priority for beacon messages (0–7, higher is more urgent).
        int beaconUserPriority = default(7);
        // Priority for regular data messages.
        int dataUserPriority = default(5);
        
        // Additional required parameters for DemoBaseApplLayer
        // Size of the beacon payload in bits.
        int beaconLengthBits @unit(bit) = default(256bit);
        // If true, prevents all nodes from sending beacons exactly at the same time to avoid collisions.
        bool avoidBeaconSynchronization = default(true);
        // Maximum random offset applied to beacon scheduling.
        double maxOffset @unit(s) = default(0.005s);
        // Whether to send WSA (Wave Service Advertisement) messages.
        bool sendWSA = default(false);
        // Identifier for the service advertised by the RSU.
        int currentOfferedServiceId = default(42);
        // Time between consecutive WSA messages.
        double wsaInterval @unit(s) = default(1s);  // WSA interval parameter
        
    gates:
        // lowerLayerIn / lowerLayerOut → Connects to the MAC/physical layer.
        input lowerLayerIn;
        output lowerLayerOut;
        // lowerControlIn / lowerControlOut → Connects to control messages from the MAC layer.
        input lowerControlIn;
        output lowerControlOut;
}