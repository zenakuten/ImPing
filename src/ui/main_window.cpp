#include "main_window.hpp"

#include <algorithm>

namespace imping {

MainWindow::MainWindow() = default;

void MainWindow::set_add_callback(AddTargetCallback callback) {
    target_panel_.set_add_callback(std::move(callback));
}

void MainWindow::set_remove_callback(RemoveTargetCallback callback) {
    target_panel_.set_remove_callback(std::move(callback));
}

void MainWindow::set_select_callback(SelectTargetCallback callback) {
    target_panel_.set_select_callback(std::move(callback));
}

void MainWindow::set_refresh_traceroute_callback(RefreshTracerouteCallback callback) {
    traceroute_panel_.set_refresh_callback(std::move(callback));
}

bool MainWindow::render(std::vector<std::shared_ptr<PingTarget>>& targets) {
    render_menu_bar();

    if (quit_requested_) return false;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 work_pos = viewport->WorkPos;
    ImVec2 work_size = viewport->WorkSize;

    // Left side panel (targets + stats)
    float side_panel_width = 0.0f;
    constexpr float splitter_v_thickness = 6.0f;

    if (show_target_panel_ || show_stats_panel_) {
        // Clamp side panel width
        float min_w = 150.0f;
        float max_w = work_size.x * 0.5f;
        side_panel_width_ = std::max(min_w, std::min(side_panel_width_, max_w));
        side_panel_width = side_panel_width_;

        ImGui::SetNextWindowPos(work_pos);
        ImGui::SetNextWindowSize(ImVec2(side_panel_width, work_size.y));
        ImGui::Begin("##SidePanel", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        if (show_target_panel_) {
            if (ImGui::CollapsingHeader("Targets", ImGuiTreeNodeFlags_DefaultOpen)) {
                target_panel_.render_inline(targets);
            }
        }

        if (show_stats_panel_) {
            if (ImGui::CollapsingHeader("Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
                stats_panel_.render_inline(targets);
            }
        }

        ImGui::End();

        // Vertical splitter between side panel and chart
        float splitter_x = work_pos.x + side_panel_width;

        ImGui::SetNextWindowPos(ImVec2(splitter_x, work_pos.y));
        ImGui::SetNextWindowSize(ImVec2(splitter_v_thickness, work_size.y));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1, 1));
        ImGui::Begin("##VSplitter", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav);

        // Draw a visible handle line
        ImDrawList* vdl = ImGui::GetWindowDrawList();
        float vcx = splitter_x + splitter_v_thickness * 0.5f;
        float vcy = work_pos.y + work_size.y * 0.5f;
        vdl->AddLine(ImVec2(vcx, vcy - 20), ImVec2(vcx, vcy + 20),
                     IM_COL32(120, 120, 140, 255), 2.0f);

        ImGui::End();
        ImGui::PopStyleVar(2);

        // Invisible drag button over the vertical splitter
        ImGui::SetNextWindowPos(ImVec2(splitter_x, work_pos.y));
        ImGui::SetNextWindowSize(ImVec2(splitter_v_thickness, work_size.y));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1, 1));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
        ImGui::Begin("##VSplitterDrag", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav);

