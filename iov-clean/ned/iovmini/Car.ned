package iovmini;

import org.car2x.veins.modules.mobility.traci.TraCIMobility;

module Car
{
    parameters:
        @display("i=block/car");
    gates:
        input in;
    submodules:
        mobility: TraCIMobility;
        app: CarApp;
    connections allowunconnected:
        in --> app.in;
}
