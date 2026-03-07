import org.car2x.veins.nodes.RSU;
import inet.linklayer.ethernet.EthernetInterface;
import inet.networklayer.common.InterfaceTable;

//
// EthernetRSU - RSU with wired Ethernet backhaul for inter-RSU communication
// Extends Veins RSU with INET Ethernet capabilities
//
module EthernetRSU extends RSU
{
    parameters:
        @networkNode;
        @display("i=device/antennatower;bgb=600,400");
        
    gates:
        inout ethg @labels(EthernetFrame);  // Ethernet gate for physical backhaul connection
        
    submodules:
        // Interface table for Ethernet MAC learning
        interfaceTable: InterfaceTable {
            @display("p=100,300;is=s");
        }
        
        // Ethernet interface for wired backhaul
        eth: EthernetInterface {
            interfaceTableModule = default("^.interfaceTable");
            @display("p=300,300;q=txQueue");
        }
        
    connections:
        // Physical Ethernet connection to backhaul switch
        eth.phys <--> ethg;
}
