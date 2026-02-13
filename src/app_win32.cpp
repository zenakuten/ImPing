#include "app.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>

#include <filesystem>

namespace imping {

std::string App::get_config_path() {
    std::filesystem::path dir;

    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        dir = std::filesystem::path(path) / "imping";
    } else {
        dir = ".";
    }

    std::filesystem::create_directories(dir);
    return (dir / "app_settings.txt").string();
}

} // namespace imping

// Windows entry point for GUI application (no console)
int main(int argc, char* argv[]);
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main(__argc, __argv);
}
