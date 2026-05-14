#!/bin/bash
# install_deps.sh — install all build and runtime dependencies
# Usage: bash scripts/dev/install_deps.sh
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
        # Required — build will fail without these
        apt_install \
            build-essential cmake ninja-build git pkg-config \
            qt6-base-dev qt6-charts-dev qt6-serialbus-dev qt6-serialport-dev \
            qt6-tools-dev qt6-l10n-tools \
            lua5.4 liblua5.4-dev \
            nlohmann-json3-dev

        # Optional — gracefully disabled at configure time if absent
        apt_install \
            libspdlog-dev libcpp-httplib-dev \
            libgtest-dev \
            can-utils
        ;;
    fedora|rhel|centos|rocky|alma)
        sudo dnf install -y \
            gcc-c++ cmake ninja-build git pkgconf \
            qt6-qtbase-devel qt6-qtcharts-devel qt6-qtserialbus-devel qt6-qtserialport-devel \
            qt6-linguist \
            lua-devel \
            nlohmann-json-devel spdlog-devel \
            gtest-devel \
            can-utils
        ;;
    arch|manjaro|endeavouros)
        sudo pacman -Sy --noconfirm \
            base-devel cmake ninja git \
            qt6-base qt6-charts qt6-serialbus qt6-serialport qt6-tools \
            lua \
            nlohmann-json spdlog \
            gtest \
            can-utils
        ;;
    *)
        echo "[WARN] Unknown distro '${ID}'. Attempting apt-get..."
        apt_install cmake ninja-build git pkg-config \
            qt6-base-dev qt6-charts-dev qt6-serialbus-dev qt6-serialport-dev \
            qt6-tools-dev qt6-l10n-tools \
            lua5.4 liblua5.4-dev nlohmann-json3-dev
        ;;
esac

echo ""
echo "=== Verification ==="
cmake --version   | head -1 && echo "[PASS] cmake"        || echo "[FAIL] cmake"
ninja --version   | head -1 && echo "[PASS] ninja"        || echo "[FAIL] ninja"
pkg-config --exists Qt6Core      && echo "[PASS] Qt6Core"      || echo "[FAIL] Qt6Core"
pkg-config --exists Qt6Charts    && echo "[PASS] Qt6Charts"    || echo "[WARN] Qt6Charts missing"
pkg-config --exists Qt6SerialBus && echo "[PASS] Qt6SerialBus" || echo "[WARN] Qt6SerialBus missing"
pkg-config --exists lua5.4       && echo "[PASS] lua5.4"       || echo "[WARN] lua5.4 missing"
echo "=== Done ==="
echo ""
echo "Build:  bash build.sh"
echo "Run:    ./build/dev/gui/socketspy"
