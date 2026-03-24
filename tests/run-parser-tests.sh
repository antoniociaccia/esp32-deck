#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${TMPDIR:-/tmp}/esp32-deck-parser-tests"
BIN_PATH="$BUILD_DIR/parser-tests"

mkdir -p "$BUILD_DIR"

c++ \
  -std=c++17 \
  -Wall \
  -Wextra \
  -I"$REPO_DIR/tests/include" \
  -I"$REPO_DIR/src" \
  -I"$REPO_DIR" \
  "$REPO_DIR/tests/parser_tests.cpp" \
  "$REPO_DIR/src/dashboard_app.cpp" \
  -o "$BIN_PATH"

"$BIN_PATH"
