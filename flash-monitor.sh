#!/bin/zsh

set -e

PORT="/dev/cu.wchusbserial21440"
BAUDRATE="115200"

echo "Compilo e carico lo sketch..."
./flash.sh

echo "Apro il monitor seriale su $PORT a $BAUDRATE baud..."
echo "Per uscire premi Ctrl+C."
arduino-cli monitor -p "$PORT" -c baudrate="$BAUDRATE"
