package iovmini;

simple EdgeServer
{
    parameters:
        @class(EdgeServerApp);
        int numRsu = default(2);
        double edgeMips = default(1e10);
        double extraQueueDelay @unit(s) = default(0s);
    gates:
        input rsuIn[numRsu];
        output rsuOut[numRsu];
}
