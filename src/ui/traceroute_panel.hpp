#pragma once

#include "../core/target.hpp"
#include "../ping/traceroute.hpp"

#include <imgui.h>
#include <memory>
#include <functional>
#include <atomic>
#include <vector>

namespace imping {

class TraceroutePanel {
public:
    using RefreshCallback = std::function<void(int)>;

    TraceroutePanel() = default;

    void set_refresh_callback(RefreshCallback callback) { refresh_callback_ = std::move(callback); }
    void set_running(bool running) { running_.store(running); }

    void render_inline(std::vector<std::shared_ptr<PingTarget>>& targets, int selected_index);

    [[nodiscard]] int active_tab_index() const { return active_tab_index_; }

private:
    void render_hop_table(const TracerouteResult& result);

    RefreshCallback refresh_callback_;
    std::atomic<bool> running_{false};
    int active_tab_index_ = -1;
};

} // namespace imping
