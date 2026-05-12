#!/bin/bash
# build.sh — build SocketSpy in one command
# Usage:  bash build.sh [--clean] [--release]
set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${ROOT_DIR}"

PRESET="dev"
CLEAN=0

for arg in "$@"; do
    case "${arg}" in
        --clean)   CLEAN=1 ;;
        --release) PRESET="release" ;;
    esac
done

# ── Prerequisites check ──────────────────────────────────────────────────────
need() { command -v "$1" &>/dev/null || { echo "[ERROR] '$1' not found. Run: bash scripts/dev/install_deps.sh"; exit 1; }; }
need cmake
need ninja

CMAKE_VER=$(cmake --version | head -1 | grep -oP '\d+\.\d+')
MIN_MAJOR=3; MIN_MINOR=28
IFS='.' read -r CUR_MAJOR CUR_MINOR <<< "${CMAKE_VER}"
if [ "${CUR_MAJOR}" -lt "${MIN_MAJOR}" ] || \
   { [ "${CUR_MAJOR}" -eq "${MIN_MAJOR}" ] && [ "${CUR_MINOR}" -lt "${MIN_MINOR}" ]; }; then
    echo "[ERROR] CMake ${MIN_MAJOR}.${MIN_MINOR}+ required (found ${CMAKE_VER})"
    exit 1
fi

# ── Clean ────────────────────────────────────────────────────────────────────
if [ "${CLEAN}" -eq 1 ] && [ -d "build/${PRESET}" ]; then
    echo "[INFO] Cleaning build/${PRESET}..."
    rm -rf "build/${PRESET}"
fi

# ── Configure (only if not already done) ────────────────────────────────────
if [ ! -f "build/${PRESET}/build.ninja" ]; then
    echo "[INFO] Configuring (preset=${PRESET})..."
    cmake --preset "${PRESET}"
fi

# ── Build ────────────────────────────────────────────────────────────────────
JOBS=$(nproc 2>/dev/null || echo 4)
echo "[INFO] Building with ${JOBS} jobs..."
cmake --build --preset "${PRESET}" --parallel "${JOBS}"

# ── Result ───────────────────────────────────────────────────────────────────
BINARY="${ROOT_DIR}/build/${PRESET}/gui/socketspy"
if [ -x "${BINARY}" ]; then
    SIZE=$(du -sh "${BINARY}" | cut -f1)
    echo ""
    echo "╔══════════════════════════════════════════════╗"
    echo "║  Build complete — ${SIZE} binary ready       "
    echo "║  ${BINARY}"
    echo "╚══════════════════════════════════════════════╝"
    echo ""
    echo "Run:  ${BINARY}"
    echo "      (or)  DISPLAY=:0 ${BINARY} &"
else
    echo "[ERROR] Binary not found at ${BINARY}"
    exit 1
fi
