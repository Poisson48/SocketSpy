# Scripts SocketSpy

Référence de tous les scripts du projet, organisés par groupe.

---

## Racine

### `build.sh`

Compile SocketSpy via CMake + Ninja. Détecte automatiquement l'architecture (x86\_64, aarch64, armv7l) et sélectionne le preset approprié. Requiert CMake ≥ 3.28 et Ninja.

| Flag | Effet |
|------|-------|
| _(aucun)_ | Build incrémental en mode Debug (`dev` ou `arm64-dev`) |
| `--release` | Build en mode Release (`release` ou `arm64-release`) |
| `--clean` | Supprime le répertoire de build avant de compiler |

```bash
bash build.sh                   # build debug
bash build.sh --release         # build release
bash build.sh --clean           # nettoie puis build debug
bash build.sh --clean --release
```

---

### `run.sh`

Pull, compile si nécessaire, puis lance l'application. Le rebuild est déclenché uniquement si un fichier source (`.cpp`, `.h`, `.qss`, `.qrc`) est plus récent que le binaire existant.

Aucun flag. Toujours en mode `dev`.

```bash
bash run.sh
```

---

### `push.sh`

Wrapper `git push` qui configure l'identité git (`Crevette Etincelante / leohaize@etik.com`) avant de pousser. Transmet tous les arguments reçus à `git push`.

```bash
bash push.sh                    # push la branche courante
bash push.sh --force-with-lease
bash push.sh origin main
```

> Toujours utiliser ce script plutôt que `git push` directement pour garantir la bonne identité auteur.

---

## `scripts/dev/`

### `bootstrap.sh`

Setup complet de l'environnement de développement en une seule commande : installation des dépendances, configuration des interfaces vcan, compilation, et exécution des tests. Affiche un résumé PASS/FAIL à la fin.

Aucun flag.

```bash
bash scripts/dev/bootstrap.sh
```

---

### `install_deps.sh`

Installe toutes les dépendances système (build + runtime). Détecte la distribution via `/etc/os-release` et utilise le gestionnaire de paquets approprié.

| Distribution | Gestionnaire |
|-------------|-------------|
| Ubuntu, Debian, Linux Mint, Pop!\_OS | `apt-get` |
| Fedora, RHEL, CentOS, Rocky, Alma | `dnf` |
| Arch, Manjaro, EndeavourOS | `pacman` |
| Autre | `apt-get` (tentative) |

Paquets obligatoires : `cmake`, `ninja`, Qt6 (base, charts, serialbus, serialport, tools), `lua5.4`, `nlohmann-json`.  
Paquets optionnels : `spdlog`, `cpp-httplib`, `gtest`, `can-utils`.

Vérifie et affiche le résultat d'installation via `pkg-config` en fin d'exécution.

Aucun flag.

```bash
bash scripts/dev/install_deps.sh
```

---

### `install_qt_static.sh`

Installe Qt 6.6.3 via `aqtinstall` (Python) dans `~/Qt/`. Nécessaire uniquement pour la cible AppImage (`cmake --preset release`). Inutile pour le développement courant.

Modules installés : `qtcharts`, `qtserialbus`, `qtserialport`, `qtdeclarative`.

Aucun flag.

```bash
bash scripts/dev/install_qt_static.sh

# Puis pour le build release :
cmake --preset release -DCMAKE_PREFIX_PATH=~/Qt/6.6.3/gcc_64
# ou :
export Qt6_DIR=~/Qt/6.6.3/gcc_64/lib/cmake/Qt6
```

---

### `setup_vcan.sh`

Charge le module noyau `vcan` et crée/active les interfaces `vcan0` et `vcan1`. Idempotent : si les interfaces existent déjà, la commande `ip link add` est ignorée silencieusement.

Requiert `sudo`. Aucun flag.

```bash
bash scripts/dev/setup_vcan.sh
# → "vcan0 vcan1 ready"
```

---

### `run_tests.sh`

Lance la suite de tests complète : build des deux presets (`dev` et `ci`), tests unitaires via ctest, tests d'intégration, fuzz AFL++ (60 s par cible), et analyse mémoire Valgrind sur les tests core.

- Appelle `setup_vcan.sh` automatiquement.
- Le preset `ci` est ignoré silencieusement s'il n'est pas configuré.
- Valgrind exclut `VcanTest*` (chemins noyau) et `*Stress*` (boucles lentes). Timeout 120 s.
- `afl-fuzz` et `valgrind` sont ignorés silencieusement s'ils sont absents.

Aucun flag.

```bash
bash scripts/dev/run_tests.sh
```

---

### `run_integration.sh`

Exécute tous les scripts `tests/integration/*.sh` en leur passant le chemin du binaire `build/dev/socketspy` comme premier argument. Requiert que `vcan0` et `vcan1` soient disponibles (sinon SKIP).

Aucun flag.

```bash
bash scripts/dev/run_integration.sh
```

---

### `run_fuzz.sh`

Lance deux cibles AFL++ pendant 60 secondes chacune et vérifie l'absence de crash.

| Cible | Corpus utilisé |
|-------|---------------|
| `fuzz_dbc_parser` | `dbc/corpus/` |
| `fuzz_eds_parser` | `dbc/corpus/` |

Ignoré silencieusement si `afl-fuzz` n'est pas installé. Les répertoires de findings temporaires sont nettoyés après chaque run.

Aucun flag.

```bash
bash scripts/dev/run_fuzz.sh
```

---

## `scripts/dist/`

### `build_appimage.sh`

Produit un AppImage autonome `SocketSpy-x86_64.AppImage` à la racine du projet. Enchaîne cinq étapes :

1. Build release (`cmake --preset release`)
2. Téléchargement de `linuxdeploy` et son plugin Qt6, mis en cache dans `build/appimage-tools/`
3. Préparation de l'`AppDir` (binaire, `.desktop`, icône)
4. Bundling des dépendances Qt6 via `linuxdeploy` (≈ 1 min), plugins embarqués : `charts`, `serialbus`, `serialport`
5. Rapport de la taille finale

Requiert `qmake6` (ou `qmake`) dans le PATH.

Aucun flag.

```bash
bash scripts/dist/build_appimage.sh

# Puis pour exécuter l'AppImage :
chmod +x SocketSpy-x86_64.AppImage
./SocketSpy-x86_64.AppImage
```

---

## `scripts/demos/`

Répertoire contenant des scripts **Lua** de démonstration. Ces scripts s'exécutent depuis l'interface de scripting intégrée de SocketSpy, pas en ligne de commande.

| Fichier | Rôle |
|---------|------|
| `ligier_pulse3/contactor_boot.lua` | Séquence de boot du contacteur (véhicule Ligier Pulse 3) |
| `ligier_pulse3/battery_scan.lua` | Scan batterie CAN (véhicule Ligier Pulse 3) |

---

## `scripts/tools/`

Répertoire contenant des scripts **Lua** utilitaires. Ces scripts s'exécutent depuis l'interface de scripting intégrée de SocketSpy, pas en ligne de commande.

| Fichier | Rôle |
|---------|------|
| `delta_detector.lua` | Détection de deltas entre trames CAN |
| `canopen_node_scan.lua` | Scan de nœuds sur un bus CANopen |
