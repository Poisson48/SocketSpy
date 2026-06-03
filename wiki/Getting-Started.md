# Getting Started — Prise en main

> **Language / Langue:** [English](#english) · [Français](#français)
> **Navigation:** [Home](Home.md) · [Features](Features.md) · [API & MCP](Api-MCP.md) · [Scripting](Scripting.md) · [FAQ](FAQ.md)

---

<a name="english"></a>
## English

### 1. Download the AppImage (fastest)

No installation required — the AppImage is self-contained:

```bash
# Grab the latest release
wget https://github.com/Poisson48/SocketSpy/releases/latest/download/SocketSpy-x86_64.AppImage
chmod +x SocketSpy-x86_64.AppImage
./SocketSpy-x86_64.AppImage
```

Requires Linux x86_64, kernel ≥ 5.4 with SocketCAN support.

### 2. Build from source

#### Requirements

| Tool | Minimum version |
|------|----------------|
| Linux | kernel 5.11+ (SocketCAN) |
| GCC or Clang | GCC 13+ / Clang 16+ (C++23) |
| CMake | 3.28+ |
| Ninja | any |
| Qt6 | 6.4+ (Widgets, Charts, SerialBus, SerialPort, LinguistTools) |
| Lua | 5.4 |
| nlohmann-json | 3.x |

Optional (gracefully disabled at configure time if absent):

| Package | Feature |
|---------|---------|
| `libspdlog-dev` + `libcpp-httplib-dev` | MCP / REST API (`socketspy-mcp` binary) |
| `libgtest-dev` | Unit tests |

#### Quick build

```bash
git clone https://github.com/Poisson48/SocketSpy
cd SocketSpy
bash scripts/dev/install_deps.sh   # install system packages (~2 min)
bash build.sh                      # configure + build (~1 min)
./build/dev/gui/socketspy          # run
```

`install_deps.sh` auto-detects your distro from `/etc/os-release`:

| Distribution | Package manager |
|-------------|-----------------|
| Ubuntu, Debian, Linux Mint, Pop!_OS | `apt-get` |
| Fedora, RHEL, CentOS, Rocky, Alma | `dnf` |
| Arch, Manjaro, EndeavourOS | `pacman` |

#### Build flags

```bash
bash build.sh            # debug build (default) → build/dev/gui/socketspy
bash build.sh --release  # optimised, no debug symbols → build/release/gui/socketspy
bash build.sh --clean    # clean then rebuild
```

`build.sh` auto-detects `aarch64` / `armv7l` and selects the matching CMake
preset, so the same command works on a Raspberry Pi.

### 3. Virtual CAN — no hardware needed

The fastest way to try SocketSpy is a virtual interface:

```bash
bash scripts/dev/setup_vcan.sh      # loads the vcan kernel module, creates vcan0 + vcan1
./build/dev/gui/socketspy           # opens on vcan0 by default

# In another terminal, inject a test frame:
cansend vcan0 123#DEADBEEF
```

`setup_vcan.sh` is idempotent — re-running it is safe.
You can also generate traffic without `can-utils` using the built-in
**[Simulator](Features.md#simulator)** panel.

### 4. Real hardware adapters

Bring up your adapter, then select it in the toolbar:

```bash
# Adjust bitrate: 125k / 250k / 500k / 1000k (500k is the most common)
sudo ip link set can0 up type can bitrate 500000
./build/dev/gui/socketspy           # select can0 in the toolbar
```

The toolbar auto-detects every `ARPHRD_CAN` interface. After plugging in an
adapter, hit the **↺** refresh button — no restart needed.

#### Supported adapters (plug-and-play)

Any adapter recognised by the Linux SocketCAN subsystem works out of the box:

| Adapter | Kernel driver |
|---------|--------------|
| CANable 2.0, CANable Pro | `gs_usb` |
| Seeed USB-CAN Analyzer | `gs_usb` |
| PEAK PCAN-USB, PCAN-USB FD | `peak_usb` |
| Kvaser Leaf, Kvaser USBcan | `kvaser_usb` |
| Ixxat USB-to-CAN | `ixxat_usb2can` / `ems_usb` |

Verify detection: `dmesg | grep -i "can\|gs_usb\|peak"` after plugging in.

### 5. Permissions

If the app opens but shows no frames, or you see "permission denied on can0":

- In-app: **Tools → "Configure CAN permissions (one-time)"**, **or**
- `sudo usermod -aG dialout $USER` then log out and back in.

Confirm raw traffic outside the app with `candump can0` (from `can-utils`).

### Next steps

- Explore every panel in **[Features](Features.md)**.
- Drive SocketSpy from Claude or scripts via the **[API & MCP server](Api-MCP.md)**.
- Automate captures with **[Lua scripting](Scripting.md)**.

---

<a name="français"></a>
## Français

### 1. Télécharger l'AppImage (le plus rapide)

Aucune installation requise — l'AppImage est autonome :

```bash
# Récupérer la dernière version
wget https://github.com/Poisson48/SocketSpy/releases/latest/download/SocketSpy-x86_64.AppImage
chmod +x SocketSpy-x86_64.AppImage
./SocketSpy-x86_64.AppImage
```

Requiert Linux x86_64, noyau ≥ 5.4 avec support SocketCAN.

### 2. Compiler depuis les sources

#### Prérequis

| Outil | Version minimale |
|-------|------------------|
| Linux | noyau 5.11+ (SocketCAN) |
| GCC ou Clang | GCC 13+ / Clang 16+ (C++23) |
| CMake | 3.28+ |
| Ninja | toute version |
| Qt6 | 6.4+ (Widgets, Charts, SerialBus, SerialPort, LinguistTools) |
| Lua | 5.4 |
| nlohmann-json | 3.x |

Optionnel (désactivé proprement à la configuration si absent) :

| Paquet | Fonctionnalité |
|--------|----------------|
| `libspdlog-dev` + `libcpp-httplib-dev` | API MCP / REST (binaire `socketspy-mcp`) |
| `libgtest-dev` | Tests unitaires |

#### Compilation rapide

```bash
git clone https://github.com/Poisson48/SocketSpy
cd SocketSpy
bash scripts/dev/install_deps.sh   # installe les paquets système (~2 min)
bash build.sh                      # configure + compile (~1 min)
./build/dev/gui/socketspy          # lancer
```

`install_deps.sh` détecte automatiquement votre distribution via
`/etc/os-release` :

| Distribution | Gestionnaire de paquets |
|-------------|-------------------------|
| Ubuntu, Debian, Linux Mint, Pop!_OS | `apt-get` |
| Fedora, RHEL, CentOS, Rocky, Alma | `dnf` |
| Arch, Manjaro, EndeavourOS | `pacman` |

#### Options de compilation

```bash
bash build.sh            # build debug (défaut) → build/dev/gui/socketspy
bash build.sh --release  # optimisé, sans symboles → build/release/gui/socketspy
bash build.sh --clean    # nettoie puis recompile
```

`build.sh` détecte automatiquement `aarch64` / `armv7l` et choisit le preset
CMake adapté ; la même commande fonctionne donc sur un Raspberry Pi.

### 3. CAN virtuel — sans matériel

Le moyen le plus rapide d'essayer SocketSpy est une interface virtuelle :

```bash
bash scripts/dev/setup_vcan.sh      # charge le module noyau vcan, crée vcan0 + vcan1
./build/dev/gui/socketspy           # s'ouvre sur vcan0 par défaut

# Dans un autre terminal, injecter une trame de test :
cansend vcan0 123#DEADBEEF
```

`setup_vcan.sh` est idempotent — le relancer est sans danger. Vous pouvez aussi
générer du trafic sans `can-utils` grâce au panneau
**[Simulateur](Features.md#simulator)** intégré.

### 4. Adaptateurs matériels réels

Activez votre adaptateur, puis sélectionnez-le dans la barre d'outils :

```bash
# Ajuster le débit : 125k / 250k / 500k / 1000k (500k est le plus courant)
sudo ip link set can0 up type can bitrate 500000
./build/dev/gui/socketspy           # sélectionner can0 dans la barre d'outils
```

La barre d'outils détecte automatiquement chaque interface `ARPHRD_CAN`. Après
avoir branché un adaptateur, cliquez sur le bouton **↺** — aucun redémarrage
nécessaire.

#### Adaptateurs pris en charge (plug-and-play)

Tout adaptateur reconnu par le sous-système Linux SocketCAN fonctionne d'emblée :

| Adaptateur | Pilote noyau |
|------------|--------------|
| CANable 2.0, CANable Pro | `gs_usb` |
| Seeed USB-CAN Analyzer | `gs_usb` |
| PEAK PCAN-USB, PCAN-USB FD | `peak_usb` |
| Kvaser Leaf, Kvaser USBcan | `kvaser_usb` |
| Ixxat USB-to-CAN | `ixxat_usb2can` / `ems_usb` |

Vérifier la détection : `dmesg | grep -i "can\|gs_usb\|peak"` après branchement.

### 5. Permissions

Si l'application s'ouvre mais n'affiche aucune trame, ou si vous voyez
« permission denied on can0 » :

- Dans l'app : **Tools → « Configure CAN permissions (one-time) »**, **ou**
- `sudo usermod -aG dialout $USER` puis déconnexion/reconnexion.

Confirmez le trafic brut hors application avec `candump can0` (de `can-utils`).

### Étapes suivantes

- Explorez chaque panneau dans **[Fonctionnalités](Features.md)**.
- Pilotez SocketSpy depuis Claude ou des scripts via le **[serveur API & MCP](Api-MCP.md)**.
- Automatisez vos captures avec le **[scripting Lua](Scripting.md)**.
