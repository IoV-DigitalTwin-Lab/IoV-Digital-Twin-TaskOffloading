package iovmini;

import org.car2x.veins.modules.mobility.traci.TraCIScenarioManagerForker;

network Top
{
    parameters:
        int numRsu = default(2);
    submodules:
        manager: TraCIScenarioManagerForker {
            parameters:
                @display("p=100,60;i=block/network2");
        }
        edge: EdgeServer {
		    parameters:
		        numRsu = parent.numRsu;   // <-- use parent’s parameter
		        @display("p=700,60;i=device/server");
		}

        rsu[numRsu]: Rsu {
            parameters:
                @display("p=300+200*index,200");
                posX = 200m + 400m*index;
                posY = 200m;
        }
    connections allowunconnected:
	    for i=0..numRsu-1 {
	        rsu[i].edgeOut --> { delay = 2ms; datarate = 100Mbps; } --> edge.rsuIn[i];
	        edge.rsuOut[i] --> { delay = 2ms; datarate = 100Mbps; } --> rsu[i].edgeIn;
	    }
}
