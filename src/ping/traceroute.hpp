#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <cstdint>
#include <atomic>

namespace imping {

struct TracerouteHop {
    int hop_number = 0;
    std::string ip;
    std::string hostname;

    // Live statistics (updated each ping interval)
    double current_ms = 0.0;
    double min_ms = 0.0;
    double max_ms = 0.0;
    double avg_ms = 0.0;
    size_t packets_sent = 0;
    size_t packets_received = 0;
    bool last_timed_out = true;

    void update(double latency_ms);
    void update_timeout();
};

struct TracerouteResult {
    std::string target_host;
    std::vector<TracerouteHop> hops;
    bool complete = false;
    bool route_discovered = false;
};

class Tracerouter {
public:
    Tracerouter();
    ~Tracerouter();

    Tracerouter(const Tracerouter&) = delete;
    Tracerouter& operator=(const Tracerouter&) = delete;

    bool initialize();

    // Discover route: find all hop IPs (one-shot)
    TracerouteResult discover_route(const std::string& host,
                                     int max_hops = 30,
                                     uint32_t timeout_ms = 1000);

    // Ping a single hop by IP (for live updates)
    // Returns latency_ms or -1 on timeout
    double ping_hop(const std::string& ip, uint32_t timeout_ms = 1000);

    static std::string reverse_dns(const std::string& ip);

private:
    struct HopProbeResult {
        std::string ip;
        double latency_ms = 0.0;
        bool timed_out = true;
    };

    HopProbeResult probe_hop(const std::string& target_ip, int ttl, uint32_t timeout_ms);
    bool initialize_platform();
    void cleanup_platform();

    void* icmp_handle_ = nullptr;  // Used by Win32 platform implementation
};

} // namespace imping
