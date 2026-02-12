#pragma once

#include "../core/target.hpp"

#include <imgui.h>
#include <vector>
#include <memory>

namespace imping {

class StatsPanel {
public:
    StatsPanel() = default;

    void render(const std::vector<std::shared_ptr<PingTarget>>& targets);
    void render_inline(const std::vector<std::shared_ptr<PingTarget>>& targets);

private:
    void render_target_stats(const PingTarget& target);
};

} // namespace imping
