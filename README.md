# SocketSpy — Linux CAN bus analysis platform

**[→ socketspy.dev — full documentation & screenshots](https://poisson48.github.io/SocketSpy/)**

100% local, no telemetry, no network calls. Built on Linux SocketCAN.

## Download

**[→ SocketSpy-x86_64.AppImage](https://github.com/Poisson48/SocketSpy/releases/latest)** — self-contained, no installation required.

```bash
chmod +x SocketSpy-x86_64.AppImage
./SocketSpy-x86_64.AppImage
```

Requires Linux x86_64, kernel ≥ 5.4 with SocketCAN support.

## Features

- **Live frame monitor** — real-time scrolling table, 2000-row buffer, search by ID, pause/clear
- **Signal decode** — load a `.dbc` file, signals decoded and displayed inline
- **Signal graphs** — up to 8 live traces on a rolling 10-second QChart window
- **Frame transmit** — send standard/extended/FD frames (DLC 0–15, 64 bytes) with input validation
- **Interface selector** — auto-detects all SocketCAN interfaces, hot-swap without restart
- **DBC parser** — full round-trip lossless parser (VERSION, BU_, BO_, SG_, VAL_, CM_)
- **Protocol decoders** — live in-app tab: CANopen NMT/SDO/PDO/EMCY, J1939, ISO-TP, UDS, OBD-II, NMEA 2000
- **Lua scripting** — in-app editor with Run/Stop + console; sandboxed Lua 5.4 engine with watchdog
- **CAN simulator** — generate CAN traffic without hardware; built-in vehicle profiles or create custom ones from the UI
- **MCP server** — JSON-RPC 2.0 over stdio or TCP (127.0.0.1 only) for Claude integration
- **io_uring capture** — high-throughput frame capture pipeline with hardware timestamps
- **Frame fuzzer** — send random, incremental, or bit-flip CAN frames at a configurable interval; frame counter per run
- **UDS tester** — ISO 14229 + ISO 15765-2 transport; Read DTC (0x19), Clear DTC (0x14), Read ECU Info via 0x22 (VIN, serial, session); live session indicator
- **UDS ECU simulator** — full ISO-TP server + UDS responder (services 0x10/0x11/0x14/0x19/0x22/0x27/0x28/0x2E/0x31/0x3E); SecurityAccess seed/key; configurable DIDs and DTCs; test UDS clients without real hardware
- **BLF / MDF4 export** — File → "Export BLF..." (Vector CANalyzer-compatible BLF v2) and "Export MDF4..." (asammdf / MATLAB MDF4 v4.10)
- **Capture diff** — compare two CAN captures (.log or .csv) side-by-side; diff by ID (A only / B only / Changed / Same), byte-level delta, filterable results
- **Multi-bus monitor** — Tools → "+ Add Second Bus..." opens a second SocketCAN interface in parallel; "Bus" column in the Monitor distinguishes frame sources
- **Detachable panel navigation** — icon sidebar replaces the old tab bar; drag any panel or right-click → "Detach" to float it as an independent window; Redock button snaps it back
- **Signal Detective** — "Detect" sidebar panel with two tabs: auto-classify all observed CAN IDs by update rate (slow / medium / fast) and signal type (digital / analog / constant / counter); wiggle test captures baseline then after a physical action and ranks the top 30 changed signals by score
- **Copy to clipboard** — Ctrl+C or right-click context menu on any table: copy a single cell, selected rows, or the full table as TSV
- **Welcome screen** — GIMP-style floating dialog on first launch; shows recent projects, resource links and "don't show again" preference
- **French / English UI** — full Qt i18n via `.ts` files; language selection persists across sessions

## Build

```bash
git clone https://github.com/Poisson48/SocketSpy
cd SocketSpy
bash scripts/dev/install_deps.sh   # install system packages (~2 min)
bash build.sh                       # configure + build (~1 min)
./build/dev/gui/socketspy           # run
```

See **[BUILDING.md](BUILDING.md)** for full instructions, supported distros, and hardware setup.

## Quick start — virtual CAN (no hardware needed)

```bash
bash scripts/dev/setup_vcan.sh      # creates vcan0
./build/dev/gui/socketspy           # opens on vcan0 by default

# In another terminal — inject a test frame
cansend vcan0 123#DEADBEEF
```

## Real hardware

```bash
# Bring up your adapter (adjust bitrate: 125k / 250k / 500k / 1000k)
sudo ip link set can0 up type can bitrate 500000

# Run and select can0 in the toolbar
./build/dev/gui/socketspy
```

The toolbar auto-detects all `ARPHRD_CAN` interfaces. Hit **↺** to refresh after plugging in an adapter.

## Supported adapters

Any adapter recognised by the Linux SocketCAN subsystem works out of the box:

| Adapter | Driver |
|---------|--------|
| CANable 2.0, CANable Pro, Seeed USB-CAN | gs_usb |
| PEAK PCAN-USB, PCAN-USB FD | peak_usb |
| Kvaser Leaf, Kvaser USBcan | kvaser_usb |
| Ixxat USB-to-CAN | ixxat_usb2can |

## Lua scripts

Pre-built scripts in `scripts/`:

| Script | Description |
|--------|-------------|
| `tools/canopen_node_scan.lua` | Scan CANopen heartbeats (0x700–0x77F), list live nodes |
| `tools/delta_detector.lua` | Detect signal changes vs baseline |
| `demos/ligier_pulse3/battery_scan.lua` | Read Ligier Pulse 3 BMS cell voltages via SDO |
| `demos/ligier_pulse3/contactor_boot.lua` | NMT + PDO contactor boot sequence |

## MCP server — Claude integration

Add to `~/.config/Claude/claude_desktop_config.json`:

```json
{
  "mcpServers": {
    "socketspy": {
      "command": "/path/to/build/dev/gui/socketspy",
      "args": ["--mcp", "--iface", "can0"]
    }
  }
}
```

The MCP server exposes 11 tools: `can_monitor`, `can_send`, `can_decode`, `can_replay`,
`can_script`, `canopen_sdo_read`, `canopen_sdo_write`, `canopen_scan`, `can_diff`, `get_stats`, `can_stop`.
All traffic stays local — TCP transport binds only to `127.0.0.1`.

## What's new in v0.7.0

- **Detachable panel navigation** — icon sidebar replaces the 22-tab scroll bar; drag or right-click any panel to detach it as a floating window; Redock button snaps it back
- **Signal Detective** — auto-classify all live CAN IDs by update rate and signal type (heuristic, no LLM); wiggle test ranks the top 30 changed signals after a physical action
- **UDS ECU simulator** — full ISO-TP + UDS server (10 services, SecurityAccess, configurable DIDs/DTCs) to test UDS clients without real hardware
- **Copy to clipboard** — Ctrl+C and right-click context menu on every table (cell / row / full table TSV)
- **Welcome screen** — GIMP-style floating dialog at startup with recent projects, resource links and "don't show again"
- **French / English UI** — full Qt i18n; language choice persists across sessions

## What's new in v0.6.0

- **Frame fuzzer** — randomised / incremental / bit-flip frame injection with configurable interval and frame counter
- **UDS tester** — full ISO 14229 + ISO 15765-2 session: Read DTC, Clear DTC, Read ECU Info (VIN / serial / session mode)
- **BLF / MDF4 export** — export captures to Vector CANalyzer-compatible BLF v2 or asammdf/MATLAB MDF4 v4.10
- **Capture diff** — diff two .log/.csv captures by CAN ID; byte-level delta, A-only / B-only / Changed / Same filter
- **Multi-bus monitor** — run two SocketCAN interfaces in parallel; per-frame "Bus" column in the Monitor

## License

GPL-3.0 — see [LICENSE](LICENSE).
