#include "target.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace imping {

PingTarget::PingTarget(const std::string& host, const TargetColor& color)
    : host_(host)
    , display_name_(host)
    , color_(color) {
}

void PingTarget::add_result(const PingResult& result) {
    std::lock_guard lock(mutex_);
    history_.push(result);
    ++total_sent_;
    if (result.success) {
        ++total_received_;
    }
}

TargetStats PingTarget::get_stats() const {
    std::lock_guard lock(mutex_);

    TargetStats stats;
    stats.packets_sent = total_sent_;
    stats.packets_received = total_received_;

    if (total_sent_ > 0) {
        stats.packet_loss_percent =
            100.0 * static_cast<double>(total_sent_ - total_received_) /
            static_cast<double>(total_sent_);
    }

    if (history_.empty()) {
        return stats;
    }

    // Calculate statistics from successful pings
    std::vector<double> latencies;
    latencies.reserve(history_.size());

    for (const auto& result : history_) {
        if (result.success) {
            latencies.push_back(result.latency_ms);
        }
    }

    if (latencies.empty()) {
        return stats;
    }

    stats.min_ms = *std::min_element(latencies.begin(), latencies.end());
    stats.max_ms = *std::max_element(latencies.begin(), latencies.end());
    stats.avg_ms = std::accumulate(latencies.begin(), latencies.end(), 0.0) /
                   static_cast<double>(latencies.size());

    // Current is the most recent successful ping
    if (auto last = history_.back(); last && last->success) {
        stats.current_ms = last->latency_ms;
    }

    // Calculate jitter (average deviation from mean)
    if (latencies.size() > 1) {
        double sum_deviation = 0.0;
        for (double lat : latencies) {
            sum_deviation += std::abs(lat - stats.avg_ms);
        }
        stats.jitter_ms = sum_deviation / static_cast<double>(latencies.size());
    }

    return stats;
}

std::vector<PingResult> PingTarget::get_history() const {
    std::lock_guard lock(mutex_);
    std::vector<PingResult> result;
    result.reserve(history_.size());
    for (const auto& entry : history_) {
        result.push_back(entry);
    }
    return result;
}

void PingTarget::set_traceroute(const TracerouteResult& result) {
    std::lock_guard lock(mutex_);
    traceroute_result_ = result;
}

std::optional<TracerouteResult> PingTarget::get_traceroute() const {
    std::lock_guard lock(mutex_);
    return traceroute_result_;
}

void PingTarget::update_hop(int hop_index, double latency_ms) {
    std::lock_guard lock(mutex_);
    if (traceroute_result_ && hop_index >= 0 &&
        hop_index < static_cast<int>(traceroute_result_->hops.size())) {
        traceroute_result_->hops[hop_index].update(latency_ms);
    }
}

void PingTarget::update_hop_timeout(int hop_index) {
    std::lock_guard lock(mutex_);
    if (traceroute_result_ && hop_index >= 0 &&
        hop_index < static_cast<int>(traceroute_result_->hops.size())) {
        traceroute_result_->hops[hop_index].update_timeout();
    }
}

bool PingTarget::has_route() const {
    std::lock_guard lock(mutex_);
    return traceroute_result_.has_value() && traceroute_result_->route_discovered;
}

std::vector<std::string> PingTarget::get_hop_ips() const {
    std::lock_guard lock(mutex_);
    std::vector<std::string> ips;
    if (traceroute_result_) {
        for (const auto& hop : traceroute_result_->hops) {
            ips.push_back(hop.ip); // empty string for timed-out hops
        }
    }
    return ips;
}

} // namespace imping
