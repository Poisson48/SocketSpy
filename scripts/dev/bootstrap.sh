#!/bin/bash
# bootstrap.sh — full dev environment setup + build + test
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PASS=0
FAIL=0

run_step() {
    local label="$1"
    shift
    if "$@"; then
        echo "[PASS] ${label}"
        PASS=$((PASS + 1))
    else
        echo "[FAIL] ${label}"
        FAIL=$((FAIL + 1))
    fi
}

echo "=== SocketSpy bootstrap ==="

run_step "install dependencies" bash "${SCRIPT_DIR}/install_deps.sh"

run_step "setup vcan interfaces" bash "${SCRIPT_DIR}/setup_vcan.sh"

run_step "cmake configure (dev preset)" \
    cmake --preset dev -S "${SCRIPT_DIR}/../../" 2>&1

run_step "cmake build (dev preset)" \
    cmake --build --preset dev 2>&1

run_step "run tests" bash "${SCRIPT_DIR}/run_tests.sh"

echo ""
echo "=== Summary ==="
echo "PASS: ${PASS}  FAIL: ${FAIL}"

if [ "${FAIL}" -gt 0 ]; then
    echo "Bootstrap FAILED — see output above."
    exit 1
fi

echo "Bootstrap complete. Binary: build/dev/socketspy"
exit 0
