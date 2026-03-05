#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
watchexec --restart \
  --postpone \
  --stop-signal SIGTERM \
  --stop-timeout 2s \
  --watch src \
  --ignore 'build/**' \
  --ignore '**/*.swp' \
  --ignore '**/*.swo' \
  --ignore '**/*~' \
  --ignore '**/.DS_Store' \
  -- ./run.sh
