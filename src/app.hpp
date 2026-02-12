#pragma once

#include "core/target.hpp"
#include "ping/pinger.hpp"
#include "ping/traceroute.hpp"
#include "ui/main_window.hpp"

#include <vector>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

namespace imping {

class App {
public:
    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    void parse_args(int argc, char* argv[]);
    bool initialize();
    void update();
    // Returns false when user wants to quit
    bool render();
    void shutdown();

    void add_target(const std::string& host);
    void remove_target(size_t index);

    void set_ping_interval(std::chrono::milliseconds interval);
    [[nodiscard]] std::chrono::milliseconds ping_interval() const { return ping_interval_; }

private:
    void ping_thread_func();
    void run_traceroute(std::shared_ptr<PingTarget> target);
    TargetColor get_next_color();

    static std::string get_config_path();
    void load_recent_targets();
    void save_recent_targets();

    std::vector<std::shared_ptr<PingTarget>> targets_;
    std::mutex targets_mutex_;

    Pinger pinger_;
    Tracerouter tracerouter_;
    MainWindow main_window_;

    std::thread ping_thread_;
    std::atomic<bool> running_{false};
    std::chrono::milliseconds ping_interval_{1000};

    // Traceroute
    std::atomic<int> selected_target_index_{-1};
    std::thread traceroute_thread_;
    std::atomic<bool> traceroute_running_{false};

    // Pending operations (queued from UI callbacks, processed in update())
    std::mutex pending_mutex_;
    std::vector<std::string> pending_adds_;
    std::vector<size_t> pending_removes_;
    std::optional<int> pending_selection_;
    bool pending_traceroute_refresh_ = false;

    // Color palette for new targets
    std::vector<TargetColor> color_palette_;
    size_t next_color_index_ = 0;
};

} // namespace imping
