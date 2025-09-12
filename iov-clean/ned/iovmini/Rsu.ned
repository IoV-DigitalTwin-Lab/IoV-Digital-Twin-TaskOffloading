package iovmini;

module Rsu
{
    parameters:
        double posX @unit(m);
        double posY @unit(m);
        @display("i=block/antenna");
    gates:
        input inFromCar;
        input edgeIn;
        output edgeOut;
    submodules:
        app: RsuApp;
    connections allowunconnected:
        inFromCar --> app.inFromCar;
        app.edgeOut --> edgeOut;
        app.edgeIn <-- edgeIn;
}
