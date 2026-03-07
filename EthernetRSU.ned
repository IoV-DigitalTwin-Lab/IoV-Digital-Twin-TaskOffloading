import org.car2x.veins.nodes.RSU;
import inet.node.ethernet.EthernetInterface;
import inet.networklayer.ipv4.Ipv4NetworkLayer;
import inet.networklayer.common.InterfaceTable;
import inet.transportlayer.udp.Udp;

//
// EthernetRSU - RSU with wired Ethernet backhaul for inter-RSU communication
// Extends Veins RSU with INET Ethernet capabilities
//
module EthernetRSU extends RSU
{
    parameters:
        @display("i=device/antennatower;bgb=600,400");
        
        // Number of Ethernet interfaces (for mesh topology)
        int numEthInterfaces = default(2);
        
    gates:
        inout ethg[numEthInterfaces] @labels(EthernetFrame);  // Ethernet gates for wired backhaul
        
    submodules:
        // Interface table required for IP routing
        interfaceTable: InterfaceTable {
            @display("p=100,300;is=s");
        }
        
        // Network layer for IP addressing
        ipv4: Ipv4NetworkLayer {
            @display("p=200,250");
        }
        
        // UDP for lightweight application-level messaging
        udp: Udp {
            @display("p=200,175");
        }
        
        // Ethernet interfaces for wired backhaul
        eth[numEthInterfaces]: EthernetInterface {
            @display("p=300,300,row,100;q=txQueue");
        }
        
    connections:
        // Connect application layer to UDP
        appl.lowerControlOut --> udp.appIn++ if exists(appl);
        appl.lowerControlIn <-- udp.appOut++ if exists(appl);
        
        // Connect UDP to network layer
        udp.ipOut --> ipv4.transportIn++;
        udp.ipIn <-- ipv4.transportOut++;
        
        // Connect network layer to Ethernet interfaces
        for i=0..numEthInterfaces-1 {
            ipv4.ifOut++ --> eth[i].upperLayerIn;
            ipv4.ifIn++ <-- eth[i].upperLayerOut;
            eth[i].phys <--> ethg[i];
        }
}
