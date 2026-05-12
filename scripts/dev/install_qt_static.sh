#!/bin/bash
# install_qt_static.sh — install Qt 6.6 static build via aqtinstall
# Only needed for the AppImage release build (cmake --preset release)
set -e

QT_VERSION="6.6.3"
QT_ARCH="linux_gcc_64"
QT_INSTALL_DIR="${HOME}/Qt"
MODULES="qtcharts qtserialbus qtserialport qtdeclarative"

echo "=== Installing Qt ${QT_VERSION} (static) via aqtinstall ==="

# Ensure pip3 is available
if ! command -v pip3 &>/dev/null; then
    echo "pip3 not found — installing python3-pip"
    sudo apt-get install -y python3-pip
fi

# Install or upgrade aqtinstall
pip3 install --quiet --upgrade aqtinstall

# Install Qt base
aqt install-qt linux desktop "${QT_VERSION}" "${QT_ARCH}" \
    --outputdir "${QT_INSTALL_DIR}" \
    --modules ${MODULES}

QT_PREFIX="${QT_INSTALL_DIR}/${QT_VERSION}/gcc_64"

if [ ! -d "${QT_PREFIX}" ]; then
    echo "[FAIL] Qt installation not found at ${QT_PREFIX}"
    exit 1
fi

echo "[PASS] Qt ${QT_VERSION} installed at ${QT_PREFIX}"
echo ""
echo "To use in release build, configure with:"
echo "  cmake --preset release -DCMAKE_PREFIX_PATH=${QT_PREFIX}"
echo ""
echo "Or export before running bootstrap:"
echo "  export Qt6_DIR=${QT_PREFIX}/lib/cmake/Qt6"
