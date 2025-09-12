namespace iovmini;

message OffloadRequest extends cPacket {
    int srcModuleId;
    double cycles;
    int bytes;
    int rsuIndex;
    simtime_t created;
}

message OffloadResult extends cPacket {
    int srcModuleId;
    int rsuIndex;
    simtime_t created;
    simtime_t finishedAt;
}
