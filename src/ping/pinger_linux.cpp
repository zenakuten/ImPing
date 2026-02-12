#include "pinger.hpp"

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <cerrno>
#include <chrono>

namespace imping {

struct IcmpPacket {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
    char data[56];
};

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

std::string Pinger::resolve_host(const std::string& host) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;

    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0) {
        return {};
    }

    std::string ip;
    if (result && result->ai_addr) {
        char ip_str[INET_ADDRSTRLEN];
        auto* addr = reinterpret_cast<sockaddr_in*>(result->ai_addr);
        if (inet_ntop(AF_INET, &addr->sin_addr, ip_str, sizeof(ip_str))) {
            ip = ip_str;
        }
    }

    freeaddrinfo(result);
    return ip;
}

bool Pinger::initialize_platform() {
    return true;
}

void Pinger::cleanup_platform() {
}

PingResult Pinger::ping_impl(const std::string& host, uint32_t timeout_ms) {
    std::string ip = resolve_host(host);
    if (ip.empty()) {
        return PingResult::Failure("Failed to resolve hostname");
    }

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        if (errno == EPERM || errno == EACCES) {
            return PingResult::Failure("Permission denied - run as root or set CAP_NET_RAW");
        }
        return PingResult::Failure("Failed to create socket: " + std::string(strerror(errno)));
    }

    // Set socket timeout
    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &dest_addr.sin_addr);

    // Prepare ICMP packet
    IcmpPacket packet{};
    packet.type = 8;  // ICMP Echo Request
    packet.code = 0;
    packet.id = htons(static_cast<uint16_t>(getpid() & 0xFFFF));
    packet.sequence = htons(1);
    memset(packet.data, 0, sizeof(packet.data));
    packet.checksum = calculate_checksum(&packet, sizeof(packet));

    auto start = std::chrono::steady_clock::now();

    ssize_t sent = sendto(sock, &packet, sizeof(packet), 0,
                          reinterpret_cast<sockaddr*>(&dest_addr), sizeof(dest_addr));
    if (sent < 0) {
        close(sock);
        return PingResult::Failure("Failed to send: " + std::string(strerror(errno)));
    }

    // Wait for reply
    char recv_buffer[1024];
    sockaddr_in from_addr{};
    socklen_t from_len = sizeof(from_addr);

    ssize_t received = recvfrom(sock, recv_buffer, sizeof(recv_buffer), 0,
                                 reinterpret_cast<sockaddr*>(&from_addr), &from_len);

    auto end = std::chrono::steady_clock::now();
    close(sock);

    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return PingResult::Timeout();
        }
        return PingResult::Failure("Failed to receive: " + std::string(strerror(errno)));
    }

    // Parse reply (skip IP header - typically 20 bytes)
    if (received < 28) {  // IP header + ICMP header minimum
        return PingResult::Failure("Invalid reply");
    }

    auto* icmp_reply = reinterpret_cast<IcmpPacket*>(recv_buffer + 20);
    if (icmp_reply->type != 0) {  // Not an echo reply
        return PingResult::Failure("Unexpected ICMP type: " + std::to_string(icmp_reply->type));
    }

    double latency_ms = std::chrono::duration<double, std::milli>(end - start).count();
    return PingResult::Success(latency_ms);
}

} // namespace imping
