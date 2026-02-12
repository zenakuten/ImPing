#pragma once

#include "chart_widget.hpp"
#include "target_panel.hpp"
#include "stats_panel.hpp"
#include "traceroute_panel.hpp"
#include "../core/target.hpp"

#include <imgui.h>
#include <vector>
#include <memory>
#include <functional>

namespace imping {

class MainWindow {
public:
    using AddTargetCallback = std::function<void(const std::string&)>;
    using RemoveTargetCallback = std::function<void(size_t)>;
    using SelectTargetCallback = std::function<void(int)>;
    using RefreshTracerouteCallback = std::function<void()>;

    MainWindow();

    void set_add_callback(AddTargetCallback callback);
    void set_remove_callback(RemoveTargetCallback callback);
    void set_select_callback(SelectTargetCallback callback);
    void set_refresh_traceroute_callback(RefreshTracerouteCallback callback);

    // Returns false when the user requests to quit
    bool render(std::vector<std::shared_ptr<PingTarget>>& targets);

    ChartWidget& chart() { return chart_; }
    TargetPanel& target_panel() { return target_panel_; }
    StatsPanel& stats_panel() { return stats_panel_; }
    TraceroutePanel& traceroute_panel() { return traceroute_panel_; }

    void set_selected_index(int index) { target_panel_.set_selected_index(index); }

private:
    void render_menu_bar();
    void render_toolbar(std::vector<std::shared_ptr<PingTarget>>& targets);

    ChartWidget chart_;
    TargetPanel target_panel_;
    StatsPanel stats_panel_;
    TraceroutePanel traceroute_panel_;

    bool show_stats_panel_ = true;
    bool show_target_panel_ = true;
    bool show_traceroute_panel_ = true;
    bool quit_requested_ = false;
    bool show_about_ = false;
    float traceroute_fraction_ = 0.3f;  // fraction of right side for traceroute
    int ping_interval_ms_ = 1000;
};

} // namespace imping
