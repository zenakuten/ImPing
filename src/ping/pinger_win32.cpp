#include "pinger.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

#include <vector>

namespace imping {

bool Pinger::initialize_platform() {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return false;
    }

    icmp_handle_ = IcmpCreateFile();
    if (icmp_handle_ == INVALID_HANDLE_VALUE) {
        WSACleanup();
        return false;
    }

    return true;
}

void Pinger::cleanup_platform() {
    if (icmp_handle_) {
        IcmpCloseHandle(icmp_handle_);
        icmp_handle_ = nullptr;
    }
    WSACleanup();
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

PingResult Pinger::ping_impl(const std::string& host, uint32_t timeout_ms) {
    std::string ip = resolve_host(host);
    if (ip.empty()) {
        return PingResult::Failure("Failed to resolve hostname");
    }

    IPAddr dest_addr;
    if (inet_pton(AF_INET, ip.c_str(), &dest_addr) != 1) {
        return PingResult::Failure("Invalid IP address");
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
}

} // namespace imping
