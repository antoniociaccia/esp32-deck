#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SOURCE_SETUP="$REPO_DIR/lib/TFT_eSPI/User_Setup.h"
DEFAULT_ARDUINO_LIBRARY_DIR="$HOME/Documents/Arduino/libraries"
if [[ ! -d "$DEFAULT_ARDUINO_LIBRARY_DIR" ]]; then
  DEFAULT_ARDUINO_LIBRARY_DIR="$HOME/Arduino/libraries"
fi
ARDUINO_LIBRARY_DIR="${ARDUINO_LIBRARY_DIR:-$DEFAULT_ARDUINO_LIBRARY_DIR}"
TARGET_SETUP="$ARDUINO_LIBRARY_DIR/TFT_eSPI/User_Setup.h"

if [[ ! -f "$SOURCE_SETUP" ]]; then
  echo "Setup TFT_eSPI non trovato: $SOURCE_SETUP" >&2
  exit 1
fi

if [[ ! -d "$(dirname "$TARGET_SETUP")" ]]; then
  echo "Libreria TFT_eSPI non trovata in: $(dirname "$TARGET_SETUP")" >&2
  echo "Installa prima TFT_eSPI oppure imposta ARDUINO_LIBRARY_DIR." >&2
  exit 1
fi

cp "$SOURCE_SETUP" "$TARGET_SETUP"
echo "Setup TFT_eSPI applicato a $TARGET_SETUP"
