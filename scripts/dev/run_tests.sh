#!/bin/bash
# run_tests.sh — build both configs, run all test suites, report results
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/../../"
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

# Ensure vcan is up
bash "${SCRIPT_DIR}/setup_vcan.sh"

# Build dev (Debug)
run_step "cmake build dev" \
    cmake --build --preset dev --parallel "$(nproc)"

# Build ci (Debug + coverage)
run_step "cmake build ci" \
    cmake --build --preset ci --parallel "$(nproc)" 2>/dev/null || \
    echo "[SKIP] ci preset not configured — run cmake --preset ci first"

# Unit tests
run_step "ctest dev" \
    bash -c "cd '${ROOT_DIR}' && ctest --preset dev --output-on-failure"

# Integration tests
run_step "integration tests" \
    bash "${SCRIPT_DIR}/run_integration.sh"

# Fuzz (60 s per target)
run_step "fuzz targets" \
    bash "${SCRIPT_DIR}/run_fuzz.sh"

# Valgrind on core unit test binary
CORE_TEST="${ROOT_DIR}/build/dev/core/tests/core_tests"
if [ -x "${CORE_TEST}" ]; then
    run_step "valgrind core_tests" \
        valgrind --error-exitcode=1 --leak-check=full \
            --suppressions="${ROOT_DIR}/.valgrind.supp" \
            "${CORE_TEST}" --gtest_brief=1 2>/dev/null
else
    echo "[SKIP] core_tests binary not found (not built yet)"
fi

echo ""
echo "=== Test Summary ==="
printf "%-20s %s\n" "PASS" "${PASS}"
printf "%-20s %s\n" "FAIL" "${FAIL}"

[ "${FAIL}" -eq 0 ] && exit 0 || exit 1
