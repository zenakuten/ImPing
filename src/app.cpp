#include "app.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace imping {

static constexpr size_t MAX_RECENT_TARGETS = 5;

App::App() {
    color_palette_ = {
        {0.3f, 0.7f, 1.0f, 1.0f},   // Light blue
        {1.0f, 0.5f, 0.3f, 1.0f},   // Orange
        {0.5f, 1.0f, 0.5f, 1.0f},   // Light green
        {1.0f, 0.7f, 0.3f, 1.0f},   // Yellow-orange
        {0.7f, 0.5f, 1.0f, 1.0f},   // Purple
        {1.0f, 0.5f, 0.7f, 1.0f},   // Pink
        {0.5f, 1.0f, 1.0f, 1.0f},   // Cyan
        {1.0f, 1.0f, 0.5f, 1.0f},   // Yellow
    };
}

App::~App() {
    shutdown();
}

void App::parse_args(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.starts_with("-")) {
            continue;
        }
        add_target(arg);
    }
}

bool App::initialize() {
    if (!pinger_.initialize()) {
        return false;
    }

    if (!tracerouter_.initialize()) {
        return false;
    }

    // Load recent targets if none were provided via command line
    if (targets_.empty()) {
        load_settings();
    }

    // Set up UI callbacks - queue operations to avoid deadlock
    main_window_.set_add_callback([this](const std::string& host) {
        std::lock_guard lock(pending_mutex_);
        pending_adds_.push_back(host);
    });

    main_window_.set_remove_callback([this](size_t index) {
        std::lock_guard lock(pending_mutex_);
        pending_removes_.push_back(index);
    });

    main_window_.set_select_callback([this](int index) {
        std::lock_guard lock(pending_mutex_);
        pending_selection_ = index;
    });

    main_window_.set_refresh_traceroute_callback([this](int index) {
        std::lock_guard lock(pending_mutex_);
        pending_traceroute_refresh_ = index;
    });

    // Start ping thread
    running_.store(true);
    ping_thread_ = std::thread(&App::ping_thread_func, this);

    return true;
}

void App::update() {
    std::vector<std::string> adds;
    std::vector<size_t> removes;
    std::optional<int> selection;
    std::optional<int> refresh_traceroute;

    {
        std::lock_guard lock(pending_mutex_);
        adds.swap(pending_adds_);
        removes.swap(pending_removes_);
        selection = pending_selection_;
        pending_selection_.reset();
        refresh_traceroute = pending_traceroute_refresh_;
        pending_traceroute_refresh_.reset();
    }

    for (const auto& host : adds) {
        add_target(host);
    }

    std::sort(removes.begin(), removes.end(), std::greater<>());
    for (size_t index : removes) {
        remove_target(index);
    }

    // Handle target selection change
    if (selection.has_value()) {
        int new_index = selection.value();
        int old_index = selected_target_index_.load();

        if (new_index != old_index) {
            selected_target_index_.store(new_index);

            // Auto-run traceroute when a new target is selected
            if (new_index >= 0) {
                std::shared_ptr<PingTarget> target;
                {
                    std::lock_guard lock(targets_mutex_);
                    if (new_index < static_cast<int>(targets_.size())) {
                        target = targets_[new_index];
                    }
                }
                if (target && !target->get_traceroute().has_value()) {
                    run_traceroute(target);
                }
            }
        }
    }

    // Handle traceroute refresh
    if (refresh_traceroute.has_value()) {
        int idx = refresh_traceroute.value();
        if (idx >= 0) {
            std::shared_ptr<PingTarget> target;
            {
                std::lock_guard lock(targets_mutex_);
                if (idx < static_cast<int>(targets_.size())) {
                    target = targets_[idx];
                }
            }
            if (target) {
                run_traceroute(target);
            }
        }
    }

    // Sync ping interval from UI slider
    ping_interval_ = std::chrono::milliseconds(main_window_.ping_interval_ms());

    // Sync active traceroute tab for hop pinging
    traceroute_tab_index_.store(main_window_.traceroute_panel().active_tab_index());

    // Update traceroute panel running state
    main_window_.traceroute_panel().set_running(traceroute_running_.load());
}

bool App::render() {
    std::lock_guard lock(targets_mutex_);
    return main_window_.render(targets_);
}

void App::shutdown() {
    running_.store(false);

    if (ping_thread_.joinable()) {
        ping_thread_.join();
    }

    if (traceroute_thread_.joinable()) {
        traceroute_thread_.join();
    }

    save_settings();
}

