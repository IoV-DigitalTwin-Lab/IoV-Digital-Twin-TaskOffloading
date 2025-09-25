import org.car2x.veins.base.modules.IBaseApplLayer;

simple PayloadVehicleApp like IBaseApplLayer
{
    parameters:
        @class(complex_network::PayloadVehicleApp);
        @display("i=block/app");

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
        double wsaInterval @unit(s) = default(1s);

    gates:
        input lowerLayerIn;
        output lowerLayerOut;
        input lowerControlIn;
        output lowerControlOut;
}