#pragma once

#include "../core/target.hpp"
#include "../ping/traceroute.hpp"

#include <imgui.h>
#include <memory>
#include <functional>
#include <atomic>

namespace imping {

class TraceroutePanel {
public:
    using RefreshCallback = std::function<void()>;

    TraceroutePanel() = default;

    void set_refresh_callback(RefreshCallback callback) { refresh_callback_ = std::move(callback); }
    void set_running(bool running) { running_.store(running); }

    void render(const std::shared_ptr<PingTarget>& target);
    void render_inline(const std::shared_ptr<PingTarget>& target);

private:
    void render_hop_table(const TracerouteResult& result);

    RefreshCallback refresh_callback_;
    std::atomic<bool> running_{false};
};

} // namespace imping
