# dev/ – Dev-Umgebung für EduMPI

Reine Entwicklungs- und Testumgebung. Die native Installation und die Anbindung an echte Cluster bleiben
unverändert. Zeiten/Performance aus dem Container sind nicht für Evaluationen geeignet.

## Stand: Ausbaustufe 1

| Service | Inhalt |
| --- | --- |
| `dev` | Build-Toolchain (Autotools, gcc/gfortran, libpq, bear, clangd, Sphinx), Repo unter `/workspaces/EduMPI` |
| `db` | TimescaleDB (offizielles Image), Schema `EduStore/edustore_schema.sql` als Init-Skript, `127.0.0.1:5432` |

Geplant: Slurm (login + 4 Nodes), SSH-Login für die GUI, Multi-Stage mit schlankem Runtime-Image.

## Benutzung

Mit VS Code: Repo öffnen → „Dev Containers: Reopen in Container“ (`.devcontainer/` verweist hierher).

Ohne VS Code: `dev/compose.sh` nimmt Docker, wenn ein Docker-Daemon läuft, sonst Podman
(erzwingen mit `EDUMPI_ENGINE=docker|podman`). Alle Argumente gehen direkt an `compose`.

```sh
dev/compose.sh up -d --build
git submodule update --init --recursive
dev/compose.sh exec -u vscode dev dev/build-edumpi.sh
dev/compose.sh exec db psql -U edumpi -d edumpi
```

Podman läuft rootless mit `PODMAN_USERNS=keep-id` (setzt `compose.sh`), damit das eingebundene Repo im
Container dem eigenen Nutzer gehört. Für VS Code mit Podman in den Einstellungen
`dev.containers.dockerPath` auf `podman` setzen.

`dev/build-edumpi.sh` setzt `-DENABLE_ANALYSIS` und linkt `libpq` (`LIBS=-lpq`), sonst entsteht ein
Open MPI ohne EduMPI-Messung. Installiert wird nach `$EDUMPI_PREFIX` (im Container `~/edumpi-install`);
`bear` erzeugt `compile_commands.json` im Repo-Root für clangd.

## DB

| | |
| --- | --- |
| Host (vom Rechner / GUI) | `127.0.0.1:5432` |
| Host (aus dem Container) | `db:5432` |
| DB / User / Passwort | `edumpi` / `edumpi` / `edumpi` (nur lokal) |

Das Schema wird nur beim ersten Start mit leerem Volume geladen.

## Zurücksetzen / Entfernen

| Was | Befehl |
| --- | --- |
| Container stoppen (Daten bleiben) | `dev/compose.sh stop` |
| DB zurücksetzen (Volume löschen) | `dev/compose.sh down -v` |
| Alles inkl. eigenem Image entfernen | `dev/compose.sh down -v --rmi local` |
| Fremd-Images (TimescaleDB, Basis-Image) | `docker rmi …` bzw. `podman rmi …` |
