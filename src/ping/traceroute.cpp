#include "traceroute.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#endif

#include <algorithm>
#include <chrono>

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
#ifdef _WIN32
    if (icmp_handle_) {
        IcmpCloseHandle(icmp_handle_);
    }
#endif
}

bool Tracerouter::initialize() {
#ifdef _WIN32
    icmp_handle_ = IcmpCreateFile();
    if (icmp_handle_ == INVALID_HANDLE_VALUE) {
        icmp_handle_ = nullptr;
        return false;
    }
#endif
    return true;
}

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

double Tracerouter::ping_hop(const std::string& ip, uint32_t timeout_ms) {
    // Ping a known hop IP directly (full TTL, no TTL trick needed)
    // We just need the round-trip time to this specific IP.

#ifdef _WIN32
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

#else
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) return -1.0;

    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &dest_addr.sin_addr);

    struct {
        uint8_t type;
        uint8_t code;
        uint16_t checksum;
        uint16_t id;
        uint16_t sequence;
        char data[56];
    } packet{};

    packet.type = 8;
    packet.id = htons(static_cast<uint16_t>(getpid() & 0xFFFF));
    packet.sequence = htons(1);

    uint32_t sum = 0;
    auto* ptr = reinterpret_cast<uint16_t*>(&packet);
    size_t len = sizeof(packet);
    while (len > 1) { sum += *ptr++; len -= 2; }
    if (len == 1) sum += *reinterpret_cast<uint8_t*>(ptr);
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    packet.checksum = static_cast<uint16_t>(~sum);

    auto start = std::chrono::steady_clock::now();

    ssize_t sent = sendto(sock, &packet, sizeof(packet), 0,
                          reinterpret_cast<sockaddr*>(&dest_addr), sizeof(dest_addr));
    if (sent < 0) { close(sock); return -1.0; }

    char recv_buf[1024];
    sockaddr_in from{};
    socklen_t from_len = sizeof(from);
    ssize_t received = recvfrom(sock, recv_buf, sizeof(recv_buf), 0,
                                 reinterpret_cast<sockaddr*>(&from), &from_len);

    auto end = std::chrono::steady_clock::now();
    close(sock);

    if (received < 0) return -1.0;

    return std::chrono::duration<double, std::milli>(end - start).count();
#endif
}

Tracerouter::HopProbeResult Tracerouter::probe_hop(const std::string& target_ip,
                                                     int ttl,
                                                     uint32_t timeout_ms) {
    HopProbeResult result;

#ifdef _WIN32
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

#else
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) return result;

    setsockopt(sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));

    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    inet_pton(AF_INET, target_ip.c_str(), &dest_addr.sin_addr);

    struct {
        uint8_t type;
        uint8_t code;
        uint16_t checksum;
        uint16_t id;
        uint16_t sequence;
        char data[56];
    } packet{};

    packet.type = 8;
    packet.id = htons(static_cast<uint16_t>(getpid() & 0xFFFF));
    packet.sequence = htons(static_cast<uint16_t>(ttl));

    uint32_t sum = 0;
    auto* cptr = reinterpret_cast<uint16_t*>(&packet);
    size_t clen = sizeof(packet);
    while (clen > 1) { sum += *cptr++; clen -= 2; }
    if (clen == 1) sum += *reinterpret_cast<uint8_t*>(cptr);
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    packet.checksum = static_cast<uint16_t>(~sum);

    auto start = std::chrono::steady_clock::now();

    ssize_t sent = sendto(sock, &packet, sizeof(packet), 0,
                          reinterpret_cast<sockaddr*>(&dest_addr), sizeof(dest_addr));
    if (sent < 0) { close(sock); return result; }

    char recv_buf[1024];
    sockaddr_in from{};
    socklen_t from_len = sizeof(from);
    ssize_t received = recvfrom(sock, recv_buf, sizeof(recv_buf), 0,
                                 reinterpret_cast<sockaddr*>(&from), &from_len);

    auto end = std::chrono::steady_clock::now();
    close(sock);

    if (received < 0) return result;

    char addr_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &from.sin_addr, addr_str, sizeof(addr_str));
    result.ip = addr_str;
    result.latency_ms = std::chrono::duration<double, std::milli>(end - start).count();
    result.timed_out = false;

    return result;
#endif
}

} // namespace imping
