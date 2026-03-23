#!/bin/zsh

set -euo pipefail

FQBN="esp32:esp32:esp32s3"
PORT="/dev/cu.usbmodem14401"
BUILD_PATH="/tmp/arduino-news-build"
BUILD_LOG="/tmp/arduino-news-build.log"
SOURCE_MANIFEST="$BUILD_PATH/.source-manifest"
APP_MAX_BYTES="4063232"
CLEAN_BUILD=0

if [[ "${1:-}" == "--clean" ]]; then
  CLEAN_BUILD=1
fi

render_bar() {
  local used="$1"
  local total="$2"
  local label="$3"
  local width=30
  local percent filled empty bar

  percent=$(awk "BEGIN { printf \"%d\", ($used * 100) / $total }")
  filled=$(awk "BEGIN { printf \"%d\", ($used * $width) / $total }")
  empty=$((width - filled))
  bar="$(printf '%*s' "$filled" '' | tr ' ' '#')$(printf '%*s' "$empty" '')"

  printf "%-5s [%s] %3d%%\n" "$label" "$bar" "$percent"
}

echo "Compilo lo sketch..."
mkdir -p "$BUILD_PATH"

CURRENT_MANIFEST=$(find . -maxdepth 1 \( -name '*.ino' -o -name '*.c' -o -name '*.cpp' -o -name '*.h' \) | sort)
PREVIOUS_MANIFEST=""
if [[ -f "$SOURCE_MANIFEST" ]]; then
  PREVIOUS_MANIFEST="$(cat "$SOURCE_MANIFEST")"
fi

if [[ "$CURRENT_MANIFEST" != "$PREVIOUS_MANIFEST" ]]; then
  echo "Sorgenti cambiati: pulizia build cache..."
  CLEAN_BUILD=1
fi

if [[ "$CLEAN_BUILD" -eq 1 ]]; then
  echo "Pulizia build cache..."
  rm -rf "$BUILD_PATH"
  mkdir -p "$BUILD_PATH"
fi

printf "%s\n" "$CURRENT_MANIFEST" > "$SOURCE_MANIFEST"
arduino-cli compile \
  --fqbn "$FQBN" \
  --build-path "$BUILD_PATH" \
  --build-property "upload.maximum_size=$APP_MAX_BYTES" \
  . | tee "$BUILD_LOG"

FLASH_LINE=$(grep -E "Sketch uses|Lo sketch usa" "$BUILD_LOG" | head -n 1 || true)
RAM_LINE=$(grep -E "Global variables use|Le variabili globali usano" "$BUILD_LOG" | head -n 1 || true)
FLASH_BYTES=$(echo "$FLASH_LINE" | sed -E 's/.*Sketch uses ([0-9]+) bytes.*/\1/' )
FLASH_MAX_BYTES=$(echo "$FLASH_LINE" | sed -E 's/.*Maximum is ([0-9]+) bytes.*/\1/' )
RAM_BYTES=$(echo "$RAM_LINE" | sed -E 's/.*Global variables use ([0-9]+) bytes.*/\1/' )
RAM_MAX_BYTES=$(echo "$RAM_LINE" | sed -E 's/.*Maximum is ([0-9]+) bytes.*/\1/' )

if [[ "$FLASH_LINE" == Lo\ sketch\ usa* ]]; then
  FLASH_BYTES=$(echo "$FLASH_LINE" | sed -E 's/.*usa ([0-9]+) byte.*/\1/' )
  FLASH_MAX_BYTES=$(echo "$FLASH_LINE" | sed -E 's/.*massimo è ([0-9]+) byte.*/\1/' )
fi

if [[ "$RAM_LINE" == Le\ variabili\ globali\ usano* ]]; then
  RAM_BYTES=$(echo "$RAM_LINE" | sed -E 's/.*usano ([0-9]+) byte.*/\1/' )
  RAM_MAX_BYTES=$(echo "$RAM_LINE" | sed -E 's/.*massimo è ([0-9]+) byte.*/\1/' )
fi

echo
echo "Riepilogo dimensioni:"
if [ -n "$FLASH_LINE" ]; then
  echo "$FLASH_LINE"
fi
if [ -n "$RAM_LINE" ]; then
  echo "$RAM_LINE"
fi
if [ -n "$FLASH_LINE" ]; then
  printf "Flash: %.2f MB / %.2f MB\n" "$(awk "BEGIN {print $FLASH_BYTES/1000000}")" "$(awk "BEGIN {print $FLASH_MAX_BYTES/1000000}")"
  render_bar "$FLASH_BYTES" "$FLASH_MAX_BYTES" "Flash"
fi
if [ -n "$RAM_LINE" ]; then
  printf "RAM: %.2f MB / %.2f MB\n" "$(awk "BEGIN {print $RAM_BYTES/1000000}")" "$(awk "BEGIN {print $RAM_MAX_BYTES/1000000}")"
  render_bar "$RAM_BYTES" "$RAM_MAX_BYTES" "RAM"
fi

echo "Carico sulla scheda $PORT..."
arduino-cli upload -p "$PORT" --fqbn "$FQBN" --build-path "$BUILD_PATH" .

echo "Upload completato."
