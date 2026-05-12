#!/bin/bash
# install_deps.sh — install all build and runtime dependencies
set -e

[ -f /etc/os-release ] && . /etc/os-release || ID="unknown"

echo "=== Installing system dependencies (distro: ${ID}) ==="

apt_install() {
    sudo apt-get update -q
    # Install per-package; a missing optional package won't abort the rest.
    for pkg in "$@"; do
        sudo apt-get install -y "${pkg}" 2>/dev/null || echo "[WARN] not found: ${pkg}"
    done
}

case "${ID}" in
    ubuntu|debian|linuxmint|pop)
        apt_install cmake ninja-build git pkg-config liburing-dev \
            qt6-base-dev qt6-charts-dev qt6-declarative-dev qt6-serialbus-dev \
            lua5.4 liblua5.4-dev luajit libluajit-5.1-dev \
            libgl1-mesa-dev libcurl4-openssl-dev \
            nlohmann-json3-dev libspdlog-dev libcpp-httplib-dev \
            libgtest-dev \
            afl++ valgrind clang-format clang-tidy can-utils
        ;;
    fedora|rhel|centos|rocky|alma)
        sudo dnf install -y cmake ninja-build git pkgconf liburing-devel \
            qt6-qtbase-devel qt6-qtcharts-devel qt6-qtdeclarative-devel \
            lua-devel luajit-devel mesa-libGL-devel libcurl-devel \
            spdlog-devel nlohmann-json-devel valgrind clang-tools-extra can-utils
        ;;
    arch|manjaro|endeavouros)
        sudo pacman -Sy --noconfirm cmake ninja git pkgconf liburing \
            qt6-base qt6-charts qt6-declarative lua luajit mesa libcurl \
            spdlog nlohmann-json valgrind clang can-utils
        ;;
    *)
        echo "[WARN] Unknown distro '${ID}'. Attempting apt-get..."
        apt_install cmake ninja-build git pkg-config liburing-dev \
            qt6-base-dev libspdlog-dev nlohmann-json3-dev valgrind
        ;;
esac

# vcpkg — used for any remaining deps not in distro repos
VCPKG_ROOT="${HOME}/.local/share/vcpkg"
if [ ! -d "${VCPKG_ROOT}" ]; then
    echo "Bootstrapping vcpkg..."
    git clone https://github.com/microsoft/vcpkg.git "${VCPKG_ROOT}"
    bash "${VCPKG_ROOT}/bootstrap-vcpkg.sh" -disableMetrics
fi

echo "=== Verification ==="
cmake --version      | head -1 && echo "[PASS] cmake"     || echo "[FAIL] cmake"
ninja --version      | head -1 && echo "[PASS] ninja"     || echo "[FAIL] ninja"
pkg-config --exists Qt6Core    && echo "[PASS] Qt6Core"   || echo "[FAIL] Qt6Core"
pkg-config --exists liburing   && echo "[PASS] liburing"  || echo "[FAIL] liburing"
pkg-config --exists lua5.4     && echo "[PASS] lua5.4"    || echo "[WARN] lua5.4 (optional)"
echo "=== Done ==="
