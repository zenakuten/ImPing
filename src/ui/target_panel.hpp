#pragma once

#include "../core/target.hpp"

#include <imgui.h>
#include <vector>
#include <memory>
#include <functional>

namespace imping {

class TargetPanel {
public:
    using AddTargetCallback = std::function<void(const std::string&)>;
    using RemoveTargetCallback = std::function<void(size_t)>;
    using SelectTargetCallback = std::function<void(int)>;

    TargetPanel();

    void set_add_callback(AddTargetCallback callback) { add_callback_ = std::move(callback); }
    void set_remove_callback(RemoveTargetCallback callback) { remove_callback_ = std::move(callback); }
    void set_select_callback(SelectTargetCallback callback) { select_callback_ = std::move(callback); }

    void render(std::vector<std::shared_ptr<PingTarget>>& targets);
    void render_inline(std::vector<std::shared_ptr<PingTarget>>& targets);

    [[nodiscard]] int selected_index() const { return selected_index_; }
    void set_selected_index(int index) { selected_index_ = index; }

private:
    void render_add_target();
    void render_target_list(std::vector<std::shared_ptr<PingTarget>>& targets);

    char host_input_[256] = {};
    int selected_index_ = -1;
    AddTargetCallback add_callback_;
    RemoveTargetCallback remove_callback_;
    SelectTargetCallback select_callback_;

    // Color palette for new targets
    static const std::vector<TargetColor>& get_color_palette();
    size_t next_color_index_ = 0;
};

} // namespace imping
