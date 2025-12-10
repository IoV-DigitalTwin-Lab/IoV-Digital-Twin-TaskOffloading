import org.car2x.veins.nodes.RSU;
import org.car2x.veins.nodes.Scenario;
import org.car2x.veins.nodes.TrafficLight;
import org.car2x.veins.modules.obstacle.VehicleObstacleControl;
import inet.physicallayer.wireless.common.medium.RadioMedium;
import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.physicallayer.wireless.ieee80211.packetlevel.Ieee80211ScalarRadioMedium;
import inet.environment.common.PhysicalEnvironment;
import inet.physicallayer.wireless.common.obstacleloss.DielectricObstacleLoss;
import inet.visualizer.canvas.integrated.IntegratedCanvasVisualizer;

// Phase 4: Enhanced network with advanced environmental modeling and visualization
network ComplexNetwork extends Scenario
{
    submodules:
        configurator: Ipv4NetworkConfigurator {
            @display("p=50,50;i=block/cogwheel");
        }
        
        // Enhanced Physical environment for advanced obstacle modeling
        physicalEnvironment: PhysicalEnvironment {
            @display("p=50,150;i=misc/town;is=l");
        }
        
        // INET Radio Medium for advanced propagation models  
        inetRadioMedium: Ieee80211ScalarRadioMedium {
            @display("p=580,50;i=misc/sun;is=l");
        }
        
        // Note: Veins Scenario already provides 'obstacles' submodule
        // It's configured via omnetpp.ini: *.obstacles.obstacles = xmldoc("obstacles.xml")
        
        // Vehicle Obstacle Control for dynamic vehicle-to-vehicle shadowing
        // IMPORTANT: Must be named "vehicleObstacles" (not vehicleObstacleControl)
        vehicleObstacles: VehicleObstacleControl {
            @display("p=50,250;i=misc/town2;is=l");
        }
        
        // Integrated Canvas Visualizer for obstacles and communication
        visualizer: IntegratedCanvasVisualizer {
            @display("p=50,350;i=block/app2;is=l");
        }
        
        rsu[3]: RSU {
            @display("p=150,140;i=device/antennatower;is=l;t=RSU");
        }

        tls[0]: TrafficLight {
            @display("i=misc/node2;is=s;t=Traffic Light");
        }
}
