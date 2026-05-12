#!/bin/bash
# run_fuzz.sh — run AFL++ fuzz targets for 60 seconds each, check for crashes
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
FUZZ_BIN_DIR="${ROOT_DIR}/build/dev/tests/fuzz"
FUZZ_CORPUS_DBC="${ROOT_DIR}/dbc/corpus"
FUZZ_TIMEOUT=60
PASS=0
FAIL=0

run_fuzz_target() {
    local label="$1"
    local binary="$2"
    local corpus="$3"
    local findings_dir
    findings_dir="/tmp/fuzz_findings_${label}_$$"

    if [ ! -x "${binary}" ]; then
        echo "[SKIP] ${label} — binary not found at ${binary}"
        return
    fi

    mkdir -p "${findings_dir}/in"
    # Seed with corpus if available
    if [ -d "${corpus}" ] && [ -n "$(ls -A "${corpus}" 2>/dev/null)" ]; then
        cp "${corpus}"/* "${findings_dir}/in/" 2>/dev/null || true
    else
        echo "seed" > "${findings_dir}/in/seed"
    fi

    echo "Running fuzz target: ${label} (${FUZZ_TIMEOUT}s)..."
    if timeout "${FUZZ_TIMEOUT}" afl-fuzz \
        -i "${findings_dir}/in" \
        -o "${findings_dir}/out" \
        -t 1000 \
        -- "${binary}" @@ &>/tmp/fuzz_log_"${label}".txt; then
        : # afl-fuzz exits 0 on timeout expiry (expected)
    fi

    # Check for crashes
    crash_count=$(find "${findings_dir}/out" -path "*/crashes/id*" 2>/dev/null | wc -l)
    if [ "${crash_count}" -gt 0 ]; then
        echo "[FAIL] ${label} — ${crash_count} crash(es) found in ${findings_dir}/out"
        FAIL=$((FAIL + 1))
    else
        echo "[PASS] ${label} — no crashes"
        PASS=$((PASS + 1))
    fi

    rm -rf "${findings_dir}"
}

if ! command -v afl-fuzz &>/dev/null; then
    echo "[SKIP] afl-fuzz not found — install afl++ to enable fuzz testing"
    exit 0
fi

run_fuzz_target "fuzz_dbc_parser" \
    "${FUZZ_BIN_DIR}/fuzz_dbc_parser" \
    "${FUZZ_CORPUS_DBC}"

run_fuzz_target "fuzz_eds_parser" \
    "${FUZZ_BIN_DIR}/fuzz_eds_parser" \
    "${ROOT_DIR}/dbc/corpus"

echo ""
echo "Fuzz: PASS=${PASS} FAIL=${FAIL}"
[ "${FAIL}" -eq 0 ] && exit 0 || exit 1
