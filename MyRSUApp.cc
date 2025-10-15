#include "MyRSUApp.h"
#include <sstream>
#include <fstream>
#include <chrono>
#include <ctime>
using namespace veins;

namespace complex_network {

Define_Module(MyRSUApp);

void MyRSUApp::initialize(int stage) {
    // Calls the parent class initialization to set up the basic functionality of the application layer.
    BaseApplLayer::initialize(stage);  // <-- changed here
    // Most initialization like setting timers is done in stage 0.
    if(stage == 0) {
        // Reads the RSU beacon interval from NED parameters.
        double interval = par("beaconInterval").doubleValue();
        // Creates a cMessage named "sendMessage" to trigger sending later.
        cMessage* sendMsg = new cMessage("sendMessage");
        // Schedules the self-message at current simulation time + 2 seconds.
        scheduleAt(simTime() + 2.0, sendMsg);

        EV << "RSU initialized with beacon interval: " << interval << "s" << endl;
        // start the HTTP poster background worker
        EV << "MyRSUApp: starting RSUHttpPoster...\n";
        poster.start();
        EV << "MyRSUApp: RSUHttpPoster started\n";
    }
}

void MyRSUApp::handleSelfMsg(cMessage* msg) {
    if(strcmp(msg->getName(), "sendMessage") == 0) {
        // Create and send your message
        DemoSafetyMessage* wsm = new DemoSafetyMessage();
        populateWSM(wsm);
        wsm->setSenderPos(curPosition);
        wsm->setUserPriority(par("beaconUserPriority").intValue());
        sendDown(wsm);

        EV << "RSU: Sent message at time " << simTime() << endl;

        double interval = par("beaconInterval").doubleValue();
        scheduleAt(simTime() + interval, msg);
    } else {
        BaseApplLayer::handleSelfMsg(msg);  // <-- changed here
    }
}

void MyRSUApp::onWSM(BaseFrame1609_4* wsm) {
    DemoSafetyMessage* dsm = dynamic_cast<DemoSafetyMessage*>(wsm);
    if(dsm) {
        // Log receipt immediately for visibility
        std::cout << "CONSOLE: MyRSUApp - onWSM() called at " << simTime() << "\n";
        // persistent log: record raw message name/time to rsu_poster.log
        try {
            std::ofstream lof("rsu_poster.log", std::ios::app);
            if (lof) {
                auto now = std::chrono::system_clock::now();
                std::time_t t = std::chrono::system_clock::to_time_t(now);
                const char* nm = dsm->getName();
                std::string payload = nm ? std::string(nm) : std::string();
                lof << std::ctime(&t) << " ONWSM raw='" << payload << "'\n";
            }
        } catch(...) {}
        EV << "RSU: Received message from vehicle at time " << simTime() << endl;

        // Attempt to read payload from the message name (PayloadVehicleApp uses setName())
        const char* nm = dsm->getName();
        std::string payload = nm ? std::string(nm) : std::string();
        std::cout << "CONSOLE: MyRSUApp - raw payload='" << payload << "'\n";

        // If payload has the expected prefix, parse key:value pairs separated by '|'
        std::ostringstream js;
        bool posted = false;
        if (!payload.empty() && payload.rfind("VEHICLE_DATA|", 0) == 0) {
            // Skip the prefix
            std::string body = payload.substr(strlen("VEHICLE_DATA|"));
            std::istringstream parts(body);
            std::string token;
            std::map<std::string,std::string> kv;
            while (std::getline(parts, token, '|')) {
                auto pos = token.find(':');
                if (pos != std::string::npos) {
                    std::string k = token.substr(0,pos);
                    std::string v = token.substr(pos+1);
                    kv[k] = v;
                }
            }

            // Build JSON object with expected fields (strings quoted where appropriate)
            js << "{";
            bool first = true;
            for (auto &p : kv) {
                if (!first) js << ", ";
                first = false;
                // Determine if value should be quoted (MAC may be string)
                if (p.first == "MAC") {
                    js << "\"" << p.first << "\": \"" << p.second << "\"";
                } else {
                    // numeric fields: try to keep as-is
                    js << "\"" << p.first << "\": " << p.second;
                }
            }
            js << "}";

            std::string jsonStr = js.str();
            std::cout << "CONSOLE: MyRSUApp - posting JSON: " << jsonStr << "\n";
            EV << "MyRSUApp: enqueueing JSON (len=" << jsonStr.size() << ")\n";
            poster.enqueue(jsonStr);
            EV << "MyRSUApp: enqueued JSON\n";
            posted = true;
        }

        // Fallback: if payload didn't contain VEHICLE_DATA, post minimal info
        if (!posted) {
            std::ostringstream ss;
            ss << "{\"VehID\":" << dsm->getSenderPos().x << ", \"Time\":" << simTime().dbl() << ", \"raw\":\"";
            // escape quotes in payload
            for (char c : payload) {
                if (c == '"') ss << '\\' << '"';
                else ss << c;
            }
            ss << "\"}";
            std::string jsonStr = ss.str();
            std::cout << "CONSOLE: MyRSUApp - posting fallback JSON: " << jsonStr << "\n";
            EV << "MyRSUApp: enqueueing fallback JSON (len=" << jsonStr.size() << ")\n";
            poster.enqueue(jsonStr);
            EV << "MyRSUApp: enqueued fallback JSON\n";
        }
    }
}

void MyRSUApp::finish() {
    // stop poster worker
    EV << "MyRSUApp: stopping RSUHttpPoster...\n";
    poster.stop();
    EV << "MyRSUApp: RSUHttpPoster stopped\n";
    BaseApplLayer::finish();
}

} // namespace complex_network
