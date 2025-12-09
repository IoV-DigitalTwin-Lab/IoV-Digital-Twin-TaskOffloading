#pragma once

#include <string>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

class RSUHttpPoster {
public:
    RSUHttpPoster(const std::string &endpoint = "http://127.0.0.1:8000/ingest");
    ~RSUHttpPoster();

    // Enqueue JSON string to send
    void enqueue(const std::string &json);

    // start/stop worker
    void start();
    void stop();
    // send a single startup ping (synchronous) to the endpoint
    void sendStartupPing();

private:
    void worker();

    std::string endpoint;
    std::thread th;
    std::queue<std::string> q;
    std::mutex m;
    std::condition_variable cv;
    std::atomic<bool> running{false};
};
