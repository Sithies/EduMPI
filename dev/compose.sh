#!/usr/bin/env bash
# Ruft Compose für die Dev-Umgebung auf – mit Docker oder Podman, je nachdem was da ist.
#   dev/compose.sh up -d --build
#   dev/compose.sh exec -u vscode dev bash
# Erzwingen: EDUMPI_ENGINE=docker|podman dev/compose.sh …
set -euo pipefail

FILE="$(cd "$(dirname "$0")" && pwd)/compose.yaml"

use_docker() { docker info >/dev/null 2>&1; }
use_podman() { command -v podman >/dev/null 2>&1; }

engine="${EDUMPI_ENGINE:-}"
if [[ -z "$engine" ]]; then
    if use_docker; then engine=docker
    elif use_podman; then engine=podman
    else
        echo "Weder Docker (mit laufendem Daemon) noch Podman gefunden." >&2
        exit 1
    fi
fi

case "$engine" in
    docker)
        if docker compose version >/dev/null 2>&1; then
            exec docker compose -f "$FILE" "$@"
        fi
        exec docker-compose -f "$FILE" "$@"
        ;;
    podman)
        # Rootless Podman: eigene UID im Container behalten, sonst gehört das
        # eingebundene Repo im Container root und der Nutzer vscode kann nicht schreiben
        export PODMAN_USERNS="${PODMAN_USERNS:-keep-id}"
        if command -v podman-compose >/dev/null 2>&1; then
            exec podman-compose -f "$FILE" "$@"
        fi
        exec podman compose -f "$FILE" "$@"
        ;;
    *)
        echo "Unbekannte Engine: $engine (docker oder podman)" >&2
        exit 1
        ;;
esac
