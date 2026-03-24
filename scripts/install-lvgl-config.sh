#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DEFAULT_ARDUINO_LIBRARY_DIR="$HOME/Documents/Arduino/libraries"
if [[ ! -d "$DEFAULT_ARDUINO_LIBRARY_DIR" ]]; then
  DEFAULT_ARDUINO_LIBRARY_DIR="$HOME/Arduino/libraries"
fi
ARDUINO_LIBRARY_DIR="${ARDUINO_LIBRARY_DIR:-$DEFAULT_ARDUINO_LIBRARY_DIR}"
SOURCE_CONF="$REPO_DIR/lv_conf.h"
TARGET_CONF="$ARDUINO_LIBRARY_DIR/lv_conf.h"
LVGL_SOURCE_CONF="$ARDUINO_LIBRARY_DIR/lvgl/src/lv_conf.h"

if [[ ! -f "$SOURCE_CONF" ]]; then
  echo "Configurazione LVGL non trovata: $SOURCE_CONF" >&2
  exit 1
fi

mkdir -p "$ARDUINO_LIBRARY_DIR"
cp "$SOURCE_CONF" "$TARGET_CONF"
echo "Configurazione LVGL copiata in $TARGET_CONF"

if [[ -d "$(dirname "$LVGL_SOURCE_CONF")" ]]; then
  cp "$SOURCE_CONF" "$LVGL_SOURCE_CONF"
  echo "Configurazione LVGL copiata in $LVGL_SOURCE_CONF"
fi
