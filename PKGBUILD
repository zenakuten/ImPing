# Maintainer:
pkgname=imping
pkgver=0.2.0
pkgrel=1
pkgdesc='A real-time ping and traceroute visualizer using ImGui and SDL3'
arch=('x86_64')
license=('MIT')
depends=('gcc-libs')
makedepends=('cmake' 'git' 'curl' 'zip' 'unzip' 'tar' 'pkg-config' 'ninja')
install=imping.install
source=()
sha256sums=()

prepare() {
    if [[ ! -d "$srcdir/_vcpkg" ]]; then
        git clone https://github.com/microsoft/vcpkg.git "$srcdir/_vcpkg"
        "$srcdir/_vcpkg/bootstrap-vcpkg.sh" -disableMetrics
    fi
}

build() {
    cmake -B "$srcdir/_build" -S "$startdir" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$srcdir/_vcpkg/scripts/buildsystems/vcpkg.cmake"

    cmake --build "$srcdir/_build"
}

package() {
    install -Dm755 "$srcdir/_build/bin/imping" "$pkgdir/usr/bin/imping"
}
