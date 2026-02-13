#pragma once

#include "ring_buffer.hpp"
#include "../ping/ping_result.hpp"
#include "../ping/traceroute.hpp"

#include <string>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <optional>

namespace imping {

// Default history size (5 minutes at 1 ping/second)
inline constexpr size_t DEFAULT_HISTORY_SIZE = 300;

struct TargetColor {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;

    uint32_t to_imgui() const {
        return (static_cast<uint32_t>(a * 255) << 24) |
               (static_cast<uint32_t>(b * 255) << 16) |
               (static_cast<uint32_t>(g * 255) << 8) |
               static_cast<uint32_t>(r * 255);
    }
};

struct TargetStats {
    double min_ms = 0.0;
    double max_ms = 0.0;
    double avg_ms = 0.0;
    double current_ms = 0.0;
    double jitter_ms = 0.0;
    size_t packets_sent = 0;
    size_t packets_received = 0;
    double packet_loss_percent = 0.0;
};

class PingTarget {
public:
    explicit PingTarget(const std::string& host, const TargetColor& color = {});

    // Thread-safe methods
    void add_result(const PingResult& result);
    [[nodiscard]] TargetStats get_stats() const;
    [[nodiscard]] std::vector<PingResult> get_history() const;

    // Traceroute
    void set_traceroute(const TracerouteResult& result);
    [[nodiscard]] std::optional<TracerouteResult> get_traceroute() const;
    void clear_traceroute();
    void update_hop(int hop_index, double latency_ms);
    void update_hop_timeout(int hop_index);
    [[nodiscard]] bool has_route() const;
    [[nodiscard]] std::vector<std::string> get_hop_ips() const;

    // Accessors
    [[nodiscard]] const std::string& host() const { return host_; }
    [[nodiscard]] const std::string& display_name() const { return display_name_; }
    [[nodiscard]] const std::string& resolved_ip() const { return resolved_ip_; }
    [[nodiscard]] TargetColor color() const { return color_; }
    [[nodiscard]] bool enabled() const { return enabled_.load(); }

    // Mutators
    void set_display_name(const std::string& name) { display_name_ = name; }
    void set_resolved_ip(const std::string& ip) { resolved_ip_ = ip; }
    void set_color(const TargetColor& color) { color_ = color; }
    void set_enabled(bool enabled) { enabled_.store(enabled); }

private:
    std::string host_;
    std::string display_name_;
    std::string resolved_ip_;
    TargetColor color_;
    std::atomic<bool> enabled_{true};

    mutable std::mutex mutex_;
    RingBuffer<PingResult, DEFAULT_HISTORY_SIZE> history_;
    size_t total_sent_ = 0;
    size_t total_received_ = 0;
    std::optional<TracerouteResult> traceroute_result_;
};

} // namespace imping
