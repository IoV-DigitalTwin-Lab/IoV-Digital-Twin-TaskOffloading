#include "rsu_http_poster.h"
#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>

RSUHttpPoster::RSUHttpPoster(const std::string &endpoint_) : endpoint(endpoint_) {}

RSUHttpPoster::~RSUHttpPoster() {
    stop();
}

void RSUHttpPoster::start() {
    if (running) return;
    running = true;
    th = std::thread(&RSUHttpPoster::worker, this);
}

void RSUHttpPoster::stop() {
    if (!running) return;
    running = false;
    cv.notify_all();
    if (th.joinable()) th.join();
}

void RSUHttpPoster::enqueue(const std::string &json) {
    {
        std::lock_guard<std::mutex> lk(m);
        q.push(json);
    }
    cv.notify_one();
    std::cout << "RSUHttpPoster: enqueued payload (len=" << json.size() << ")\n";
    try {
        std::ofstream lof("rsu_poster.log", std::ios::app);
        if (lof) {
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            lof << std::ctime(&t) << " ENQUEUE len=" << json.size() << " payload=" << json << "\n";
        }
    } catch(...) {}
}

void RSUHttpPoster::worker() {
    while (running) {
        std::string json;
        {
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [&]{ return !q.empty() || !running; });
            if (!running && q.empty()) break;
            if (!q.empty()) {
                json = q.front();
                q.pop();
            }
        }
        if (json.empty()) continue;

        CURL *curl = curl_easy_init();
        if (!curl) {
            std::cerr << "RSUHttpPoster: curl init failed\n";
            continue;
        }

        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)json.size());
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

        CURLcode res = curl_easy_perform(curl);
        
        try {
            std::ofstream lof("rsu_poster.log", std::ios::app);
            if (lof) {
                auto now = std::chrono::system_clock::now();
                std::time_t t = std::chrono::system_clock::to_time_t(now);
                if (res == CURLE_OK) {
                    lof << std::ctime(&t) << " POST OK to " << endpoint << "\n";
                } else {
                    lof << std::ctime(&t) << " POST FAILED: " << curl_easy_strerror(res) << "\n";
                }
            }
        } catch(...) {}

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
}
