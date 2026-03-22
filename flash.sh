#!/bin/zsh

set -euo pipefail

FQBN="esp32:esp32:d32_pro:PartitionScheme=no_ota"
PORT="/dev/cu.wchusbserial21440"
BUILD_PATH="/tmp/arduino-news-build"
BUILD_LOG="/tmp/arduino-news-build.log"
FLASH_SIZE="16MB"
APP_MAX_BYTES="8388608"

echo "Compilo lo sketch..."
rm -rf "$BUILD_PATH"
arduino-cli compile --fqbn "$FQBN" --build-property "build.flash_size=$FLASH_SIZE" --build-property "upload.maximum_size=$APP_MAX_BYTES" --build-path "$BUILD_PATH" . | tee "$BUILD_LOG"

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
fi
if [ -n "$RAM_LINE" ]; then
  printf "RAM: %.2f MB / %.2f MB\n" "$(awk "BEGIN {print $RAM_BYTES/1000000}")" "$(awk "BEGIN {print $RAM_MAX_BYTES/1000000}")"
fi

echo "Carico sulla scheda $PORT..."
arduino-cli upload -p "$PORT" --fqbn "$FQBN" --upload-property "build.flash_size=$FLASH_SIZE" --build-path "$BUILD_PATH" .

echo "Upload completato."
