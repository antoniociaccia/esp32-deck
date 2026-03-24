#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DEFAULT_ARDUINO_LIBRARY_DIR="$HOME/Documents/Arduino/libraries"
if [[ ! -d "$DEFAULT_ARDUINO_LIBRARY_DIR" ]]; then
  DEFAULT_ARDUINO_LIBRARY_DIR="$HOME/Arduino/libraries"
fi
ARDUINO_LIBRARY_DIR="${ARDUINO_LIBRARY_DIR:-$DEFAULT_ARDUINO_LIBRARY_DIR}"

mkdir -p "$ARDUINO_LIBRARY_DIR"

install_library() {
  local library_name="$1"
  local source_dir="$REPO_DIR/lib/$library_name"
  local target_dir="$ARDUINO_LIBRARY_DIR/$library_name"

  if [[ ! -d "$source_dir" ]]; then
    echo "Libreria repo non trovata: $source_dir" >&2
    exit 1
  fi

  rm -rf "$target_dir"
  mkdir -p "$target_dir"
  cp -R "$source_dir/"* "$target_dir/"
  echo "Installata libreria repo: $library_name"
}

install_library "FT6336U_CTP_Controller"
