simple TaskGenerationControl
{
    parameters:
        @class(complex_network::TaskGenerationControl);
        @display("i=block/control;p=600,300");
        
        // Quick task generation: format "vehicleId:TaskType" (e.g., "4:Perception")
        // Supports: Perception, Nav, Local, Voice, Fleet, Sensor
        // Example: 4:Perception → generates COOPERATIVE_PERCEPTION task for vehicle 4
        string generateTask @mutable = default("");
        
        // Parameter for manual task generation across all vehicles
        string globalManualTask @mutable = default("");
        
        // Targeted task generation: which vehicle ID to target (-1 = all vehicles)
        int targetVehicleId @mutable = default(-1);
        
        // Task type for targeted generation: "Perception", "Nav", "Local", "Voice", etc.
        string targetTaskType @mutable = default("");
}
