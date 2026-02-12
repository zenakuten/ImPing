#include "chart_widget.hpp"

#include <algorithm>
#include <cmath>

namespace imping {

ChartWidget::ChartWidget() = default;

void ChartWidget::render(const std::vector<std::shared_ptr<PingTarget>>& targets) {
    ImGui::BeginChild("ChartArea", ImVec2(0, 0), ImGuiChildFlags_Borders);

    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();

    // Minimum size
    if (canvas_size.x < 100 || canvas_size.y < 100) {
        ImGui::EndChild();
        return;
    }

    // Reserve space for axis labels
    constexpr float left_margin = 50.0f;
    constexpr float bottom_margin = 20.0f;
    constexpr float top_margin = 10.0f;
    constexpr float right_margin = 10.0f;

    ImVec2 chart_pos(canvas_pos.x + left_margin, canvas_pos.y + top_margin);
    ImVec2 chart_size(canvas_size.x - left_margin - right_margin,
                      canvas_size.y - top_margin - bottom_margin);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Background
    draw_list->AddRectFilled(chart_pos,
                             ImVec2(chart_pos.x + chart_size.x, chart_pos.y + chart_size.y),
                             IM_COL32(20, 20, 30, 255));

    // Auto-scale Y axis
    if (settings_.auto_scale && !targets.empty()) {
        float max_latency = 0.0f;
        for (const auto& target : targets) {
            if (!target->enabled()) continue;
            auto stats = target->get_stats();
            max_latency = std::max(max_latency, static_cast<float>(stats.max_ms));
        }
        // Round up to nice value
        if (max_latency > 0) {
            settings_.y_max = std::ceil(max_latency / 10.0f) * 10.0f + 10.0f;
            settings_.y_max = std::max(settings_.y_max, 50.0f);
        }
    }

    draw_grid(draw_list, chart_pos, chart_size);
    draw_axis_labels(draw_list, chart_pos, chart_size);

    // Draw each target's data
    for (const auto& target : targets) {
        if (target->enabled()) {
            draw_series(draw_list, chart_pos, chart_size, *target);
        }
    }

    // Handle zoom with mouse wheel
    if (ImGui::IsWindowHovered()) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0) {
            settings_.visible_samples = std::clamp(
                settings_.visible_samples + static_cast<int>(wheel * -5),
                10, 300
            );
        }
    }

    // Tooltip on hover
    draw_tooltip(chart_pos, chart_size, targets);

    ImGui::EndChild();
}

void ChartWidget::draw_grid(ImDrawList* draw_list, ImVec2 pos, ImVec2 size) {
    const ImU32 grid_color = IM_COL32(60, 60, 80, 255);
    const ImU32 grid_color_major = IM_COL32(80, 80, 100, 255);

    // Horizontal grid lines
    float y_range = settings_.y_max - settings_.y_min;
    int num_lines = static_cast<int>(y_range / settings_.grid_line_interval);

    for (int i = 0; i <= num_lines; ++i) {
        float value = settings_.y_min + i * settings_.grid_line_interval;
        float y = value_to_y(value, pos, size);

        ImU32 color = (i % 5 == 0) ? grid_color_major : grid_color;
        draw_list->AddLine(ImVec2(pos.x, y), ImVec2(pos.x + size.x, y), color);
    }

    // Vertical grid lines (time-based)
    int num_v_lines = 6;
    for (int i = 0; i <= num_v_lines; ++i) {
        float x = pos.x + (size.x * i / num_v_lines);
        draw_list->AddLine(ImVec2(x, pos.y), ImVec2(x, pos.y + size.y), grid_color);
    }
}

void ChartWidget::draw_axis_labels(ImDrawList* draw_list, ImVec2 pos, ImVec2 size) {
    const ImU32 text_color = IM_COL32(200, 200, 200, 255);

    // Y-axis labels
    float y_range = settings_.y_max - settings_.y_min;
    int num_labels = 5;
    for (int i = 0; i <= num_labels; ++i) {
        float value = settings_.y_min + (y_range * i / num_labels);
        float y = value_to_y(value, pos, size);

        char label[32];
        snprintf(label, sizeof(label), "%.0f", value);

        ImVec2 text_size = ImGui::CalcTextSize(label);
        draw_list->AddText(ImVec2(pos.x - text_size.x - 5, y - text_size.y / 2),
                           text_color, label);
    }

    // X-axis label (ms)
    draw_list->AddText(ImVec2(pos.x - 45, pos.y - 5), text_color, "ms");
}

