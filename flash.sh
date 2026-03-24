#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/flash-common.sh"

FQBN="esp32:esp32:esp32s3"
CDC_ON_BOOT="1"
BUILD_JOBS="0"
PORT_WAIT_SECONDS=5
DEFAULT_PARTITIONS_FILE="$SCRIPT_DIR/partitions.csv"
OTA_PARTITIONS_FILE="$SCRIPT_DIR/partitions_ota.csv"
PARTITIONS_FILE="$DEFAULT_PARTITIONS_FILE"
BUILD_PATH_BASE="/tmp/arduino-news-build"
BUILD_PATH="$BUILD_PATH_BASE"
BUILD_LOG="${BUILD_PATH_BASE}.log"
APP_MAX_BYTES=""
SKETCH_PATH="$SCRIPT_DIR"
TEMP_SKETCH_DIR=""

CLEAN_BUILD=0
UPLOAD_ONLY=0
BUILD_ONLY=0
PORT_OVERRIDE=""

usage() {
  cat <<'EOF'
Uso:
  ./flash.sh [--clean] [--build-only] [--upload-only] [--ota-layout] [--partitions-file file.csv] [--port /dev/...] [--jobs N]

Opzioni:
  --clean         pulisce la cache di build prima della compilazione
  --build-only    compila senza fare upload
  --upload-only   salta la compilazione e riusa gli artefatti gia presenti
  --ota-layout    usa il layout OTA in partitions_ota.csv
  --partitions-file PATH
                  usa un file partizioni alternativo per compile e upload
  --port PATH     forza la porta seriale
  --jobs N        numero job paralleli per arduino-cli compile (0 = tutti i core)
EOF
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

refresh_build_paths() {
  local suffix=""

  if [[ "$PARTITIONS_FILE" != "$DEFAULT_PARTITIONS_FILE" ]]; then
    suffix="-$(basename "$PARTITIONS_FILE" .csv)"
  fi

  BUILD_PATH="${BUILD_PATH_BASE}${suffix}"
  BUILD_LOG="${BUILD_PATH}.log"
}

cleanup_transient_state() {
  if [[ -n "$TEMP_SKETCH_DIR" && -d "$TEMP_SKETCH_DIR" ]]; then
    rm -rf "$(dirname "$TEMP_SKETCH_DIR")"
    TEMP_SKETCH_DIR=""
  fi
}

prepare_sketch_path() {
  if [[ "$PARTITIONS_FILE" == "$DEFAULT_PARTITIONS_FILE" ]]; then
    SKETCH_PATH="$SCRIPT_DIR"
    return 0
  fi

  if [[ ! -f "$PARTITIONS_FILE" ]]; then
    echo "File partizioni non trovato: $PARTITIONS_FILE"
    exit 1
  fi

  local temp_parent
  temp_parent="$(mktemp -d "${TMPDIR:-/tmp}/arduino-news-sketch.XXXXXX")"
  TEMP_SKETCH_DIR="$temp_parent/news"
  mkdir -p "$TEMP_SKETCH_DIR"
  shopt -s nullglob
  for entry in "$SCRIPT_DIR"/*; do
    local base
    base="$(basename "$entry")"
    if [[ "$base" == "$(basename "$TEMP_SKETCH_DIR")" || "$base" == "partitions.csv" ]]; then
      continue
    fi
    cp -R "$entry" "$TEMP_SKETCH_DIR/$base"
  done
  shopt -u nullglob

  cp "$PARTITIONS_FILE" "$TEMP_SKETCH_DIR/partitions.csv"
  trap cleanup_transient_state EXIT
  SKETCH_PATH="$TEMP_SKETCH_DIR"
}

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

parse_size_summary() {
  local flash_line ram_line flash_bytes flash_max ram_bytes ram_max

  flash_line=$(grep -E "Sketch uses|Lo sketch usa" "$BUILD_LOG" | head -n 1 || true)
  ram_line=$(grep -E "Global variables use|Le variabili globali usano" "$BUILD_LOG" | head -n 1 || true)
  flash_bytes=$(echo "$flash_line" | sed -E 's/.*Sketch uses ([0-9]+) bytes.*/\1/')
  flash_max=$(echo "$flash_line" | sed -E 's/.*Maximum is ([0-9]+) bytes.*/\1/')
  ram_bytes=$(echo "$ram_line" | sed -E 's/.*Global variables use ([0-9]+) bytes.*/\1/')
  ram_max=$(echo "$ram_line" | sed -E 's/.*Maximum is ([0-9]+) bytes.*/\1/')

  if [[ "$flash_line" == Lo\ sketch\ usa* ]]; then
    flash_bytes=$(echo "$flash_line" | sed -E 's/.*usa ([0-9]+) byte.*/\1/')
    flash_max=$(echo "$flash_line" | sed -E 's/.*massimo è ([0-9]+) byte.*/\1/')
  fi

  if [[ "$ram_line" == Le\ variabili\ globali\ usano* ]]; then
    ram_bytes=$(echo "$ram_line" | sed -E 's/.*usano ([0-9]+) byte.*/\1/')
    ram_max=$(echo "$ram_line" | sed -E 's/.*massimo è ([0-9]+) byte.*/\1/')
  fi

  echo
  echo "Riepilogo dimensioni:"
  [[ -n "$flash_line" ]] && echo "$flash_line"
  [[ -n "$ram_line" ]] && echo "$ram_line"

  if [[ -n "$flash_line" && -n "$flash_bytes" && -n "$flash_max" ]]; then
    printf "Flash: %.2f MB / %.2f MB\n" "$(awk "BEGIN {print $flash_bytes/1000000}")" "$(awk "BEGIN {print $flash_max/1000000}")"
    render_bar "$flash_bytes" "$flash_max" "Flash"
  fi

  if [[ -n "$ram_line" && -n "$ram_bytes" && -n "$ram_max" ]]; then
    printf "RAM: %.2f MB / %.2f MB\n" "$(awk "BEGIN {print $ram_bytes/1000000}")" "$(awk "BEGIN {print $ram_max/1000000}")"
    render_bar "$ram_bytes" "$ram_max" "RAM"
  fi
}

compile_sketch() {
  echo "Compilo lo sketch..."
  mkdir -p "$BUILD_PATH"

  if [[ "$CLEAN_BUILD" -eq 1 ]]; then
    echo "Pulizia build cache..."
    rm -rf "$BUILD_PATH"
    mkdir -p "$BUILD_PATH"
  fi

  arduino-cli compile \
    --fqbn "$FQBN" \
    --build-path "$BUILD_PATH" \
    --jobs "$BUILD_JOBS" \
    --build-property "upload.maximum_size=$APP_MAX_BYTES" \
    --build-property "build.cdc_on_boot=$CDC_ON_BOOT" \
    "$SKETCH_PATH" | tee "$BUILD_LOG"

  parse_size_summary
}

ensure_upload_artifacts() {
  if [[ ! -d "$BUILD_PATH" ]]; then
    echo "Build path non trovato: $BUILD_PATH"
    echo "Esegui prima ./flash.sh oppure rimuovi --upload-only"
    exit 1
  fi

  local -a bins=()
  shopt -s nullglob
  bins=("$BUILD_PATH"/*.bin)
  shopt -u nullglob
  if (( ${#bins[@]} == 0 )); then
    echo "Artefatti di build non trovati in $BUILD_PATH"
    echo "Esegui prima ./flash.sh oppure rimuovi --upload-only"
    exit 1
  fi
}

resolve_port() {
  local port=""

  if [[ -n "$PORT_OVERRIDE" ]]; then
    if ! port_exists "$PORT_OVERRIDE"; then
      echo "Porta seriale non trovata: $PORT_OVERRIDE"
      exit 1
    fi
    echo "$PORT_OVERRIDE"
    return 0
  fi

  echo "Attendo la porta seriale per ${PORT_WAIT_SECONDS}s..." >&2
  port="$(wait_for_port "" "$PORT_WAIT_SECONDS" || true)"
  if [[ -z "$port" ]]; then
    echo "Porta seriale non trovata." >&2
    echo "Controlla con: arduino-cli board list" >&2
    exit 1
  fi

  echo "$port"
}

ensure_port_available() {
  local port="$1"

  if ! port_exists "$port"; then
    echo "Porta seriale assente: $port"
    echo "Controlla con: arduino-cli board list"
    exit 1
  fi

  if check_port_busy "$port"; then
    echo "Porta seriale occupata: $port"
    print_port_owner "$port" || true
    local owner_pid
    owner_pid="$(port_owner_pid "$port" || true)"
    if [[ -n "$owner_pid" ]]; then
      echo "Per liberarla puoi provare: kill $owner_pid"
    fi
    exit 1
  fi
}

while (( $# > 0 )); do
  case "$1" in
    --clean)
      CLEAN_BUILD=1
      ;;
    --upload-only)
      UPLOAD_ONLY=1
      ;;
    --build-only)
      BUILD_ONLY=1
      ;;
    --ota-layout)
      PARTITIONS_FILE="$OTA_PARTITIONS_FILE"
      ;;
    --partitions-file)
      shift
      PARTITIONS_FILE="${1:-}"
      if [[ -z "$PARTITIONS_FILE" ]]; then
        echo "Valore mancante per --partitions-file"
        exit 1
      fi
      if [[ "$PARTITIONS_FILE" != /* ]]; then
        PARTITIONS_FILE="$SCRIPT_DIR/$PARTITIONS_FILE"
      fi
      ;;
    --port)
      shift
      PORT_OVERRIDE="${1:-}"
      if [[ -z "$PORT_OVERRIDE" ]]; then
        echo "Valore mancante per --port"
        exit 1
      fi
      ;;
    --jobs)
      shift
      BUILD_JOBS="${1:-}"
      if [[ -z "$BUILD_JOBS" ]]; then
        echo "Valore mancante per --jobs"
        exit 1
      fi
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "Opzione non riconosciuta: $1"
      usage
      exit 1
      ;;
  esac
  shift
done

if [[ ! -f "$PARTITIONS_FILE" ]]; then
  echo "File partizioni non trovato: $PARTITIONS_FILE"
  exit 1
fi

refresh_build_paths
APP_MAX_BYTES="$(resolve_app_max_bytes "$PARTITIONS_FILE")"
prepare_sketch_path

if [[ "$UPLOAD_ONLY" -eq 1 && "$BUILD_ONLY" -eq 1 ]]; then
  echo "Non puoi usare insieme --upload-only e --build-only"
  exit 1
fi

if [[ "$UPLOAD_ONLY" -eq 0 ]]; then
  compile_sketch
else
  ensure_upload_artifacts
fi

if [[ "$BUILD_ONLY" -eq 1 ]]; then
  echo "Build completata, upload saltato."
  exit 0
fi

PORT="$(resolve_port)"
ensure_port_available "$PORT"

echo "Carico sulla scheda $PORT..."
arduino-cli upload -p "$PORT" --fqbn "$FQBN" --build-path "$BUILD_PATH" "$SKETCH_PATH"

echo "Upload completato."
