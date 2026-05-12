#!/bin/bash
# run_integration.sh — run integration tests using vcan0/vcan1
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
INTEGRATION_DIR="${ROOT_DIR}/tests/integration"
BUILD_BIN="${ROOT_DIR}/build/dev/socketspy"
PASS=0
FAIL=0

run_test() {
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

# Verify vcan interfaces are up
ip link show vcan0 &>/dev/null || { echo "[SKIP] vcan0 not available"; exit 0; }
ip link show vcan1 &>/dev/null || { echo "[SKIP] vcan1 not available"; exit 0; }

# Verify binary exists
if [ ! -x "${BUILD_BIN}" ]; then
    echo "[SKIP] socketspy binary not built yet at ${BUILD_BIN}"
    exit 0
fi

# Run each integration test script found in tests/integration/
if [ -d "${INTEGRATION_DIR}" ]; then
    for test_script in "${INTEGRATION_DIR}"/*.sh; do
        [ -f "${test_script}" ] || continue
        label="$(basename "${test_script}" .sh)"
        run_test "${label}" bash "${test_script}" "${BUILD_BIN}"
    done
else
    echo "[SKIP] No integration tests found in ${INTEGRATION_DIR}"
fi

echo ""
echo "Integration: PASS=${PASS} FAIL=${FAIL}"
[ "${FAIL}" -eq 0 ] && exit 0 || exit 1
