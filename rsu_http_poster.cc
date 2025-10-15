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
    // log enqueue (visible in stdout)
    std::cout << "RSUHttpPoster: enqueued payload (len=" << json.size() << ")\n";
    // persistent log
    try {
        std::ofstream lof("rsu_poster.log", std::ios::app);
        if (lof) {
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            lof << std::ctime(&t) << " ENQUEUE len=" << json.size() << " payload=" << json << "\n";
        }
    } catch(...) {}
    cv.notify_one();
}

void RSUHttpPoster::worker() {
    // Initialize libcurl for this thread
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
        std::cerr << "RSUHttpPoster: curl_global_init failed" << std::endl;
        return;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        std::cerr << "RSUHttpPoster: curl init failed" << std::endl;
        std::cout << "RSUHttpPoster: curl init failed\n";
        curl_global_cleanup();
        return;
    }

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
    // Don't use signals in multi-threaded environment
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

    std::cout << "RSUHttpPoster: worker started, endpoint=" << endpoint << std::endl;
    try {
        std::ofstream lof("rsu_poster.log", std::ios::app);
        if (lof) {
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            lof << std::ctime(&t) << " WORKER_STARTED endpoint=" << endpoint << "\n";
        }
    } catch(...) {}

    while (running) {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&]{ return !q.empty() || !running; });
        if (!running && q.empty()) break;
        std::string json = std::move(q.front());
        q.pop();
        lk.unlock();

    // set payload for this request
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)json.size());
    // log the payload we are about to send
    std::cout << "RSUHttpPoster: sending JSON payload: " << json << std::endl;
    try {
        std::ofstream lof("rsu_poster.log", std::ios::app);
        if (lof) {
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            lof << std::ctime(&t) << " SEND payload=" << json << "\n";
        }
    } catch(...) {}
    // perform request
    CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::string err = curl_easy_strerror(res);
            std::cerr << "RSUHttpPoster: POST failed: " << err << std::endl;
            std::cout << "RSUHttpPoster: POST failed: " << err << "\n";
            try {
                std::ofstream lof("rsu_poster.log", std::ios::app);
                if (lof) {
                    auto now = std::chrono::system_clock::now();
                    std::time_t t = std::chrono::system_clock::to_time_t(now);
                    lof << std::ctime(&t) << " RESULT ERROR=" << err << "\n";
                }
            } catch(...) {}
            // Optionally: push back or write to local retry file
        } else {
            long response_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            std::cout << "RSUHttpPoster: POST succeeded, HTTP response code: " << response_code << std::endl;
            try {
                std::ofstream lof("rsu_poster.log", std::ios::app);
                if (lof) {
                    auto now = std::chrono::system_clock::now();
                    std::time_t t = std::chrono::system_clock::to_time_t(now);
                    lof << std::ctime(&t) << " RESULT OK code=" << response_code << "\n";
                }
            } catch(...) {}
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    std::cout << "RSUHttpPoster: worker exiting" << std::endl;
}
