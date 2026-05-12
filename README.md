# SocketSpy — Linux CAN bus analysis platform

100% local, no telemetry, no network calls. Built on Linux SocketCAN.

## Features

- **Live frame monitor** — real-time scrolling table, 500-row circular buffer, pause/clear
- **Signal decode** — load a `.dbc` file, signals decoded and displayed inline
- **Signal graphs** — up to 4 live traces on a rolling 10-second QChart window
- **Frame transmit** — send standard/extended/FD frames with input validation
- **Interface selector** — auto-detects all SocketCAN interfaces, hot-swap without restart
- **DBC parser** — full round-trip lossless parser (VERSION, BU_, BO_, SG_, VAL_, CM_)
- **Protocol decoders** — CANopen NMT/SDO/PDO/EMCY, J1939, ISO-TP, UDS, OBD-II, NMEA 2000
- **Lua scripting** — sandboxed Lua 5.4 engine with watchdog, CAN/SDO/log APIs
- **MCP server** — JSON-RPC 2.0 over stdio or TCP (127.0.0.1 only) for Claude integration
- **io_uring capture** — high-throughput frame capture pipeline with hardware timestamps

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

## License

MIT — see [LICENSE](LICENSE).
