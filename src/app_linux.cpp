#include "app.hpp"

#include <cstdlib>
#include <filesystem>

namespace imping {

std::string App::get_config_path() {
    std::filesystem::path dir;

    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        dir = std::filesystem::path(xdg) / "imping";
    } else {
        const char* home = std::getenv("HOME");
        dir = std::filesystem::path(home ? home : ".") / ".config" / "imping";
    }

    std::filesystem::create_directories(dir);
    return (dir / "recent_targets.txt").string();
}

} // namespace imping