void App::add_target(const std::string& host) {
    if (host.empty()) return;

    auto color = get_next_color();
    auto target = std::make_shared<PingTarget>(host, color);

    std::string ip = Pinger::resolve_host(host);
    if (!ip.empty()) {
        target->set_resolved_ip(ip);
    }

    std::lock_guard lock(targets_mutex_);
    targets_.push_back(std::move(target));
}

void App::remove_target(size_t index) {
    std::lock_guard lock(targets_mutex_);

    if (index < targets_.size()) {
        targets_.erase(targets_.begin() + static_cast<ptrdiff_t>(index));

        // Adjust selected index
        int sel = selected_target_index_.load();
        if (sel == static_cast<int>(index)) {
            selected_target_index_.store(-1);
        } else if (sel > static_cast<int>(index)) {
            selected_target_index_.store(sel - 1);
        }
    }
}

void App::set_ping_interval(std::chrono::milliseconds interval) {
    ping_interval_ = interval;
}

void App::ping_thread_func() {
    while (running_.load()) {
        auto start = std::chrono::steady_clock::now();

        // Ping all enabled targets
        std::vector<std::shared_ptr<PingTarget>> targets_snapshot;
        {
            std::lock_guard lock(targets_mutex_);
            targets_snapshot = targets_;
        }

        for (const auto& target : targets_snapshot) {
            if (!target->enabled()) continue;

            std::string host = target->resolved_ip().empty()
                               ? target->host()
                               : target->resolved_ip();

            auto result = pinger_.ping(host, 1000);
            target->add_result(result);
        }

        // Ping all hops of the active traceroute tab's route
        int sel = traceroute_tab_index_.load();
        if (sel >= 0 && sel < static_cast<int>(targets_snapshot.size())) {
            auto& selected = targets_snapshot[sel];
            if (selected->has_route()) {
                auto hop_ips = selected->get_hop_ips();
                for (int i = 0; i < static_cast<int>(hop_ips.size()); ++i) {
                    if (hop_ips[i].empty()) {
                        selected->update_hop_timeout(i);
                        continue;
                    }
                    double latency = tracerouter_.ping_hop(hop_ips[i], 1000);
                    if (latency >= 0) {
                        selected->update_hop(i, latency);
                    } else {
                        selected->update_hop_timeout(i);
                    }
                }
            }
        }

        auto elapsed = std::chrono::steady_clock::now() - start;
        auto remaining = ping_interval_ - std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);

        if (remaining > std::chrono::milliseconds(0)) {
            std::this_thread::sleep_for(remaining);
        }
    }
}

void App::run_traceroute(std::shared_ptr<PingTarget> target) {
    if (traceroute_running_.load()) {
        return;
    }

    if (traceroute_thread_.joinable()) {
        traceroute_thread_.join();
    }

    traceroute_running_.store(true);

    traceroute_thread_ = std::thread([this, target]() {
        std::string host = target->resolved_ip().empty()
                           ? target->host()
                           : target->resolved_ip();

        auto result = tracerouter_.discover_route(host);
        target->set_traceroute(result);
        traceroute_running_.store(false);
    });
}

TargetColor App::get_next_color() {
    if (color_palette_.empty()) {
        return {1.0f, 1.0f, 1.0f, 1.0f};
    }

    TargetColor color = color_palette_[next_color_index_ % color_palette_.size()];
    ++next_color_index_;
    return color;
}

void App::load_settings() {
    std::ifstream file(get_config_path());
    if (!file.is_open()) return;

    std::string line;
    size_t count = 0;
    while (std::getline(file, line)) {
        // Trim whitespace
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        auto end = line.find_last_not_of(" \t\r\n");
        std::string trimmed = line.substr(start, end - start + 1);
        if (trimmed.empty()) continue;

        // Parse key=value settings
        if (trimmed.starts_with("interval=")) {
            int ms = std::atoi(trimmed.c_str() + 9);
            ms = std::clamp(ms, 100, 5000);
            ping_interval_ = std::chrono::milliseconds(ms);
            main_window_.set_ping_interval_ms(ms);
            continue;
        }

        // Otherwise treat as a target hostname
        if (count < MAX_RECENT_TARGETS) {
            add_target(trimmed);
            ++count;
        }
    }
}

void App::save_settings() {
    std::lock_guard lock(targets_mutex_);

    std::ofstream file(get_config_path());
    if (!file.is_open()) return;

    file << "interval=" << ping_interval_.count() << "\n";

    // Save the last 5 targets
    size_t start = 0;
    if (targets_.size() > MAX_RECENT_TARGETS) {
        start = targets_.size() - MAX_RECENT_TARGETS;
    }

    for (size_t i = start; i < targets_.size(); ++i) {
        file << targets_[i]->host() << "\n";
    }
}

} // namespace imping
