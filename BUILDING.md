# Building SocketSpy

## Requirements

| Tool | Minimum version |
|------|----------------|
| Linux | kernel 5.11+ (io_uring, SocketCAN) |
| GCC or Clang | GCC 13+ / Clang 16+ (C++23) |
| CMake | 3.28+ |
| Ninja | any recent |
| Qt6 | 6.4+ (Widgets, Charts, SerialBus) |
| liburing | 2.2+ |
| Lua | 5.4 |

---

## Quick start (Ubuntu / Debian / Linux Mint / Pop!_OS)

```bash
# 1 — Clone
git clone https://github.com/Poisson48/SocketSpy
cd SocketSpy

# 2 — Install dependencies (~2 min)
bash scripts/dev/install_deps.sh

# 3 — Build (~1 min)
bash build.sh

# 4 — Run
./build/dev/gui/socketspy
```

---

## Dependency installation by distro

### Ubuntu 24.04 / Debian 12 / Linux Mint 22

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake ninja-build git pkg-config \
    qt6-base-dev qt6-charts-dev qt6-serialbus-dev \
    liburing-dev \
    lua5.4 liblua5.4-dev \
    nlohmann-json3-dev libspdlog-dev libcpp-httplib-dev \
    libgtest-dev \
    can-utils
```

> **Ubuntu 22.04 note**: Qt 6.4 is not in the default repos.
> Install via `sudo add-apt-repository ppa:ubuntuhandbook1/ppa` or use the
> [Qt online installer](https://www.qt.io/download-qt-installer).

### Fedora 40+

```bash
sudo dnf install -y \
    gcc-c++ cmake ninja-build git pkgconf \
    qt6-qtbase-devel qt6-qtcharts-devel qt6-qtserialbus-devel \
    liburing-devel \
    lua-devel \
    nlohmann-json-devel spdlog-devel \
    gtest-devel \
    can-utils
```

### Arch Linux / Manjaro

```bash
sudo pacman -Sy --noconfirm \
    base-devel cmake ninja git \
    qt6-base qt6-charts \
    liburing \
    lua \
    nlohmann-json spdlog \
    gtest \
    can-utils
```

---

## Build options

```bash
# Standard debug build (default)
bash build.sh

# Clean rebuild
bash build.sh --clean

# Release build (optimised, no debug symbols)
bash build.sh --release
```

The binary is always at:
- Debug:   `build/dev/gui/socketspy`
- Release: `build/release/gui/socketspy`

---

## ARM / Raspberry Pi

### 1. Native build on ARM (Raspberry Pi OS Bookworm, Orange Pi, etc.)

`./build.sh` detects the host architecture automatically and selects the `arm64-dev` or `arm64-release` preset.

```bash
# Install dependencies (Raspberry Pi OS Bookworm / Debian 12 arm64)
sudo apt-get install -y \
    build-essential cmake ninja-build git pkg-config \
    qt6-base-dev qt6-charts-dev qt6-serialbus-dev qt6-serialport-dev \
    liburing-dev \
    lua5.4 liblua5.4-dev \
    can-utils

# Build (debug)
bash build.sh

# Release build
bash build.sh --release
```

Binary locations: `build/arm64-dev/gui/socketspy` / `build/arm64-release/gui/socketspy`

### 2. Cross-compilation from x86 to aarch64

```bash
# Install cross-compiler
sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# Configure and build
cmake --preset arm64-cross
cmake --build build/arm64-cross
```

The toolchain file is at `cmake/toolchains/aarch64-linux-gnu.cmake`.

> **Note**: io_uring is available on ARM Linux since kernel 5.1. Raspberry Pi OS Bookworm ships kernel 6.1+, so it is fully supported.

---

## CAN hardware setup

### Virtual interface (no hardware needed)

```bash
bash scripts/dev/setup_vcan.sh
# Creates vcan0 — usable immediately in the GUI
```

### Real USB-CAN adapter

```bash
# Find the interface name (usually can0)
ip link show | grep can

# Bring it up at the correct bitrate
sudo ip link set can0 up type can bitrate 500000

# Or for 250 kbit/s (common on CANopen / Ligier vehicles)
sudo ip link set can0 up type can bitrate 250000
```

Common bitrates: 125k, 250k, 500k, 1000k

Then select `can0` in the SocketSpy interface selector (toolbar, top of window).

### Supported adapters (plug-and-play, no driver install needed)

| Adapter | Kernel driver |
|---------|--------------|
| CANable 2.0, Canable Pro | gs_usb |
| Seeed USB-CAN Analyzer | gs_usb |
| PEAK PCAN-USB, PCAN-USB FD | peak_usb |
| Kvaser Leaf, Kvaser USBcan | kvaser_usb |
| Ixxat USB-to-CAN | ixxat_usb2can |
| Any gs_usb compatible | gs_usb |

Verify recognition: `dmesg | grep -i "can\|gs_usb\|peak"` after plugging in.

---

## Troubleshooting

**`cmake --preset dev` fails with "preset not found"**  
→ CMake version < 3.28. Check: `cmake --version`. Upgrade via `pip install cmake` or download from cmake.org.

**`Qt6Charts not found`**  
→ On Ubuntu: `sudo apt-get install qt6-charts-dev`  
→ On Fedora: `sudo dnf install qt6-qtcharts-devel`

**`liburing not found`**  
→ `sudo apt-get install liburing-dev` (Debian/Ubuntu)  
→ `sudo dnf install liburing-devel` (Fedora)

**App opens but shows no frames**  
→ Check the interface is up: `ip link show can0`  
→ Check bitrate matches the bus: try 250000 then 500000  
→ Verify with `candump can0` (from can-utils) before opening the app

**Permission denied on can0**  
→ Add your user to the `dialout` group: `sudo usermod -aG dialout $USER` then log out/in  
→ Or run with sudo (not recommended for daily use)
