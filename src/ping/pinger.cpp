#include "pinger.hpp"

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
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <cerrno>
#endif

#include <chrono>

namespace imping {

#ifndef _WIN32
// ICMP packet structure for Unix
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
#endif

Pinger::Pinger() = default;

Pinger::~Pinger() {
    running_.store(false);
    queue_cv_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

#ifdef _WIN32
    if (icmp_handle_) {
        IcmpCloseHandle(icmp_handle_);
    }
    WSACleanup();
#endif
}

bool Pinger::initialize() {
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return false;
    }

    icmp_handle_ = IcmpCreateFile();
    if (icmp_handle_ == INVALID_HANDLE_VALUE) {
        WSACleanup();
        return false;
    }
#endif

    running_.store(true);

    // Create worker threads
    unsigned int num_workers = std::max(2u, std::thread::hardware_concurrency() / 2);
    for (unsigned int i = 0; i < num_workers; ++i) {
        workers_.emplace_back(&Pinger::worker_thread, this);
    }

    return true;
}

void Pinger::ping_async(const std::string& host,
                        uint32_t timeout_ms,
                        std::function<void(const std::string&, const PingResult&)> callback) {
    PingRequest request;
    request.host = host;
    request.timeout_ms = timeout_ms;
    request.callback = std::move(callback);

    {
        std::lock_guard lock(queue_mutex_);
        request_queue_.push(std::move(request));
    }
    queue_cv_.notify_one();
}

PingResult Pinger::ping(const std::string& host, uint32_t timeout_ms) {
    return ping_impl(host, timeout_ms);
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

void Pinger::worker_thread() {
    while (running_.load()) {
        PingRequest request;

        {
            std::unique_lock lock(queue_mutex_);
            queue_cv_.wait(lock, [this] {
                return !request_queue_.empty() || !running_.load();
            });

            if (!running_.load() && request_queue_.empty()) {
                break;
            }

            if (request_queue_.empty()) {
                continue;
            }

            request = std::move(request_queue_.front());
            request_queue_.pop();
        }

        auto result = ping_impl(request.host, request.timeout_ms);
        if (request.callback) {
            request.callback(request.host, result);
        }
    }
}

PingResult Pinger::ping_impl(const std::string& host, uint32_t timeout_ms) {
    std::string ip = resolve_host(host);
    if (ip.empty()) {
        return PingResult::Failure("Failed to resolve hostname");
    }

#ifdef _WIN32
    // Windows implementation using ICMP API
    IPAddr dest_addr;
    if (inet_pton(AF_INET, ip.c_str(), &dest_addr) != 1) {
        return PingResult::Failure("Invalid IP address");
    }

    char send_data[] = "imping";
    constexpr DWORD reply_size = sizeof(ICMP_ECHO_REPLY) + sizeof(send_data) + 8;
    std::vector<char> reply_buffer(reply_size);

    auto start = std::chrono::steady_clock::now();

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

    if (reply_count == 0) {
        DWORD error = GetLastError();
        if (error == IP_REQ_TIMED_OUT) {
            return PingResult::Timeout();
        }
        return PingResult::Failure("ICMP send failed: " + std::to_string(error));
    }

    auto* reply = reinterpret_cast<ICMP_ECHO_REPLY*>(reply_buffer.data());
    if (reply->Status != IP_SUCCESS) {
        return PingResult::Failure("ICMP error: " + std::to_string(reply->Status));
    }

    return PingResult::Success(static_cast<double>(reply->RoundTripTime));

#else
    // Unix implementation using raw sockets
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
#endif
}

} // namespace imping
