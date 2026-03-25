#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${TMPDIR:-/tmp}/esp32-deck-parser-tests"
BIN_PATH="$BUILD_DIR/parser-tests"

mkdir -p "$BUILD_DIR"
mkdir -p "$REPO_DIR/tests/include"

if [[ ! -f "$REPO_DIR/tests/include/doctest.h" ]]; then
  echo "ERRORE: doctest.h non trovato."
  echo "Per favore scaricalo eseguendo questo comando:"
  echo "curl -L https://raw.githubusercontent.com/doctest/doctest/master/doctest/doctest.h -o tests/include/doctest.h"
  exit 1
fi

c++ \
  -std=c++17 \
  -Wall \
  -Wextra \
  -I"$REPO_DIR/tests/include" \
  -I"$REPO_DIR/src" \
  -I"$REPO_DIR" \
  "$REPO_DIR/tests/test_main.cpp" \
  "$REPO_DIR/tests/test_weather.cpp" \
  "$REPO_DIR/tests/test_news.cpp" \
  "$REPO_DIR/tests/test_ota.cpp" \
  "$REPO_DIR/tests/test_ui.cpp" \
  "$REPO_DIR/tests/test_network.cpp" \
  "$REPO_DIR/tests/test_services.cpp" \
  "$REPO_DIR/tests/test_utils.cpp" \
  "$REPO_DIR/tests/test_battery.cpp" \
  "$REPO_DIR/src/dashboard_app.cpp" \
  "$REPO_DIR/src/dashboard_ota.cpp" \
  "$REPO_DIR/src/dashboard_services_weather.cpp" \
  "$REPO_DIR/src/dashboard_services_news.cpp" \
  "$REPO_DIR/src/dashboard_services.cpp" \
  "$REPO_DIR/src/dashboard_battery.cpp" \
  -o "$BIN_PATH"

"$BIN_PATH"
