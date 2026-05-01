simple TaskGenerationControl
{
    parameters:
        @class(complex_network::TaskGenerationControl);
        @display("i=block/control;p=600,300");

        // Qtenv task-control panel:
        // Click the canvas "Task Control" figure to open an active Qtenv dialog.
        // These parameters are also usable from the inspector or omnetpp.ini.
        string selectedVehicleId @mutable = default("0");
        string selectedTaskType @mutable @enum(
            "COOPERATIVE_PERCEPTION",
            "ROUTE_OPTIMIZATION",
            "FLEET_TRAFFIC_FORECAST",
            "VOICE_COMMAND_PROCESSING",
            "SENSOR_HEALTH_CHECK"
        ) = default("COOPERATIVE_PERCEPTION");
        bool triggerSelectedTask @mutable = default(false);
        
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
