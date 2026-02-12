#pragma once

#include <chrono>
#include <string>

namespace imping {

struct PingResult {
    std::chrono::steady_clock::time_point timestamp;
    double latency_ms = 0.0;
    bool success = false;
    std::string error_message;

    static PingResult Success(double latency_ms) {
        PingResult result;
        result.timestamp = std::chrono::steady_clock::now();
        result.latency_ms = latency_ms;
        result.success = true;
        return result;
    }

    static PingResult Failure(const std::string& error) {
        PingResult result;
        result.timestamp = std::chrono::steady_clock::now();
        result.success = false;
        result.error_message = error;
        return result;
    }

    static PingResult Timeout() {
        return Failure("Request timed out");
    }
};

} // namespace imping
