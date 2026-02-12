#include "target_panel.hpp"

#include <cstring>

namespace imping {

TargetPanel::TargetPanel() = default;

void TargetPanel::render(std::vector<std::shared_ptr<PingTarget>>& targets) {
    ImGui::Begin("Targets");
    render_inline(targets);
    ImGui::End();
}

void TargetPanel::render_inline(std::vector<std::shared_ptr<PingTarget>>& targets) {
    render_add_target();
    ImGui::Separator();
    render_target_list(targets);
}

void TargetPanel::render_add_target() {
    ImGui::Text("Add Target");

    ImGui::SetNextItemWidth(-60);
    bool enter_pressed = ImGui::InputText("##host", host_input_, sizeof(host_input_),
                                           ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::SameLine();

    if ((ImGui::Button("Add") || enter_pressed) && strlen(host_input_) > 0) {
        if (add_callback_) {
            add_callback_(host_input_);
        }
        host_input_[0] = '\0';
    }
}

void TargetPanel::render_target_list(std::vector<std::shared_ptr<PingTarget>>& targets) {
    // Clamp selected index if targets were removed
    if (selected_index_ >= static_cast<int>(targets.size())) {
        selected_index_ = -1;
    }

    ImGui::Text("Active Targets (%zu)", targets.size());

    for (size_t i = 0; i < targets.size(); ++i) {
        auto& target = targets[i];
        ImGui::PushID(static_cast<int>(i));

        bool is_selected = (static_cast<int>(i) == selected_index_);

        // Selectable row background
        if (ImGui::Selectable("##select", is_selected,
                              ImGuiSelectableFlags_AllowOverlap,
                              ImVec2(0, ImGui::GetFrameHeight()))) {
            int new_index = static_cast<int>(i);
            if (selected_index_ == new_index) {
                selected_index_ = -1; // Deselect on re-click
                new_index = -1;
            } else {
                selected_index_ = new_index;
            }
            if (select_callback_) {
                select_callback_(selected_index_);
            }
        }

        ImGui::SameLine(0, 0);

        // Enable/disable checkbox
        bool enabled = target->enabled();
        if (ImGui::Checkbox("##enabled", &enabled)) {
            target->set_enabled(enabled);
        }

        ImGui::SameLine();

        // Color indicator
        TargetColor color = target->color();
        ImVec4 col(color.r, color.g, color.b, color.a);
        if (ImGui::ColorEdit4("##color", &col.x,
                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
            target->set_color({col.x, col.y, col.z, col.w});
        }

        ImGui::SameLine();

        // Host name
        ImGui::Text("%s", target->display_name().c_str());

        // Show resolved IP if different from display name
        if (!target->resolved_ip().empty() &&
            target->resolved_ip() != target->display_name()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", target->resolved_ip().c_str());
        }

        ImGui::SameLine();

        // Current latency
        auto stats = target->get_stats();
        if (stats.packets_received > 0) {
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%.1fms", stats.current_ms);
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "---");
        }

        ImGui::SameLine();

        // Remove button
        if (ImGui::Button("X")) {
            if (remove_callback_) {
                remove_callback_(i);
            }
            // Adjust selection if needed
            if (selected_index_ == static_cast<int>(i)) {
                selected_index_ = -1;
            } else if (selected_index_ > static_cast<int>(i)) {
                --selected_index_;
            }
        }

        ImGui::PopID();
    }
}

const std::vector<TargetColor>& TargetPanel::get_color_palette() {
    static const std::vector<TargetColor> palette = {
        {0.3f, 0.7f, 1.0f, 1.0f},   // Light blue
        {1.0f, 0.5f, 0.3f, 1.0f},   // Orange
        {0.5f, 1.0f, 0.5f, 1.0f},   // Light green
        {1.0f, 0.7f, 0.3f, 1.0f},   // Yellow-orange
        {0.7f, 0.5f, 1.0f, 1.0f},   // Purple
        {1.0f, 0.5f, 0.7f, 1.0f},   // Pink
        {0.5f, 1.0f, 1.0f, 1.0f},   // Cyan
        {1.0f, 1.0f, 0.5f, 1.0f},   // Yellow
    };
    return palette;
}

} // namespace imping
