# dev/ – lokale Dev-Umgebung für EduMPI

Simulierter Slurm-Cluster mit EduMPI und TimescaleDB in Containern – zum Entwickeln und Testen von
Backend und GUI auf dem eigenen Rechner. Die native Installation und die Anbindung an echte Cluster
bleiben unverändert. Zeiten/Performance aus dem Container sind nicht für Evaluationen geeignet.

Läuft mit Docker (ab 28.0) oder rootless Podman (ab 4.3), auf Linux und macOS inkl. Apple Silicon.
`dev/compose.sh` erkennt die Engine selbst und ruft Compose mit den passenden Dateien auf – immer darüber
starten, nicht direkt mit `docker compose`.

## Aufbau

| Service | Rolle |
| --- | --- |
| `dev` | Build-Container (Autotools, gcc/gfortran, libpq, bear, clangd, Sphinx); Repo unter `/workspaces/EduMPI`, VS Code hängt sich hier an |
| `login` | Slurm-Controller + SSH-Login (`127.0.0.1:2222`) |
| `node1`…`node4` | Slurm-Rechenknoten, je 4 CPUs |
| `db` | TimescaleDB, Schema `EduStore/edustore_schema.sql`, `127.0.0.1:5432` |

Alle Container außer `db` nutzen dasselbe Image. Gemeinsame Volumes wie auf einem echten Cluster:
`/opt/edumpi` (EduMPI-Installation, gebaut im `dev`-Container) und `/home/dev` (Home des Nutzers `dev`).
Partition `all` (wie von der GUI erwartet), Nutzer `dev`.

## Schnellstart

```sh
git submodule update --init --recursive        # einmalig nach dem Klonen
dev/compose.sh up -d --build                   # Image bauen, Cluster starten
dev/compose.sh exec login sinfo                # 4 Nodes "idle"?
dev/compose.sh exec -u vscode dev dev/build-edumpi.sh   # EduMPI bauen → /opt/edumpi (dauert)
```

`dev/build-edumpi.sh` setzt `-DENABLE_ANALYSIS` und linkt `libpq`, sonst entsteht ein Open MPI ohne
EduMPI-Messung. Danach sehen alle Nodes sofort den neuen Stand; Neubau nach Code-Änderungen genauso.
`bear` erzeugt `compile_commands.json` im Repo-Root für clangd.

### Einstellungen (Umgebungsvariablen vor `compose.sh` bzw. `activate`)

| Variable | Standard | Wofür |
| --- | --- | --- |
| `EDUMPI_DEV_SSH_PORT` | `2222` | SSH-Port auf dem Rechner; muss beim Start **und** beim `source dev/ssh/activate` gleich sein |
| `EDUMPI_DB_PORT` | `5432` | DB-Port auf dem Rechner, z. B. `5433`, wenn schon ein lokales PostgreSQL läuft; in der GUI denselben Port eintragen |
| `EDUMPI_TZ` | Zeitzone des Rechners | Zeitzone im Cluster; die GUI vergleicht Job-Endzeiten mit ihrer Ortszeit |
| `EDUMPI_ENGINE` | automatisch | `docker` oder `podman` erzwingen |

Podman: `compose.sh` lädt zusätzlich `compose.podman.yaml` (bildet den eigenen Nutzer im `dev`-Container
auf `vscode` ab, damit der Build ins Repo schreiben darf). Für VS Code mit Podman
`dev.containers.dockerPath` auf `podman` setzen und `compose.podman.yaml` in `dockerComposeFile` ergänzen.
SELinux (Fedora/RHEL): Repo und Schema werden mit `label=disable` eingebunden, ohne Umlabeln auf dem Host.

## SSH-Zugang (für GUI und Terminal)

`dev/ssh/` enthält einen **festen, unsicheren Dev-Key** (`dev_key`, ohne Passphrase) und feste Host-Keys
des Login-Containers. Der Key taugt nur für diesen lokalen Container und ist absichtlich öffentlich –
nie für etwas anderes verwenden. Es wird nichts in `~/.ssh` geändert.

```sh
source dev/ssh/activate      # nur diese Shell: ssh/scp zu "edumpi-dev" → Login-Container
ssh dev@edumpi-dev sinfo
edumpi_dev_deactivate        # oder Shell schließen
```

`activate` stellt `dev/ssh/bin` vorne in den `PATH`. Die Wrapper `ssh`/`scp` dort leiten nur den Host
`edumpi-dev` um (Port 2222, Dev-Key, feste Host-Key-Prüfung) und ersetzen dabei einen mitgegebenen Port –
die GUI ruft ssh fest mit `-p 22` auf. Alle anderen Hosts gehen unverändert ans echte ssh. Ein `scp`
direkt zwischen `edumpi-dev` und einem anderen Host wird abgelehnt (in zwei Schritten kopieren).

