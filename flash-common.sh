#!/bin/zsh

port_exists() {
  local port="${1:-}"
  [[ -n "$port" && -e "$port" ]]
}

detect_port() {
  local preferred_port="${1:-}"
  local port=""
  local -a ports

  if port_exists "$preferred_port"; then
    echo "$preferred_port"
    return 0
  fi

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

check_port_busy() {
  local port="${1:-}"
  [[ -n "$port" ]] || return 1
  lsof "$port" >/dev/null 2>&1
}

port_owner_pid() {
  local port="${1:-}"
  [[ -n "$port" ]] || return 1
  lsof -t "$port" 2>/dev/null | head -n 1
}

print_port_owner() {
  local port="${1:-}"
  [[ -n "$port" ]] || return 1

  local owner_lines
  owner_lines="$(lsof "$port" 2>/dev/null || true)"
  if [[ -n "$owner_lines" ]]; then
    echo "$owner_lines"
    return 0
  fi

  return 1
}

wait_for_port() {
  local preferred_port="${1:-}"
  local seconds="${2:-5}"
  local port=""

  port="$(detect_port "$preferred_port" || true)"
  if [[ -n "$port" ]]; then
    echo "$port"
    return 0
  fi

  for _ in $(seq 1 "$seconds"); do
    sleep 1
    port="$(detect_port "$preferred_port" || true)"
    if [[ -n "$port" ]]; then
      echo "$port"
      return 0
    fi
  done

  return 1
}
