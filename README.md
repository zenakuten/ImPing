# ImPing - PingPlotter like application using ImGui and SDL

![image](doc/imping.png)

## Building

### Prerequisites

- CMake 3.20+
- C++20 compiler
- [vcpkg](https://github.com/microsoft/vcpkg)

### Windows

```bash
cmake --preset vcpkg-release
cmake --build build --config Release
```

The binary will be in `build/bin/imping.exe`.

### Linux

#### Arch Linux

```bash
makepkg -si
```

This builds and installs the `imping` package. The post-install script sets `cap_net_raw` so pinging works without root.

#### Other distributions

```bash
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh -disableMetrics

cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake

cmake --build build
```

The binary will be in `build/bin/imping`. To allow pinging without root:

```bash
sudo setcap cap_net_raw+ep build/bin/imping
```