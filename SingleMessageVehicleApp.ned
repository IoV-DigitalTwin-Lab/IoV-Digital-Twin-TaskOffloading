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
        
        string targetKind = default("vehicle"); // "vehicle" or "rsu"
        int    targetVehicleIndex = default(-1); // node[k] to unicast to (when targetKind="vehicle")
        int    targetRsuIndex     = default(-1); // rsu[k] to unicast to (when targetKind="rsu")
        double sendTime @unit(s)  = default(7s); // when to send the one-shot message


    gates:
        input lowerLayerIn;
        output lowerLayerOut;
        input lowerControlIn;
        output lowerControlOut;
}
