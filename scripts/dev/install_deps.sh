#!/bin/bash
# install_deps.sh — install all build and runtime dependencies
set -e

PASS=0
FAIL=0

check() {
    local label="$1"
    shift
    if "$@" &>/dev/null; then
        echo "[PASS] ${label}"
        PASS=$((PASS + 1))
    else
        echo "[FAIL] ${label}"
        FAIL=$((FAIL + 1))
    fi
}

# Detect distro
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO_ID="${ID}"
else
    DISTRO_ID="unknown"
fi

APT_PKGS="cmake ninja-build git pkg-config liburing-dev \
    qt6-base-dev qt6-charts-dev qt6-declarative-dev qt6-serialbus-dev \
    lua5.4 liblua5.4-dev luajit libluajit-dev \
    libgl1-mesa-dev libcurl4-openssl-dev \
    nlohmann-json3-dev libspdlog-dev libcpp-httplib-dev \
    afl++ valgrind clang-format clang-tidy can-utils"

DNF_PKGS="cmake ninja-build git pkgconf liburing-devel \
    qt6-qtbase-devel qt6-qtcharts-devel qt6-qtdeclarative-devel \
    lua-devel luajit-devel mesa-libGL-devel libcurl-devel \
    afl++ valgrind clang-tools-extra can-utils"

PACMAN_PKGS="cmake ninja git pkgconf liburing \
    qt6-base qt6-charts qt6-declarative \
    lua luajit mesa libcurl \
    valgrind clang can-utils"

echo "=== Installing system dependencies (distro: ${DISTRO_ID}) ==="

case "${DISTRO_ID}" in
    ubuntu|debian|linuxmint|pop)
        sudo apt-get update -q
        sudo apt-get install -y ${APT_PKGS}
        ;;
    fedora|rhel|centos|rocky|alma)
        sudo dnf install -y ${DNF_PKGS}
        ;;
    arch|manjaro|endeavouros)
        sudo pacman -Sy --noconfirm ${PACMAN_PKGS}
        ;;
    *)
        echo "[WARN] Unknown distro '${DISTRO_ID}'. Attempting apt-get..."
        sudo apt-get install -y ${APT_PKGS} || true
        ;;
esac

# vcpkg bootstrap
VCPKG_ROOT="${HOME}/.local/share/vcpkg"
if [ ! -d "${VCPKG_ROOT}" ]; then
    echo "Bootstrapping vcpkg at ${VCPKG_ROOT}..."
    git clone https://github.com/microsoft/vcpkg.git "${VCPKG_ROOT}"
    bash "${VCPKG_ROOT}/bootstrap-vcpkg.sh" -disableMetrics
fi

check "vcpkg binary present"  test -x "${VCPKG_ROOT}/vcpkg"
check "cmake present"         cmake --version
check "ninja present"         ninja --version
check "Qt6 core pkg-config"   pkg-config --exists Qt6Core
check "liburing pkg-config"   pkg-config --exists liburing
check "lua5.4 pkg-config"     pkg-config --exists lua5.4
echo "PASS: ${PASS}  FAIL: ${FAIL}"
[ "${FAIL}" -eq 0 ] && exit 0 || exit 1
