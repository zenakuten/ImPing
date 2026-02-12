#include "pinger.hpp"

namespace imping {

Pinger::Pinger() = default;

Pinger::~Pinger() {
    running_.store(false);
    queue_cv_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    cleanup_platform();
}

bool Pinger::initialize() {
    if (!initialize_platform()) {
        return false;
    }

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

} // namespace imping
