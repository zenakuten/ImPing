#include "traceroute.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

#include <vector>
#include <chrono>

namespace imping {

std::string Tracerouter::reverse_dns(const std::string& ip) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    char host[NI_MAXHOST];
    if (getnameinfo(reinterpret_cast<sockaddr*>(&addr), sizeof(addr),
                    host, sizeof(host), nullptr, 0, 0) == 0) {
        return host;
    }
    return {};
}

TracerouteResult Tracerouter::discover_route(const std::string& host,
                                              int max_hops,
                                              uint32_t timeout_ms) {
    TracerouteResult result;
    result.target_host = host;

    // Resolve hostname to IP
    addrinfo hints{};
    hints.ai_family = AF_INET;
    addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res) {
        return result;
    }

    char ip_str[INET_ADDRSTRLEN];
    auto* addr = reinterpret_cast<sockaddr_in*>(res->ai_addr);
    inet_ntop(AF_INET, &addr->sin_addr, ip_str, sizeof(ip_str));
    std::string target_ip = ip_str;
    freeaddrinfo(res);

    for (int ttl = 1; ttl <= max_hops; ++ttl) {
        auto probe = probe_hop(target_ip, ttl, timeout_ms);

        TracerouteHop hop;
        hop.hop_number = ttl;

        if (!probe.timed_out) {
            hop.ip = probe.ip;
            hop.hostname = reverse_dns(probe.ip);
            hop.last_timed_out = false;
            // Record the discovery ping as the first data point
            hop.update(probe.latency_ms);
        } else {
            hop.last_timed_out = true;
        }

        result.hops.push_back(hop);

        // Reached destination
        if (probe.ip == target_ip) {
            result.complete = true;
            break;
        }
    }

    result.route_discovered = true;
    return result;
}

bool Tracerouter::initialize_platform() {
    icmp_handle_ = IcmpCreateFile();
    if (icmp_handle_ == INVALID_HANDLE_VALUE) {
        icmp_handle_ = nullptr;
        return false;
    }
    return true;
}

void Tracerouter::cleanup_platform() {
    if (icmp_handle_) {
        IcmpCloseHandle(icmp_handle_);
        icmp_handle_ = nullptr;
    }
}

double Tracerouter::ping_hop(const std::string& ip, uint32_t timeout_ms) {
    if (!icmp_handle_) return -1.0;

    IPAddr dest_addr;
    if (inet_pton(AF_INET, ip.c_str(), &dest_addr) != 1) {
        return -1.0;
    }

    char send_data[] = "imping";
    constexpr DWORD reply_size = sizeof(ICMP_ECHO_REPLY) + sizeof(send_data) + 8;
    std::vector<char> reply_buffer(reply_size);

    DWORD reply_count = IcmpSendEcho(
        icmp_handle_,
        dest_addr,
        send_data,
        sizeof(send_data),
        nullptr,
        reply_buffer.data(),
        reply_size,
        timeout_ms
    );

    if (reply_count == 0) return -1.0;

    auto* reply = reinterpret_cast<ICMP_ECHO_REPLY*>(reply_buffer.data());
    if (reply->Status != IP_SUCCESS) return -1.0;

    return static_cast<double>(reply->RoundTripTime);
}

Tracerouter::HopProbeResult Tracerouter::probe_hop(const std::string& target_ip,
                                                     int ttl,
                                                     uint32_t timeout_ms) {
    HopProbeResult result;

    if (!icmp_handle_) return result;

    IPAddr dest_addr;
    if (inet_pton(AF_INET, target_ip.c_str(), &dest_addr) != 1) {
        return result;
    }

    char send_data[] = "imping";
    constexpr DWORD reply_size = sizeof(ICMP_ECHO_REPLY) + sizeof(send_data) + 8;
    std::vector<char> reply_buffer(reply_size);

    IP_OPTION_INFORMATION opts{};
    opts.Ttl = static_cast<UCHAR>(ttl);

    DWORD reply_count = IcmpSendEcho(
        icmp_handle_,
        dest_addr,
        send_data,
        sizeof(send_data),
        &opts,
        reply_buffer.data(),
        reply_size,
        timeout_ms
    );

    auto* reply = reinterpret_cast<ICMP_ECHO_REPLY*>(reply_buffer.data());

    if (reply_count == 0) {
        DWORD err = GetLastError();
        if (err == IP_TTL_EXPIRED_TRANSIT || err == IP_TTL_EXPIRED_REASSEM) {
            in_addr reply_addr;
            reply_addr.s_addr = reply->Address;
            char addr_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &reply_addr, addr_str, sizeof(addr_str));
            result.ip = addr_str;
            result.latency_ms = static_cast<double>(reply->RoundTripTime);
            result.timed_out = false;
            return result;
        }
        return result; // timed_out = true
    }

    if (reply->Status == IP_SUCCESS ||
        reply->Status == IP_TTL_EXPIRED_TRANSIT ||
        reply->Status == IP_TTL_EXPIRED_REASSEM) {
        in_addr reply_addr;
        reply_addr.s_addr = reply->Address;
        char addr_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &reply_addr, addr_str, sizeof(addr_str));
        result.ip = addr_str;
        result.latency_ms = static_cast<double>(reply->RoundTripTime);
        result.timed_out = false;
    }

    return result;
}

} // namespace imping
