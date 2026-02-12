#include "stats_panel.hpp"

namespace imping {

void StatsPanel::render(const std::vector<std::shared_ptr<PingTarget>>& targets) {
    ImGui::Begin("Statistics");
    render_inline(targets);
    ImGui::End();
}

void StatsPanel::render_inline(const std::vector<std::shared_ptr<PingTarget>>& targets) {
    if (targets.empty()) {
        ImGui::TextDisabled("No targets configured");
        return;
    }

    if (ImGui::BeginTable("StatsTable", 7,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Host");
        ImGui::TableSetupColumn("Current");
        ImGui::TableSetupColumn("Min");
        ImGui::TableSetupColumn("Avg");
        ImGui::TableSetupColumn("Max");
        ImGui::TableSetupColumn("Jitter");
        ImGui::TableSetupColumn("Loss");
        ImGui::TableHeadersRow();

        for (const auto& target : targets) {
            if (!target->enabled()) continue;
            render_target_stats(*target);
        }

        ImGui::EndTable();
    }
}

void StatsPanel::render_target_stats(const PingTarget& target) {
    auto stats = target.get_stats();
    TargetColor color = target.color();
    ImVec4 col(color.r, color.g, color.b, color.a);

    ImGui::TableNextRow();

    // Host
    ImGui::TableNextColumn();
    ImGui::TextColored(col, "%s", target.display_name().c_str());

    // Current
    ImGui::TableNextColumn();
    if (stats.packets_received > 0 && stats.current_ms > 0) {
        ImGui::Text("%.1f ms", stats.current_ms);
    } else {
        ImGui::TextDisabled("---");
    }

    // Min
    ImGui::TableNextColumn();
    if (stats.packets_received > 0) {
        ImGui::Text("%.1f ms", stats.min_ms);
    } else {
        ImGui::TextDisabled("---");
    }

    // Avg
    ImGui::TableNextColumn();
    if (stats.packets_received > 0) {
        ImGui::Text("%.1f ms", stats.avg_ms);
    } else {
        ImGui::TextDisabled("---");
    }

    // Max
    ImGui::TableNextColumn();
    if (stats.packets_received > 0) {
        ImGui::Text("%.1f ms", stats.max_ms);
    } else {
        ImGui::TextDisabled("---");
    }

    // Jitter
    ImGui::TableNextColumn();
    if (stats.packets_received > 1) {
        ImGui::Text("%.1f ms", stats.jitter_ms);
    } else {
        ImGui::TextDisabled("---");
    }

    // Packet loss
    ImGui::TableNextColumn();
    if (stats.packets_sent > 0) {
        if (stats.packet_loss_percent > 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                               "%.1f%%", stats.packet_loss_percent);
        } else {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "0%%");
        }
    } else {
        ImGui::TextDisabled("---");
    }
}

} // namespace imping
