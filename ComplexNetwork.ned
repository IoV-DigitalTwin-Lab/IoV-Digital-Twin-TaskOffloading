import org.car2x.veins.nodes.RSU;
import org.car2x.veins.nodes.TrafficLight;
import org.car2x.veins.nodes.Car;
import org.car2x.veins.base.connectionManager.ConnectionManager;
import org.car2x.veins.base.modules.BaseWorldUtility;
import org.car2x.veins.modules.mobility.traci.TraCIScenarioManager;
import org.car2x.veins.modules.obstacle.ObstacleControl;
import org.car2x.veins.modules.obstacle.VehicleObstacleControl;
import org.car2x.veins.modules.world.annotations.AnnotationManager;
import org.car2x.veins.visualizer.roads.RoadsCanvasVisualizer;
import inet.physicallayer.wireless.common.medium.RadioMedium;
import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.physicallayer.wireless.ieee80211.packetlevel.Ieee80211ScalarRadioMedium;
import inet.environment.common.PhysicalEnvironment;
import inet.physicallayer.wireless.common.obstacleloss.DielectricObstacleLoss;
import inet.visualizer.canvas.integrated.IntegratedCanvasVisualizer;

// Phase 4: Enhanced network with advanced environmental modeling and visualization
// NOTE: Does not extend Scenario to use regular TraCIScenarioManager (supports all SUMO versions)
network ComplexNetwork
{
    parameters:
        double playgroundSizeX @unit(m) = default(7000m);
        double playgroundSizeY @unit(m) = default(7000m);
        double playgroundSizeZ @unit(m) = default(50m);
        @display("bgb=7000,7000");
        
    submodules:
        // Veins core components (from Scenario.ned)
        obstacles: ObstacleControl {
            @display("p=240,50");
        }
        annotations: AnnotationManager {
            @display("p=260,50");
        }
        connectionManager: ConnectionManager {
            @display("p=150,0;i=abstract/multicast");
        }
        world: BaseWorldUtility {
            parameters:
                playgroundSizeX = parent.playgroundSizeX;
                playgroundSizeY = parent.playgroundSizeY;
                playgroundSizeZ = parent.playgroundSizeZ;
                @display("p=30,0;i=misc/globe");
        }
        // Use regular TraCIScenarioManager (supports all SUMO API versions)
        manager: TraCIScenarioManager {
            @display("p=512,128");
        }
        roadsCanvasVisualizer: RoadsCanvasVisualizer {
            @display("p=300,0");
        }
        node[0]: Car {
        }
        
        // INET components
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
        
        // Vehicle Obstacle Control for dynamic vehicle-to-vehicle shadowing
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
        
    connections allowunconnected:
}
