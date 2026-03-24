#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
VERSION_FILE="$REPO_DIR/src/version.h"
DEFAULT_BRANCH="${DEFAULT_BRANCH:-main}"
PUSH_ENABLED=1

usage() {
  cat <<'EOF'
Uso:
  bash ./scripts/release-tag.sh [--no-push]

Descrizione:
  Legge FW_VERSION da src/version.h, crea il tag vX.Y.Z e,
  per default, esegue push di branch e tag.

Opzioni:
  --no-push   crea il tag solo in locale
EOF
}

read_version() {
  sed -n 's/.*FW_VERSION = "\([^"]*\)".*/\1/p' "$VERSION_FILE" | head -n 1
}

require_clean_worktree() {
  if [[ -n "$(git -C "$REPO_DIR" status --short)" ]]; then
    echo "Worktree non pulito. Committa o stasha prima di creare la release." >&2
    exit 1
  fi
}

require_up_to_date_branch() {
  git -C "$REPO_DIR" fetch origin "$DEFAULT_BRANCH" --quiet

  local local_sha remote_sha
  local_sha="$(git -C "$REPO_DIR" rev-parse "$DEFAULT_BRANCH")"
  remote_sha="$(git -C "$REPO_DIR" rev-parse "origin/$DEFAULT_BRANCH")"

  if [[ "$local_sha" != "$remote_sha" ]]; then
    echo "Il branch locale $DEFAULT_BRANCH non è allineato con origin/$DEFAULT_BRANCH." >&2
    echo "Esegui prima push o pull e riprova." >&2
    exit 1
  fi
}

main() {
  while (( $# > 0 )); do
    case "$1" in
      --no-push)
        PUSH_ENABLED=0
        ;;
      -h|--help)
        usage
        exit 0
        ;;
      *)
        echo "Opzione non riconosciuta: $1" >&2
        usage >&2
        exit 1
        ;;
    esac
    shift
  done

  if [[ ! -f "$VERSION_FILE" ]]; then
    echo "File versione non trovato: $VERSION_FILE" >&2
    exit 1
  fi

  local version tag_name
  version="$(read_version)"
  if [[ -z "$version" ]]; then
    echo "FW_VERSION non trovata in $VERSION_FILE" >&2
    exit 1
  fi

  tag_name="v${version}"

  require_clean_worktree

  if git -C "$REPO_DIR" rev-parse -q --verify "refs/tags/$tag_name" >/dev/null; then
    echo "Il tag $tag_name esiste già." >&2
    exit 1
  fi

  if [[ "$PUSH_ENABLED" -eq 1 ]]; then
    require_up_to_date_branch
  fi

  git -C "$REPO_DIR" tag "$tag_name"
  echo "Creato tag locale: $tag_name"

  if [[ "$PUSH_ENABLED" -eq 1 ]]; then
    git -C "$REPO_DIR" push origin "$DEFAULT_BRANCH"
    git -C "$REPO_DIR" push origin "$tag_name"
    echo "Push completato: $DEFAULT_BRANCH + $tag_name"
    echo "Il workflow Release OTA dovrebbe partire su GitHub."
  else
    echo "Tag creato solo in locale."
  fi
}

main "$@"
