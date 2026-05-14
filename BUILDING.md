# Building SocketSpy

## Quick start (Ubuntu / Debian / Linux Mint / Pop!_OS)

```bash
# 1 — Clone
git clone https://github.com/Poisson48/SocketSpy
cd SocketSpy

# 2 — Install dependencies (~1 min)
bash scripts/dev/install_deps.sh

# 3 — Build (~1 min)
bash build.sh

# 4 — Run
./build/dev/gui/socketspy
```

---

## Requirements

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
| libspdlog-dev + libcpp-httplib-dev | MCP/REST API (`socketspy-mcp` binary) |
| libgtest-dev | Unit tests |

---

## Manual dependency installation by distro

### Ubuntu 24.04 / Debian 12 / Linux Mint 22

```bash
sudo apt-get install -y \
    build-essential cmake ninja-build git pkg-config \
    qt6-base-dev qt6-charts-dev qt6-serialbus-dev qt6-serialport-dev \
    qt6-tools-dev qt6-l10n-tools \
    lua5.4 liblua5.4-dev \
    nlohmann-json3-dev \
    libspdlog-dev libcpp-httplib-dev libgtest-dev \
    can-utils
```

> **Ubuntu 22.04**: Qt 6.4 is not in the default repos.
> Install via `sudo add-apt-repository ppa:ubuntuhandbook1/ppa` or the
> [Qt online installer](https://www.qt.io/download-qt-installer).

### Fedora 40+

```bash
sudo dnf install -y \
    gcc-c++ cmake ninja-build git pkgconf \
    qt6-qtbase-devel qt6-qtcharts-devel qt6-qtserialbus-devel qt6-qtserialport-devel \
    qt6-linguist \
    lua-devel \
    nlohmann-json-devel spdlog-devel \
    gtest-devel \
    can-utils
```

### Arch Linux / Manjaro

```bash
sudo pacman -Sy --noconfirm \
    base-devel cmake ninja git \
    qt6-base qt6-charts qt6-serialbus qt6-serialport qt6-tools \
    lua \
    nlohmann-json spdlog \
    gtest \
    can-utils
```

---

## Build options

```bash
bash build.sh            # debug build (default)
bash build.sh --clean    # clean then rebuild
bash build.sh --release  # optimised, no debug symbols
```

Binaries:
- Debug:   `build/dev/gui/socketspy`
- Release: `build/release/gui/socketspy`

---

## ARM / Raspberry Pi

`build.sh` auto-detects `aarch64` / `armv7l` and selects the right preset.

```bash
# Raspberry Pi OS Bookworm / Debian 12 arm64
sudo apt-get install -y \
    build-essential cmake ninja-build git pkg-config \
    qt6-base-dev qt6-charts-dev qt6-serialbus-dev qt6-serialport-dev \
    qt6-tools-dev qt6-l10n-tools \
    lua5.4 liblua5.4-dev nlohmann-json3-dev \
    can-utils

bash build.sh
./build/arm64-dev/gui/socketspy
```

Cross-compilation from x86:

```bash
sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
cmake --preset arm64-cross
cmake --build build/arm64-cross
```

Toolchain file: `cmake/toolchains/aarch64-linux-gnu.cmake`

---

## CAN hardware setup

### Virtual interface (no hardware needed)

```bash
bash scripts/dev/setup_vcan.sh   # creates vcan0
```

### Real USB-CAN adapter

```bash
sudo ip link set can0 up type can bitrate 500000   # 500k (most common)
# or 250000 for CANopen / some OBD buses
```

Common bitrates: 125k · 250k · 500k · 1000k

### Supported adapters (plug-and-play)

| Adapter | Kernel driver |
|---------|--------------|
| CANable 2.0, CANable Pro | gs_usb |
| Seeed USB-CAN Analyzer | gs_usb |
| PEAK PCAN-USB, PCAN-USB FD | peak_usb |
| Kvaser Leaf, Kvaser USBcan | kvaser_usb |
| Ixxat USB-to-CAN | ems_usb |

Verify: `dmesg | grep -i "can\|gs_usb\|peak"` after plugging in.

---

## Troubleshooting

**`Qt6Charts not found`**  
→ `sudo apt-get install qt6-charts-dev`

**`lua5.4 not found`**  
→ `sudo apt-get install liblua5.4-dev`

**`nlohmann_json not found`**  
→ `sudo apt-get install nlohmann-json3-dev`

**`CMake 3.28+ required`**  
→ `pip install cmake` or download from cmake.org

**App opens but shows no frames**  
→ Check interface is up: `ip link show can0`  
→ Verify with `candump can0` (from can-utils)

**Permission denied on can0**  
→ Tools → "Configure CAN permissions (one-time)" in the app  
→ Or: `sudo usermod -aG dialout $USER` then re-login
