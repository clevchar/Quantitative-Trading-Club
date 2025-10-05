#!/usr/bin/env bash
set -euo pipefail

# Usage: run_simulator.sh [path/to/file.gz] [dest_ip] [dest_port]
GZ_PATH=${1:-"$(pwd)/01302020.NASDAQ_ITCH50.gz"}
DEST_IP=${2:-127.0.0.1}
DEST_PORT=${3:-9000}

if [ ! -f "$GZ_PATH" ]; then
  echo "File not found: $GZ_PATH" >&2
  exit 2
fi

TMP=$(mktemp -t itch_sim_XXXXXX)
trap 'rm -f "$TMP"' EXIT

echo "Decompressing $GZ_PATH -> $TMP"
gunzip -c "$GZ_PATH" > "$TMP"

BIN="$(dirname "$0")/build/simulator/simulator_bin"
if [ ! -x "$BIN" ]; then
  BIN="$(pwd)/build/simulator/simulator_bin"
fi

if [ ! -x "$BIN" ]; then
  echo "simulator_bin not found or not executable: $BIN" >&2
  exit 3
fi

echo "Running simulator: $BIN $TMP $DEST_IP $DEST_PORT --burst 1400"
"$BIN" "$TMP" "$DEST_IP" "$DEST_PORT" --burst 1400
