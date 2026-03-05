#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
echo "===== $(date) ====="
cmake --build build -j
echo "===== RUN ====="
clear &
./build/memory_emu &
app_pid=$!
cleanup() {
  kill -TERM "$app_pid" 2>/dev/null || true
  wait "$app_pid" 2>/dev/null || true
}
trap cleanup INT TERM EXIT
wait "$app_pid"
