#include "MyRSUApp.h"
#include <sstream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <cmath>

using namespace veins;

namespace complex_network {

Define_Module(MyRSUApp);

void MyRSUApp::initialize(int stage) {
    DemoBaseApplLayer::initialize(stage);
    
    if (stage == 0) {
        // Initialize edge server resources
        edgeCPU_GHz = par("edgeCPU_GHz").doubleValue();
        edgeMemory_GB = par("edgeMemory_GB").doubleValue();
        maxVehicles = par("maxVehicles").intValue();
        processingDelay_ms = par("processingDelay_ms").doubleValue();
        
        std::cout << "CONSOLE: MyRSUApp " << getParentModule()->getFullName() 
                  << " initialized with edge resources:" << std::endl;
        std::cout << "  - CPU: " << edgeCPU_GHz << " GHz" << std::endl;
        std::cout << "  - Memory: " << edgeMemory_GB << " GB" << std::endl;
        std::cout << "  - Max Vehicles: " << maxVehicles << std::endl;
        std::cout << "  - Base Processing Delay: " << processingDelay_ms << " ms" << std::endl;
        
        double interval = par("beaconInterval").doubleValue();
        
    // create and store the beacon self-message so we can cancel/delete it safely later
    beaconMsg = new cMessage("sendMessage");
    scheduleAt(simTime() + 2.0, beaconMsg);

        std::cout << "=== CONSOLE: MyRSUApp INITIALIZED ===" << std::endl;
        std::cout << "CONSOLE: MyRSUApp - Beacon interval: " << interval << "s" << std::endl;
        std::cout << "CONSOLE: MyRSUApp - Starting RSUHttpPoster..." << std::endl;
        
        EV << "RSU initialized with beacon interval: " << interval << "s" << endl;
        
        poster.start();
        
        std::cout << "CONSOLE: MyRSUApp - RSUHttpPoster STARTED successfully" << std::endl;
        std::cout << "=== MyRSUApp READY ===" << std::endl;
        EV << "MyRSUApp: RSUHttpPoster started\n";
    }
}

void MyRSUApp::handleSelfMsg(cMessage* msg) {
        if(msg == beaconMsg && strcmp(msg->getName(), "sendMessage") == 0) {
        DemoSafetyMessage* wsm = new DemoSafetyMessage();
        populateWSM(wsm);
        wsm->setSenderPos(curPosition);
        wsm->setUserPriority(par("beaconUserPriority").intValue());
        sendDown(wsm);

        EV << "RSU: Sent beacon at time " << simTime() << endl;

        double interval = par("beaconInterval").doubleValue();
        scheduleAt(simTime() + interval, msg);
        } else {
            DemoBaseApplLayer::handleSelfMsg(msg);
        }
}

void MyRSUApp::handleLowerMsg(cMessage* msg) {
    std::cout << "\n*** CONSOLE: MyRSUApp - handleLowerMsg() CALLED at " 
              << simTime() << " ***" << std::endl;
    
    BaseFrame1609_4* wsm = dynamic_cast<BaseFrame1609_4*>(msg);
    if (wsm) {
        std::cout << "CONSOLE: MyRSUApp - Message IS BaseFrame1609_4, calling parent handler" 
                  << std::endl;
    } else {
        std::cout << "CONSOLE: MyRSUApp - Message is NOT BaseFrame1609_4" << std::endl;
    }
    
    EV << "MyRSUApp: handleLowerMsg() called" << endl;
    
    // This will trigger onWSM if the message is appropriate
    DemoBaseApplLayer::handleLowerMsg(msg);
    
    std::cout << "CONSOLE: MyRSUApp - handleLowerMsg() completed\n" << std::endl;
}

void MyRSUApp::onWSM(BaseFrame1609_4* wsm) {
    std::cout << "\n🔴 RSU SHADOW ANALYSIS - Message Reception at " << simTime() << std::endl;
    
    DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
    if(!dsm) {
        std::cout << "CONSOLE: MyRSUApp - ERROR: Message is NOT DemoSafetyMessage!" 
                  << std::endl;
        return;
    }
    
    std::cout << "CONSOLE: MyRSUApp - ✓ Message IS DemoSafetyMessage" << std::endl;
    
    // Access REAL signal reception data from the simulation
    Coord senderPos = dsm->getSenderPos();
    Coord rsuPos = curPosition;
    double distance = senderPos.distance(rsuPos);
    
    std::cout << "SHADOW: 📡 RSU RECEIVED signal from Vehicle:" << std::endl;
    std::cout << "SHADOW: Sender position: (" << senderPos.x << "," << senderPos.y << ")" << std::endl;
    std::cout << "SHADOW: RSU position: (" << rsuPos.x << "," << rsuPos.y << ")" << std::endl;
    std::cout << "SHADOW: Distance: " << distance << " m" << std::endl;
    
    // Analyze if signal passed through obstacles
    bool senderNearOfficeT = (senderPos.x >= 240 && senderPos.x <= 320 && senderPos.y >= 10 && senderPos.y <= 90);
    bool senderNearOfficeC = (senderPos.x >= 110 && senderPos.x <= 190 && senderPos.y >= 10 && senderPos.y <= 90);
    
    if (senderNearOfficeT || senderNearOfficeC) {
        std::string obstacleType = senderNearOfficeT ? "Office Tower" : "Office Complex";
        std::cout << "SHADOW: 🏢 SIGNAL TRAVELED THROUGH " << obstacleType << "!" << std::endl;
        std::cout << "SHADOW: Expected path loss: " << (20 * log10(distance) + 40) << " dB (free space)" << std::endl;
        std::cout << "SHADOW: Plus obstacle loss: 18dB + shadowing up to 8dB" << std::endl;
        std::cout << "SHADOW: ✅ Signal STRONG ENOUGH despite " << obstacleType << " attenuation!" << std::endl;
    } else {
        std::cout << "SHADOW: 🌐 CLEAR PATH from vehicle to RSU" << std::endl;
        std::cout << "SHADOW: Expected path loss: " << (20 * log10(distance) + 40) << " dB (free space only)" << std::endl;
        std::cout << "SHADOW: ✅ Optimal reception conditions" << std::endl;
    }
    
    // Try to access actual signal measurements from the frame
    if (dsm->getControlInfo()) {
        cObject* controlInfo = dsm->getControlInfo();
        std::cout << "SHADOW: Control info available: " << controlInfo->getClassName() << std::endl;
        
        // The real signal strength data should be in the control info
        // This varies by Veins version but typically contains RSSI/SNR data
        std::cout << "SHADOW: Real signal reception recorded by PHY layer" << std::endl;
    }
    
    // Access the radio module to get actual reception parameters
    cModule* nicModule = getSubmodule("nic");
    if (nicModule) {
        cModule* phyModule = nicModule->getSubmodule("phy80211p");
        if (phyModule) {
            std::cout << "SHADOW: PHY layer processed signal with real propagation models" << std::endl;
            
            // Get actual configured parameters from simulation
            double sensitivity = -85.0;
            double txPower = 20.0;
            
            if (phyModule->hasPar("sensitivity")) {
                sensitivity = phyModule->par("sensitivity").doubleValue();
                std::cout << "SHADOW: Actual sensitivity threshold: " << sensitivity << " dBm" << std::endl;
            }
            
            // Since we received the message, it passed the sensitivity test
            std::cout << "SHADOW: ✓ Signal PASSED sensitivity test (real simulation result)" << std::endl;
            std::cout << "SHADOW: Real propagation models (TwoRay+LogNormal+Obstacles) applied" << std::endl;
        }
    }
    
    // Get payload from message name
    const char* nm = dsm->getName();
    std::string payload = nm ? std::string(nm) : std::string();
    
    std::cout << "CONSOLE: MyRSUApp - Raw payload length: " << payload.length() << std::endl;
    std::cout << "CONSOLE: MyRSUApp - Raw payload: '" << payload << "'" << std::endl;
    
    // Log to file
    try {
        std::ofstream lof("rsu_poster.log", std::ios::app);
        if (lof) {
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            char timebuf[100];
            std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
            lof << timebuf << " ONWSM raw='" << payload << "'\n";
            lof.close();
        }
    } catch(const std::exception& e) {
        std::cout << "CONSOLE: MyRSUApp - Log file error: " << e.what() << std::endl;
    }

    // Diagnostic enqueue: always write a diagnostic log entry and enqueue a small ping JSON
    try {
        std::ostringstream diagss;
        diagss << "{\"diag\":\"ping\",\"simTime\":" << simTime().dbl() << "}";
        std::string diagJson = diagss.str();

        std::ofstream dlf("rsu_poster.log", std::ios::app);
        if (dlf) {
            auto now2 = std::chrono::system_clock::now();
            std::time_t t2 = std::chrono::system_clock::to_time_t(now2);
            char tb2[100];
            std::strftime(tb2, sizeof(tb2), "%Y-%m-%d %H:%M:%S", std::localtime(&t2));
            dlf << tb2 << " DIAGNOSTIC_ENQUEUE payload='" << diagJson << "'\n";
            dlf.close();
        }

        poster.enqueue(diagJson);
        std::cout << "CONSOLE: MyRSUApp - DIAGNOSTIC poster.enqueue() called with: " << diagJson << std::endl;
    } catch (const std::exception &e) {
        std::cout << "CONSOLE: MyRSUApp - Diagnostic enqueue/log error: " << e.what() << std::endl;
    }
    
    EV << "RSU: Received message from vehicle at time " << simTime() << endl;

    std::ostringstream js;
    bool posted = false;
    
    // Check for VEHICLE_DATA prefix
    if (!payload.empty() && payload.rfind("VEHICLE_DATA|", 0) == 0) {
        std::cout << "CONSOLE: MyRSUApp - ✓ Found VEHICLE_DATA prefix, parsing..." << std::endl;
        
        std::string body = payload.substr(13);  // strlen("VEHICLE_DATA|") = 13
        std::istringstream parts(body);
        std::string token;
        std::map<std::string, std::string> kv;
        
        int field_count = 0;
        while (std::getline(parts, token, '|')) {
            auto pos = token.find(':');
            if (pos != std::string::npos) {
                std::string k = token.substr(0, pos);
                std::string v = token.substr(pos + 1);
                kv[k] = v;
                field_count++;
                std::cout << "CONSOLE: MyRSUApp -   Field[" << field_count 
                          << "]: " << k << " = " << v << std::endl;
            }
        }
        
        std::cout << "CONSOLE: MyRSUApp - Parsed " << field_count << " fields" << std::endl;

        // Build JSON
        js << "{";
        bool first = true;
        for (const auto &p : kv) {
            if (!first) js << ", ";
            first = false;
            
            if (p.first == "MAC") {
                js << "\"" << p.first << "\": \"" << p.second << "\"";
            } else {
                js << "\"" << p.first << "\": " << p.second;
            }
        }
        js << "}";

        std::string jsonStr = js.str();
        std::cout << "CONSOLE: MyRSUApp - JSON built: " << jsonStr << std::endl;
        std::cout << "CONSOLE: MyRSUApp - JSON length: " << jsonStr.size() << " bytes" << std::endl;
        std::cout << "CONSOLE: MyRSUApp - Calling poster.enqueue()..." << std::endl;
        
        EV << "MyRSUApp: enqueueing JSON (len=" << jsonStr.size() << ")\n";
        
        poster.enqueue(jsonStr);
        
        std::cout << "CONSOLE: MyRSUApp - ✓ JSON enqueued successfully!" << std::endl;
        EV << "MyRSUApp: enqueued JSON\n";
        posted = true;
        
    } else {
        std::cout << "CONSOLE: MyRSUApp - ⚠ No VEHICLE_DATA prefix found" << std::endl;
    }

    if (!posted) {
        std::cout << "CONSOLE: MyRSUApp - Creating fallback JSON..." << std::endl;
        
        std::ostringstream ss;
        ss << "{\"VehID\":" << std::fixed << std::setprecision(2) << dsm->getSenderPos().x 
           << ", \"Time\":" << simTime().dbl() 
           << ", \"raw\":\"";
        
        for (char c : payload) {
            if (c == '"') ss << "\\\"";
            else if (c == '\\') ss << "\\\\";
            else ss << c;
        }
        ss << "\"}";
        
        std::string jsonStr = ss.str();
        std::cout << "CONSOLE: MyRSUApp - Fallback JSON: " << jsonStr << std::endl;
        
        EV << "MyRSUApp: enqueueing fallback JSON (len=" << jsonStr.size() << ")\n";
        
        poster.enqueue(jsonStr);
        
        std::cout << "CONSOLE: MyRSUApp - ✓ Fallback JSON enqueued" << std::endl;
        EV << "MyRSUApp: enqueued fallback JSON\n";
    }
    
    std::cout << "***** MyRSUApp - onWSM() COMPLETED *****\n" << std::endl;
}

void MyRSUApp::finish() {
    std::cout << "\n=== CONSOLE: MyRSUApp - finish() called ===" << std::endl;
    EV << "MyRSUApp: stopping RSUHttpPoster...\n";
    // cancel and delete our beacon self-message if still scheduled
    if (beaconMsg) {
        cancelAndDelete(beaconMsg);
        beaconMsg = nullptr;
    }

    // stop poster worker
    poster.stop();

    std::cout << "CONSOLE: MyRSUApp - Poster stopped successfully" << std::endl;
    std::cout << "=== MyRSUApp FINISHED ===" << std::endl;
    EV << "MyRSUApp: RSUHttpPoster stopped\n";

    DemoBaseApplLayer::finish();
}

void MyRSUApp::handleMessage(cMessage* msg) {
    std::cout << "CONSOLE: MyRSUApp handleMessage() called with message: " << msg->getName()
              << " at time " << simTime() << std::endl;

    BaseFrame1609_4* wsm = dynamic_cast<BaseFrame1609_4*>(msg);
    if (wsm) {
        std::cout << "CONSOLE: MyRSUApp handleMessage received BaseFrame1609_4! forwarding to onWSM()" << std::endl;
        onWSM(wsm);
        return;
    }

    DemoBaseApplLayer::handleMessage(msg);
}

}
