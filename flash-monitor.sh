#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/flash-common.sh"

BAUDRATE="115200"
PORT_WAIT_SECONDS=15
PORT_OVERRIDE=""
FLASH_ARGS=()

usage() {
  cat <<'EOF'
Uso:
  ./flash-monitor.sh [opzioni flash] [--port /dev/...]

Esempi:
  ./flash-monitor.sh
  ./flash-monitor.sh --clean
  ./flash-monitor.sh --upload-only
  ./flash-monitor.sh --port /dev/cu.usbmodem14301
EOF
}

while (( $# > 0 )); do
  case "$1" in
    --port)
      shift
      PORT_OVERRIDE="${1:-}"
      if [[ -z "$PORT_OVERRIDE" ]]; then
        echo "Valore mancante per --port"
        exit 1
      fi
      FLASH_ARGS+=("--port" "$PORT_OVERRIDE")
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      FLASH_ARGS+=("$1")
      ;;
  esac
  shift
done

echo "Compilo e carico lo sketch..."
"$SCRIPT_DIR/flash.sh" "${FLASH_ARGS[@]}"

echo "Attendo il riaggancio della porta seriale..."
PORT="$(wait_for_port "$PORT_OVERRIDE" "$PORT_WAIT_SECONDS" || true)"
if [[ -z "$PORT" ]]; then
  echo "Porta seriale non trovata dopo il flash."
  echo "Controlla con: arduino-cli board list"
  exit 1
fi

if check_port_busy "$PORT"; then
  echo "Porta seriale occupata: $PORT"
  print_port_owner "$PORT" || true
  owner_pid="$(port_owner_pid "$PORT" || true)"
  if [[ -n "$owner_pid" ]]; then
    echo "Per liberarla puoi provare: kill $owner_pid"
  fi
  exit 1
fi

echo "Apro il monitor seriale su $PORT a $BAUDRATE baud..."
echo "Per uscire premi Ctrl+C."
arduino-cli monitor -p "$PORT" -c baudrate="$BAUDRATE"
