#include "traceroute_panel.hpp"

namespace imping {

void TraceroutePanel::render(const std::shared_ptr<PingTarget>& target) {
    ImGui::Begin("Traceroute");
    render_inline(target);
    ImGui::End();
}

void TraceroutePanel::render_inline(const std::shared_ptr<PingTarget>& target) {
    if (!target) {
        ImGui::TextDisabled("Select a target to view traceroute");
        return;
    }

    // Header with target name and refresh button
    ImGui::Text("Traceroute: %s", target->display_name().c_str());

    ImGui::SameLine();

    bool is_running = running_.load();
    if (is_running) {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Refresh")) {
        if (refresh_callback_) {
            refresh_callback_();
        }
    }

    if (is_running) {
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Discovering route...");
    }

    ImGui::Separator();

    auto traceroute = target->get_traceroute();
    if (!traceroute.has_value()) {
        if (!is_running) {
            ImGui::TextDisabled("No traceroute data. Click Refresh to run.");
        }
        return;
    }

    render_hop_table(traceroute.value());
}

void TraceroutePanel::render_hop_table(const TracerouteResult& result) {
    if (result.hops.empty()) {
        ImGui::TextDisabled("No hops found");
        return;
    }

    if (ImGui::BeginTable("TracerouteTable", 8,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                          ImVec2(0, 0))) {
        ImGui::TableSetupColumn("Hop", ImGuiTableColumnFlags_WidthFixed, 30.0f);
        ImGui::TableSetupColumn("IP", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Hostname", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthFixed, 65.0f);
        ImGui::TableSetupColumn("Avg", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Min", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Max", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Loss", ImGuiTableColumnFlags_WidthFixed, 45.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        for (const auto& hop : result.hops) {
            ImGui::TableNextRow();

            // Hop number
            ImGui::TableNextColumn();
            ImGui::Text("%d", hop.hop_number);

            // IP
            ImGui::TableNextColumn();
            if (hop.ip.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "*");
            } else {
                ImGui::Text("%s", hop.ip.c_str());
            }

            // Hostname
            ImGui::TableNextColumn();
            if (hop.ip.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No response");
            } else if (!hop.hostname.empty() && hop.hostname != hop.ip) {
                ImGui::Text("%s", hop.hostname.c_str());
            } else {
                ImGui::TextDisabled("---");
            }

            // Current latency
            ImGui::TableNextColumn();
            if (hop.ip.empty() || hop.last_timed_out) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "*");
            } else {
                ImGui::Text("%.1f ms", hop.current_ms);
            }

            // Avg
            ImGui::TableNextColumn();
            if (hop.packets_received > 0) {
                ImGui::Text("%.1f ms", hop.avg_ms);
            } else {
                ImGui::TextDisabled("---");
            }

            // Min
            ImGui::TableNextColumn();
            if (hop.packets_received > 0) {
                ImGui::Text("%.1f ms", hop.min_ms);
            } else {
                ImGui::TextDisabled("---");
            }

            // Max
            ImGui::TableNextColumn();
            if (hop.packets_received > 0) {
                ImGui::Text("%.1f ms", hop.max_ms);
            } else {
                ImGui::TextDisabled("---");
            }

            // Packet loss
            ImGui::TableNextColumn();
            if (hop.packets_sent > 0) {
                double loss = 100.0 * static_cast<double>(hop.packets_sent - hop.packets_received)
                              / static_cast<double>(hop.packets_sent);
                if (loss > 0) {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%.0f%%", loss);
                } else {
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "0%%");
                }
            } else {
                ImGui::TextDisabled("---");
            }
        }

        ImGui::EndTable();
    }

    if (result.complete) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                           "Route: %zu hops", result.hops.size());
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                           "Route incomplete (%zu hops)", result.hops.size());
    }
}

} // namespace imping
