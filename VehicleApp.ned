import org.car2x.veins.modules.application.ieee80211p.DemoBaseApplLayer;

simple VehicleApp extends DemoBaseApplLayer
{
    parameters:
        @class(complex_network::VehicleApp);

        // ---- Vehicle (persistent) state ----
        double initFlocHz;                        // local CPU capacity (Hz)
        double initTxPower_mW;                    // radio TX power budget (mW)

        // ---- Heartbeat (continuous vehicle telemetry) ----
        double heartbeatIntervalS @unit(s) = default(0.5s);  // e.g., 2 Hz

        // ---- Task arrival (stochastic) ----
        double taskGenIntervalS @unit(s);         // mean cadence for task arrivals
        double paramJitterFactor = default(0.0);  // optional slow drift of Floc/TxP (0 = off)

        // ---- Task distribution config (not task state) ----
        double taskSizeB_min = default(1e5);
        double taskSizeB_max = default(1.5e5);
        double cpuPerByte_min = default(2e6);
        double cpuPerByte_max = default(4e6);
        double deadlineS_min @unit(s) = default(0.15s);
        double deadlineS_max @unit(s) = default(0.35s);
        int    divs_min = default(1);
        int    divs_max = default(4);

        @display("i=device/car;is=s;t=Vehicle App");
}
