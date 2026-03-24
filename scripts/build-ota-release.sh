#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
FQBN="${FQBN:-esp32:esp32:esp32s3}"
BUILD_ROOT="${BUILD_ROOT:-$REPO_DIR/.build/ota-release}"
TEMP_PARENT=""
TEMP_SKETCH_DIR=""
BUILD_PATH="$BUILD_ROOT/build"
PARTITIONS_FILE="$REPO_DIR/partitions_ota.csv"
OTA_MANIFEST_NAME="${OTA_MANIFEST_NAME:-manifest-stable.json}"
OTA_MIN_BATTERY_PERCENT="${OTA_MIN_BATTERY_PERCENT:-30}"

read_version_field() {
  local constant_name="$1"
  sed -n "s/.*${constant_name} = \"\\([^\"]*\\)\".*/\\1/p" "$REPO_DIR/src/version.h" | head -n 1
}

resolve_app_max_bytes() {
  local partitions_file="$1"
  local name type subtype offset size flags size_token

  while IFS=, read -r name type subtype offset size flags; do
    type="${type//[[:space:]]/}"
    if [[ "$type" != "app" ]]; then
      continue
    fi

    size_token="${size//[[:space:]]/}"
    if [[ -n "$size_token" ]]; then
      echo $((size_token))
      return 0
    fi
  done < "$partitions_file"

  echo "Impossibile determinare la dimensione app da: $partitions_file" >&2
  exit 1
}

cleanup() {
  if [[ -n "$TEMP_PARENT" && -d "$TEMP_PARENT" ]]; then
    rm -rf "$TEMP_PARENT"
  fi
}

copy_repo_to_temp_sketch() {
  TEMP_PARENT="$(mktemp -d "${TMPDIR:-/tmp}/esp32-deck-ota.XXXXXX")"
  TEMP_SKETCH_DIR="$TEMP_PARENT/news"
  mkdir -p "$TEMP_SKETCH_DIR"

  shopt -s dotglob nullglob
  for entry in "$REPO_DIR"/*; do
    local base
    base="$(basename "$entry")"
    case "$base" in
      .git|.build|docs|secrets.h|partitions.csv)
        continue
        ;;
    esac
    cp -R "$entry" "$TEMP_SKETCH_DIR/$base"
  done
  shopt -u dotglob nullglob

  cp "$PARTITIONS_FILE" "$TEMP_SKETCH_DIR/partitions.csv"
}

write_stub_secrets() {
  cat > "$TEMP_SKETCH_DIR/secrets.h" <<'EOF'
#ifndef SECRETS_H
#define SECRETS_H

static const char *WIFI_SSID = "";
static const char *WIFI_PASSWORD = "";

static const char *NTP_SERVER_1 = "pool.ntp.org";
static const char *NTP_SERVER_2 = "time.nist.gov";
static const char *TZ_INFO = "CET-1CEST,M3.5.0/2,M10.5.0/3";

static const char *OPENWEATHER_API_KEY = "";
static const char *WEATHER_CITY_LABEL = "OTA";
static constexpr float WEATHER_LATITUDE = 0.0f;
static constexpr float WEATHER_LONGITUDE = 0.0f;

static const char *NEWS_API_URL = "";
static const char *NEWS_API_KEY = "";
static const char *OTA_MANIFEST_URL = "";

#endif
EOF
}

generate_manifest() {
  local board_id="$1"
  local version="$2"
  local channel="$3"
  local tag_name="$4"
  local bin_name="$5"
  local sha256="$6"
  local size_bytes="$7"
  local notes="$8"
  local bin_base_url="${OTA_BIN_BASE_URL:-https://github.com/example/example/releases/download/${tag_name}}"

  cat > "$BUILD_ROOT/$OTA_MANIFEST_NAME" <<EOF
{
  "channel": "${channel}",
  "board": "${board_id}",
  "version": "${version}",
  "build": "${tag_name}",
  "bin_url": "${bin_base_url}/${bin_name}",
  "sha256": "${sha256}",
  "size": ${size_bytes},
  "min_battery_percent": ${OTA_MIN_BATTERY_PERCENT},
  "notes": "${notes}"
}
EOF
}

main() {
  trap cleanup EXIT

  local version channel board_id tag_name app_max_bytes bin_name source_bin sha256 size_bytes notes
  version="$(read_version_field FW_VERSION)"
  channel="$(read_version_field FW_RELEASE_CHANNEL)"
  board_id="$(read_version_field FW_BOARD_ID)"
  tag_name="${OTA_TAG_NAME:-v${version}}"
  app_max_bytes="$(resolve_app_max_bytes "$PARTITIONS_FILE")"
  bin_name="esp32-deck-${board_id}-${tag_name}.bin"
  notes="GitHub Release ${tag_name}"

  rm -rf "$BUILD_ROOT"
  mkdir -p "$BUILD_ROOT"

  bash "$REPO_DIR/scripts/install-lvgl-config.sh"
  bash "$REPO_DIR/scripts/install-repo-libraries.sh"
  bash "$REPO_DIR/scripts/apply-tftespi-setup.sh"

  copy_repo_to_temp_sketch
  write_stub_secrets

  arduino-cli compile \
    --fqbn "$FQBN" \
    --build-path "$BUILD_PATH" \
    --jobs 0 \
    --build-property "upload.maximum_size=$app_max_bytes" \
    --build-property "build.cdc_on_boot=1" \
    "$TEMP_SKETCH_DIR"

  source_bin="$BUILD_PATH/news.ino.bin"
  if [[ ! -f "$source_bin" ]]; then
    echo "Binary OTA non trovato: $source_bin" >&2
    exit 1
  fi

  cp "$source_bin" "$BUILD_ROOT/$bin_name"
  sha256="$(shasum -a 256 "$BUILD_ROOT/$bin_name" | awk '{print $1}')"
  size_bytes="$(wc -c < "$BUILD_ROOT/$bin_name" | tr -d '[:space:]')"

  generate_manifest "$board_id" "$version" "$channel" "$tag_name" "$bin_name" "$sha256" "$size_bytes" "$notes"

  echo "OTA binary: $BUILD_ROOT/$bin_name"
  echo "OTA manifest: $BUILD_ROOT/$OTA_MANIFEST_NAME"
}

main "$@"
