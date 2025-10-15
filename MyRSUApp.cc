#include "MyRSUApp.h"
#include <sstream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>

using namespace veins;

namespace complex_network {

Define_Module(MyRSUApp);

void MyRSUApp::initialize(int stage) {
    DemoBaseApplLayer::initialize(stage);
    
    if(stage == 0) {
        double interval = par("beaconInterval").doubleValue();
        
        cMessage* sendMsg = new cMessage("sendMessage");
        scheduleAt(simTime() + 2.0, sendMsg);

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
    if(strcmp(msg->getName(), "sendMessage") == 0) {
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
    std::cout << "\n***** CONSOLE: MyRSUApp - onWSM() TRIGGERED at " 
              << simTime() << " *****" << std::endl;
    
    DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
    if(!dsm) {
        std::cout << "CONSOLE: MyRSUApp - ERROR: Message is NOT DemoSafetyMessage!" 
                  << std::endl;
        return;
    }
    
    std::cout << "CONSOLE: MyRSUApp - ✓ Message IS DemoSafetyMessage" << std::endl;
    
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

    // Fallback
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
    
    poster.stop();
    
    std::cout << "CONSOLE: MyRSUApp - Poster stopped successfully" << std::endl;
    std::cout << "=== MyRSUApp FINISHED ===" << std::endl;
    EV << "MyRSUApp: RSUHttpPoster stopped\n";
    
    DemoBaseApplLayer::finish();
}

} // namespace complex_network