void ChartWidget::draw_series(ImDrawList* draw_list, ImVec2 pos, ImVec2 size,
                               const PingTarget& target) {
    auto history = target.get_history();
    if (history.empty()) return;

    ImU32 color = target.color().to_imgui();
    ImU32 timeout_color = IM_COL32(255, 50, 50, 200);

    // Calculate visible range
    int total_samples = static_cast<int>(history.size());
    int start_idx = std::max(0, total_samples - settings_.visible_samples - scroll_offset_);
    int end_idx = std::min(total_samples, start_idx + settings_.visible_samples);

    if (start_idx >= end_idx) return;

    float x_step = size.x / static_cast<float>(settings_.visible_samples - 1);

    std::vector<ImVec2> points;
    points.reserve(end_idx - start_idx);

    for (int i = start_idx; i < end_idx; ++i) {
        const auto& result = history[i];
        float x = pos.x + (i - start_idx) * x_step;

        if (result.success) {
            float y = value_to_y(static_cast<float>(result.latency_ms), pos, size);
            points.push_back(ImVec2(x, y));
        } else {
            // Draw existing points if any
            if (points.size() > 1) {
                draw_list->AddPolyline(points.data(), static_cast<int>(points.size()),
                                       color, ImDrawFlags_None, 2.0f);
            }
            points.clear();

            // Draw timeout marker
            float y_top = pos.y + 5;
            draw_list->AddRectFilled(ImVec2(x - 2, y_top),
                                     ImVec2(x + 2, pos.y + size.y - 5),
                                     timeout_color);
        }
    }

    // Draw remaining points
    if (points.size() > 1) {
        draw_list->AddPolyline(points.data(), static_cast<int>(points.size()),
                               color, ImDrawFlags_None, 2.0f);
    }

    // Draw dots at data points
    for (int i = start_idx; i < end_idx; ++i) {
        const auto& result = history[i];
        if (result.success) {
            float x = pos.x + (i - start_idx) * x_step;
            float y = value_to_y(static_cast<float>(result.latency_ms), pos, size);
            draw_list->AddCircleFilled(ImVec2(x, y), 3.0f, color);
        }
    }
}

void ChartWidget::draw_tooltip(ImVec2 pos, ImVec2 size,
                                const std::vector<std::shared_ptr<PingTarget>>& targets) {
    ImVec2 mouse = ImGui::GetMousePos();

    // Check if mouse is within chart area
    if (mouse.x < pos.x || mouse.x > pos.x + size.x ||
        mouse.y < pos.y || mouse.y > pos.y + size.y) {
        return;
    }

    ImGui::BeginTooltip();

    for (const auto& target : targets) {
        if (!target->enabled()) continue;

        auto history = target->get_history();
        if (history.empty()) continue;

        int sample_idx = sample_from_x(mouse.x, pos, size, history.size());
        if (sample_idx < 0 || sample_idx >= static_cast<int>(history.size())) continue;

        const auto& result = history[sample_idx];
        ImVec4 color(target->color().r, target->color().g,
                     target->color().b, target->color().a);

        ImGui::TextColored(color, "%s:", target->display_name().c_str());
        ImGui::SameLine();

        if (result.success) {
            ImGui::Text("%.2f ms", result.latency_ms);
        } else {
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Timeout");
        }
    }

    ImGui::EndTooltip();
}

float ChartWidget::value_to_y(float value, ImVec2 pos, ImVec2 size) const {
    float normalized = (value - settings_.y_min) / (settings_.y_max - settings_.y_min);
    return pos.y + size.y - (normalized * size.y);
}

int ChartWidget::sample_from_x(float x, ImVec2 pos, ImVec2 size, size_t total_samples) const {
    float relative_x = (x - pos.x) / size.x;
    int visible_index = static_cast<int>(relative_x * (settings_.visible_samples - 1));

    int start_idx = std::max(0, static_cast<int>(total_samples) - settings_.visible_samples - scroll_offset_);
    return start_idx + visible_index;
}

} // namespace imping
