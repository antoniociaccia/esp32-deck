#!/bin/zsh

set -euo pipefail

BAUDRATE="115200"

detect_port() {
  local port=""
  local -a ports

  port=$(arduino-cli board list | awk '/ESP32-S3|esp32s3/ {print $1; exit}')
  if [[ -n "$port" ]]; then
    echo "$port"
    return 0
  fi

  ports=(/dev/cu.usbmodem*(N))
  if (( ${#ports[@]} > 0 )); then
    echo "$ports[1]"
    return 0
  fi

  ports=(/dev/tty.usbmodem*(N))
  if (( ${#ports[@]} > 0 )); then
    echo "$ports[1]"
    return 0
  fi

  return 1
}

echo "Compilo e carico lo sketch..."
./flash.sh "$@"

echo "Attendo il riaggancio della porta seriale..."
PORT=""
for _ in {1..15}; do
  sleep 1
  PORT="$(detect_port || true)"
  if [[ -n "$PORT" ]]; then
    break
  fi
done

if [[ -z "$PORT" ]]; then
  echo "Porta seriale non trovata dopo il flash."
  echo "Controlla con: arduino-cli board list"
  exit 1
fi

echo "Apro il monitor seriale su $PORT a $BAUDRATE baud..."
echo "Per uscire premi Ctrl+C."
arduino-cli monitor -p "$PORT" -c baudrate="$BAUDRATE"