        ImGui::InvisibleButton("##vsplitter_btn", ImVec2(splitter_v_thickness, work_size.y));
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        if (ImGui::IsItemActive()) {
            float delta = ImGui::GetIO().MouseDelta.x;
            if (delta != 0.0f) {
                side_panel_width_ += delta;
                side_panel_width_ = std::max(min_w, std::min(side_panel_width_, max_w));
            }
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }

        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        side_panel_width += splitter_v_thickness;
    }

    // Right side: chart on top, traceroute on bottom with draggable splitter
    float right_x = work_pos.x + side_panel_width;
    float right_width = work_size.x - side_panel_width;

    float traceroute_height = 0.0f;
    constexpr float splitter_thickness = 6.0f;

    if (show_traceroute_panel_) {
        traceroute_height = work_size.y * traceroute_fraction_;
        // Clamp to reasonable bounds
        float min_panel = 60.0f;
        traceroute_height = std::max(min_panel, std::min(traceroute_height, work_size.y - min_panel - splitter_thickness));
    }

    float chart_height = work_size.y - traceroute_height - (show_traceroute_panel_ ? splitter_thickness : 0.0f);

    // Chart
    ImGui::SetNextWindowPos(ImVec2(right_x, work_pos.y));
    ImGui::SetNextWindowSize(ImVec2(right_width, chart_height));
    ImGui::Begin("Ping Chart", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoCollapse);
    render_toolbar(targets);
    chart_.render(targets);
    ImGui::End();

    // Draggable splitter between chart and traceroute
    if (show_traceroute_panel_) {
        float splitter_y = work_pos.y + chart_height;

        ImGui::SetNextWindowPos(ImVec2(right_x, splitter_y));
        ImGui::SetNextWindowSize(ImVec2(right_width, splitter_thickness));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1, 1));
        ImGui::Begin("##Splitter", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav);

        // Draw a visible handle line
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        float cx = right_x + right_width * 0.5f;
        float cy = splitter_y + splitter_thickness * 0.5f;
        draw_list->AddLine(ImVec2(cx - 20, cy), ImVec2(cx + 20, cy),
                           IM_COL32(120, 120, 140, 255), 2.0f);

        ImGui::End();
        ImGui::PopStyleVar(2);

        // Invisible drag button over the splitter area
        ImGui::SetNextWindowPos(ImVec2(right_x, splitter_y));
        ImGui::SetNextWindowSize(ImVec2(right_width, splitter_thickness));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1, 1));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
        ImGui::Begin("##SplitterDrag", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav);

        ImGui::InvisibleButton("##splitter_btn", ImVec2(right_width, splitter_thickness));
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }
        if (ImGui::IsItemActive()) {
            float delta = ImGui::GetIO().MouseDelta.y;
            if (delta != 0.0f) {
                traceroute_fraction_ -= delta / work_size.y;
                traceroute_fraction_ = std::max(0.1f, std::min(0.7f, traceroute_fraction_));
            }
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }

        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        // Traceroute panel
        ImGui::SetNextWindowPos(ImVec2(right_x, splitter_y + splitter_thickness));
        ImGui::SetNextWindowSize(ImVec2(right_width, traceroute_height));
        ImGui::Begin("Traceroute", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse);

        traceroute_panel_.render_inline(targets, target_panel_.selected_index());
        ImGui::End();
    }

    return true;
}

void MainWindow::render_menu_bar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                quit_requested_ = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Targets Panel", nullptr, &show_target_panel_);
            ImGui::MenuItem("Statistics Panel", nullptr, &show_stats_panel_);
            ImGui::MenuItem("Traceroute Panel", nullptr, &show_traceroute_panel_);
            ImGui::Separator();

            if (ImGui::BeginMenu("Chart Settings")) {
                auto& settings = chart_.settings();

                ImGui::Checkbox("Auto-scale Y axis", &settings.auto_scale);

                if (!settings.auto_scale) {
                    ImGui::SliderFloat("Y Max", &settings.y_max, 10.0f, 1000.0f, "%.0f ms");
                }

                ImGui::SliderInt("Visible Samples", &settings.visible_samples, 10, 300);
                ImGui::SliderFloat("Grid Interval", &settings.grid_line_interval, 5.0f, 100.0f, "%.0f ms");

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                show_about_ = true;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if (show_about_) {
        ImGui::OpenPopup("About ImPing");
        show_about_ = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("About ImPing", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("ImPing");
        ImGui::Separator();
        ImGui::Text("Version 0.2.0");
        ImGui::Text("A real-time ping and traceroute visualizer.");
        ImGui::Spacing();
        ImGui::TextDisabled("Built with ImGui, SDL3, and vcpkg");
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void MainWindow::render_toolbar(std::vector<std::shared_ptr<PingTarget>>& targets) {
    ImGui::SetNextItemWidth(100);
    if (ImGui::SliderInt("Interval", &ping_interval_ms_, 100, 5000, "%d ms")) {
        // Interval changed
    }

    ImGui::SameLine();
    ImGui::TextDisabled("| Targets: %zu", targets.size());

    size_t enabled = 0;
    for (const auto& t : targets) {
        if (t->enabled()) ++enabled;
    }

    ImGui::SameLine();
    ImGui::TextDisabled("| Active: %zu", enabled);

    ImGui::SameLine();
    ImGui::TextDisabled("| Scroll: Mouse wheel to zoom");
}

} // namespace imping
