#pragma once

#include "ping_result.hpp"

#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>

namespace imping {

struct PingRequest {
    std::string host;
    uint32_t timeout_ms = 1000;
    std::function<void(const std::string&, const PingResult&)> callback;
};

class Pinger {
public:
    Pinger();
    ~Pinger();

    // Non-copyable
    Pinger(const Pinger&) = delete;
    Pinger& operator=(const Pinger&) = delete;

    // Initialize the pinger (platform-specific setup)
    bool initialize();

    // Queue a ping request (thread-safe)
    void ping_async(const std::string& host,
                    uint32_t timeout_ms,
                    std::function<void(const std::string&, const PingResult&)> callback);

    // Synchronous ping (blocks until complete)
    PingResult ping(const std::string& host, uint32_t timeout_ms = 1000);

    // Resolve hostname to IP address
    static std::string resolve_host(const std::string& host);

private:
    void worker_thread();
    PingResult ping_impl(const std::string& host, uint32_t timeout_ms);
    bool initialize_platform();
    void cleanup_platform();

    std::atomic<bool> running_{false};
    std::vector<std::thread> workers_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<PingRequest> request_queue_;

    void* icmp_handle_ = nullptr;  // Used by Win32 platform implementation
};

} // namespace imping
