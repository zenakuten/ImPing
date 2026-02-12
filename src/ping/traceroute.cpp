#include "traceroute.hpp"

#include <algorithm>

namespace imping {

// --- TracerouteHop live stat updates ---

void TracerouteHop::update(double latency_ms) {
    ++packets_sent;
    ++packets_received;
    current_ms = latency_ms;
    last_timed_out = false;

    if (packets_received == 1) {
        min_ms = latency_ms;
        max_ms = latency_ms;
        avg_ms = latency_ms;
    } else {
        min_ms = std::min(min_ms, latency_ms);
        max_ms = std::max(max_ms, latency_ms);
        // Running average
        avg_ms += (latency_ms - avg_ms) / static_cast<double>(packets_received);
    }
}

void TracerouteHop::update_timeout() {
    ++packets_sent;
    last_timed_out = true;
}

// --- Tracerouter ---

Tracerouter::Tracerouter() = default;

Tracerouter::~Tracerouter() {
    cleanup_platform();
}

bool Tracerouter::initialize() {
    return initialize_platform();
}

} // namespace imping
