package iovmini;

simple CarApp
{
    parameters:
        @class(CarApp);
        double taskInterval @unit(s) = default(2s);
        double taskCycles = default(5e8);
        int taskBytes = default(100000);
        double localMips = default(2e9);
        double uplinkBps = default(6e6);
    gates:
        input in;
}
