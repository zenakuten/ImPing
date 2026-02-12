#include "traceroute.hpp"

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
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
    return true;
}

void Tracerouter::cleanup_platform() {
}

static uint16_t calculate_checksum(void* data, size_t len) {
    auto* ptr = static_cast<uint16_t*>(data);
    uint32_t sum = 0;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }

    if (len == 1) {
        sum += *reinterpret_cast<uint8_t*>(ptr);
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);

    return static_cast<uint16_t>(~sum);
}

double Tracerouter::ping_hop(const std::string& ip, uint32_t timeout_ms) {
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
    packet.checksum = calculate_checksum(&packet, sizeof(packet));

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
}

Tracerouter::HopProbeResult Tracerouter::probe_hop(const std::string& target_ip,
                                                     int ttl,
                                                     uint32_t timeout_ms) {
    HopProbeResult result;

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
    packet.checksum = calculate_checksum(&packet, sizeof(packet));

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
}

} // namespace imping
