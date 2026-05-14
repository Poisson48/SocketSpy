#!/bin/bash
# run.sh — pull, build if needed, then launch SocketSpy
set -e
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

# ── Pull latest ──────────────────────────────────────────────────────────────
echo "[1/3] Pulling latest…"
git pull --ff-only

# ── Build (only if sources changed since last binary) ────────────────────────
BINARY="$ROOT/build/dev/gui/socketspy"
LAST_SRC=$(find gui core dbc scripting protocols api -name "*.cpp" -o -name "*.h" -o -name "*.qss" -o -name "*.qrc" 2>/dev/null | xargs stat -c %Y 2>/dev/null | sort -n | tail -1)
LAST_BIN=$(stat -c %Y "$BINARY" 2>/dev/null || echo 0)

if [ "$LAST_SRC" -gt "$LAST_BIN" ] || [ ! -x "$BINARY" ]; then
    echo "[2/3] Building…"
    bash build.sh
else
    echo "[2/3] Binary up-to-date, skipping build."
fi

# ── Launch ───────────────────────────────────────────────────────────────────
echo "[3/3] Launching SocketSpy…"
exec "$BINARY"
