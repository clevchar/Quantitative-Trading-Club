#!/usr/bin/env bash
set -euo pipefail

# Usage: run_parser.sh [path/to/file.gz]
GZ_PATH=${1:-"$(pwd)/01302020.NASDAQ_ITCH50.gz"}

if [ ! -f "$GZ_PATH" ]; then
  echo "File not found: $GZ_PATH" >&2
  exit 2
fi

TMP=$(mktemp -t itch_parser_XXXXXX)
trap 'rm -f "$TMP"' EXIT

echo "Decompressing $GZ_PATH -> $TMP"
gunzip -c "$GZ_PATH" > "$TMP"

BIN="$(dirname "$0")/build/parser/parser_bin"
if [ ! -x "$BIN" ]; then
  # fallback to build location
  BIN="$(pwd)/build/parser/parser_bin"
fi

if [ ! -x "$BIN" ]; then
  echo "parser_bin not found or not executable: $BIN" >&2
  exit 3
fi

echo "Running parser: $BIN $TMP"
"$BIN" "$TMP"
