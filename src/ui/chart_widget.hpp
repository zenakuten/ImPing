#pragma once

#include "../core/target.hpp"

#include <imgui.h>
#include <vector>
#include <memory>

namespace imping {

struct ChartSettings {
    float y_min = 0.0f;
    float y_max = 100.0f;
    bool auto_scale = true;
    int visible_samples = 60;  // Number of samples visible at once
    float grid_line_interval = 20.0f;  // ms between horizontal grid lines
};

class ChartWidget {
public:
    ChartWidget();

    void render(const std::vector<std::shared_ptr<PingTarget>>& targets);

    ChartSettings& settings() { return settings_; }
    const ChartSettings& settings() const { return settings_; }

private:
    void draw_grid(ImDrawList* draw_list, ImVec2 pos, ImVec2 size);
    void draw_axis_labels(ImDrawList* draw_list, ImVec2 pos, ImVec2 size);
    void draw_series(ImDrawList* draw_list, ImVec2 pos, ImVec2 size,
                     const PingTarget& target);
    void draw_tooltip(ImVec2 pos, ImVec2 size,
                      const std::vector<std::shared_ptr<PingTarget>>& targets);

    float value_to_y(float value, ImVec2 pos, ImVec2 size) const;
    int sample_from_x(float x, ImVec2 pos, ImVec2 size, size_t total_samples) const;

    ChartSettings settings_;
    int scroll_offset_ = 0;
};

} // namespace imping
