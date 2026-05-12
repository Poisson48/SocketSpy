#!/bin/bash
# build_appimage.sh — package SocketSpy as a self-contained AppImage
# Usage:  bash scripts/dist/build_appimage.sh
# Output: SocketSpy-x86_64.AppImage  (in project root)
set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DIST_DIR="${ROOT_DIR}/scripts/dist"
TOOLS_DIR="${ROOT_DIR}/build/appimage-tools"
APPDIR="${ROOT_DIR}/build/AppDir"

cd "${ROOT_DIR}"

# ── 1. Release build ─────────────────────────────────────────────────────────
echo "[1/5] Building release..."
[ -f "build/release/build.ninja" ] || cmake --preset release
cmake --build --preset release --parallel "$(nproc)"
BINARY="${ROOT_DIR}/build/release/gui/socketspy"
[ -x "${BINARY}" ] || { echo "[ERROR] Binary not found"; exit 1; }

# ── 2. Download linuxdeploy + Qt6 plugin ─────────────────────────────────────
echo "[2/5] Fetching linuxdeploy tools..."
mkdir -p "${TOOLS_DIR}"

LINUXDEPLOY="${TOOLS_DIR}/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="${TOOLS_DIR}/linuxdeploy-plugin-qt-x86_64.AppImage"

if [ ! -f "${LINUXDEPLOY}" ]; then
    curl -fsSL -o "${LINUXDEPLOY}" \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    chmod +x "${LINUXDEPLOY}"
fi
if [ ! -f "${LINUXDEPLOY_QT}" ]; then
    curl -fsSL -o "${LINUXDEPLOY_QT}" \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
    chmod +x "${LINUXDEPLOY_QT}"
fi

# ── 3. Prepare AppDir ────────────────────────────────────────────────────────
echo "[3/5] Preparing AppDir..."
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"

cp "${BINARY}"                     "${APPDIR}/usr/bin/socketspy"
cp "${DIST_DIR}/socketspy.desktop" "${APPDIR}/usr/share/applications/"
cp "${DIST_DIR}/socketspy.png"     "${APPDIR}/usr/share/icons/hicolor/256x256/apps/socketspy.png"
# AppImage also needs icon at root of AppDir
cp "${DIST_DIR}/socketspy.png"     "${APPDIR}/socketspy.png"

# ── 4. Bundle Qt6 + all shared dependencies ──────────────────────────────────
echo "[4/5] Bundling Qt6 dependencies (this takes ~1 min)..."

QT6_QMAKE=$(command -v qmake6 2>/dev/null || \
            command -v qmake  2>/dev/null || \
            find /usr/lib/qt6 /usr/lib/x86_64-linux-gnu/qt6 -name "qmake" 2>/dev/null | head -1)
[ -n "${QT6_QMAKE}" ] || { echo "[ERROR] qmake6 not found — install qt6-base-dev"; exit 1; }
echo "  Using qmake: ${QT6_QMAKE}"

export QMAKE="${QT6_QMAKE}"
export EXTRA_QT_PLUGINS="charts;serialbus"

"${LINUXDEPLOY}" --appimage-extract-and-run \
    --appdir "${APPDIR}" \
    --plugin qt \
    --executable "${APPDIR}/usr/bin/socketspy" \
    --desktop-file "${DIST_DIR}/socketspy.desktop" \
    --icon-file "${DIST_DIR}/socketspy.png" \
    --output appimage 2>&1 | grep -Ev "^$|rpath|WARNING.*optional"

# ── 5. Report ────────────────────────────────────────────────────────────────
echo "[5/5] Done."
APPIMAGE=$(ls "${ROOT_DIR}"/SocketSpy*.AppImage 2>/dev/null | head -1)
[ -z "${APPIMAGE}" ] && APPIMAGE=$(ls SocketSpy*.AppImage 2>/dev/null | head -1)

if [ -f "${APPIMAGE}" ]; then
    SIZE=$(du -sh "${APPIMAGE}" | cut -f1)
    echo ""
    echo "╔══════════════════════════════════════════╗"
    echo "║  AppImage ready — ${SIZE}                "
    echo "║  $(basename "${APPIMAGE}")"
    echo "╚══════════════════════════════════════════╝"
    echo "Run:  chmod +x $(basename "${APPIMAGE}") && ./$(basename "${APPIMAGE}")"
else
    echo "[ERROR] AppImage not produced — check linuxdeploy output above."
    exit 1
fi