## GUI gegen den Dev-Cluster

GUI aus einer Shell starten, in der `dev/ssh/activate` geladen ist (eigener Build, AppImage; in Qt Creator
oder VS Code `dev/ssh/bin` vorne in den `PATH` der Run-Umgebung setzen). Auf dem Mac aus dem Terminal
starten – ein Start aus dem Finder erbt den `PATH` nicht.

| Formular | Feld | Wert |
| --- | --- | --- |
| Cluster | Hostname | `edumpi-dev` |
| | Benutzer | `dev` |
| | Installationspfad | `/opt/edumpi` |
| Datenbank | Host / Port | `127.0.0.1` / `5432` (bzw. `EDUMPI_DB_PORT`) |
| | DB / Benutzer / Passwort | `edumpi` / `edumpi` / `edumpi` |

Die GUI gibt ihren DB-Host an die MPI-Ranks weiter. In jedem Container zeigt `127.0.0.1:5432` deshalb
per Weiterleitung auf den DB-Container – derselbe Wert funktioniert so auf dem Rechner und auf den Nodes.

Die GUI sendet über `nix-shell -p --run "sbatch …"` ab; im Login-Container gibt es dafür einen kleinen
Ersatz ohne Nix (`container/nix-shell`).

Was geht und was nicht:

| GUI-Modus | Stand |
| --- | --- |
| EduMPI (Messung) | geht; bis 8 Ranks (4 Nodes × 4 CPUs, die GUI fordert 2 CPUs je Rank an) |
| ohne EduMPI | läuft, nutzt aber dieselbe EduMPI-Installation ohne DB-Variablen: nichts wird gemessen, jeder Rank schreibt „One or more environment variables are not set“ in die Ausgabe; Zeiten sind nicht die eines reinen Open MPI |
| OpenMP | geht bis 4 Threads (ein Node) |
| Score-P | geht nicht: Score-P/Scalasca/PAPI sind nicht im Image (`scorep: command not found`) |

## Ohne GUI testen

`examples/edumpi_test.c` erzeugt ca. 15 s lang Punkt-zu-Punkt- (8 B bis 1 MiB, Late Sender),
nicht-blockierende und kollektive Kommunikation; `examples/job.sh` ist der Job, wie ihn die GUI erzeugt.

```sh
source dev/ssh/activate
ssh dev@edumpi-dev 'mkdir -p eduMPI_files'
scp dev/examples/edumpi_test.c dev/examples/job.sh dev@edumpi-dev:eduMPI_files/
ssh dev@edumpi-dev 'cd eduMPI_files && sbatch job.sh'
ssh dev@edumpi-dev 'cat eduMPI_files/slurm-*.out'
dev/compose.sh exec db psql -U edumpi -d edumpi -c 'select * from edumpi_runs'
```

## Wichtig zu wissen

- Nur `MPI_Init` startet das EduMPI-Logging, nicht `MPI_Init_thread`.
- Ist die DB nicht erreichbar oder fehlt das Schema, hängen Ranks > 0 in `MPI_Init` bzw. der Job in
  `MPI_Finalize` (EduMPI prüft Verbindung und Abfragen nicht). Dann `scancel` und DB prüfen.
- Die Run-ID ist die Slurm-Job-ID. Der Slurm-Zustand liegt deshalb in einem Volume; DB und Slurm-Zustand
  nur gemeinsam zurücksetzen, sonst landen neue Läufe in alten Run-IDs.
- Slurm 23.11 (Ubuntu 24.04) läuft ohne cgroups (`slurm/cgroup.conf`), unprivilegiert.
- `slurm/munge.key` ist wie der SSH-Key ein fester, unsicherer Dev-Schlüssel. SSH und DB sind nur auf
  `127.0.0.1` veröffentlicht; Docker vor 28.0 machte solche Ports trotzdem im LAN erreichbar
  (`compose.sh` warnt dann).
- Zeiten: Der Cluster läuft in der Zeitzone des Rechners (`EDUMPI_TZ`), die DB in UTC; EduMPI schreibt
  Start-/Endzeit als Ortszeit, deshalb setzt der Cluster auch `PGTZ`.

## Zurücksetzen / Entfernen

| Was | Befehl |
| --- | --- |
| Container stoppen (Daten bleiben) | `dev/compose.sh stop` |
| DB + Slurm-Zustand zurücksetzen | `dev/compose.sh down` und `docker volume rm edumpi_edumpi-db edumpi_slurm-state` (Podman: `podman volume rm …`) |
| Alles inkl. EduMPI-Installation, Home und eigenem Image | Docker: `dev/compose.sh down -v --rmi local` · Podman: `dev/compose.sh down -v` und `podman rmi edumpi-dev:latest` |
| Fremd-Images (TimescaleDB, Basis-Image) | `docker rmi …` bzw. `podman rmi …` |
