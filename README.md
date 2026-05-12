# SocketSpy — Linux CAN bus analysis platform

## Features

- Live frame monitor with filtering and colorization
- Signal decode and graphing from DBC definitions
- Frame transmission and injection with scheduling
- Replay of recorded sessions with time scaling
- Lua scripting for automation and custom decode logic
- Reverse engineering toolkit (entropy, pattern detection, diff mode)
- Protocol support: CANopen, J1939, OBD-II, UDS, ISO-TP, NMEA 2000
- DBC file editor with validation
- Local REST API (127.0.0.1 only)
- Local MCP server for Claude Desktop integration (127.0.0.1 only)

## Build from Source

```bash
git clone https://github.com/Poisson48/SocketSpy
cd SocketSpy
bash scripts/dev/bootstrap.sh
```

The bootstrap script installs all dependencies, configures the build, and runs the test suite.
On success the binary is at `build/dev/socketspy`.

## Quick Start with vcan (no hardware required)

```bash
# Set up virtual CAN interfaces
bash scripts/dev/setup_vcan.sh

# Run SocketSpy on vcan0
./build/dev/socketspy --iface vcan0

# In another terminal — send a test frame
cansend vcan0 123#DEADBEEF
```

## Hardware Compatibility

- CANable 2.0 (gs_usb)
- PEAK PCAN-USB FD (peak_usb)
- Kvaser Leaf (kvaser_usb)
- Ixxat USB-to-CAN (ixxat_usb2can)
- Seeed USB-CAN (gs_usb)
- Any adapter using the gs_usb kernel driver

All interfaces are accessed via the standard Linux SocketCAN API.

## MCP Setup for Claude Desktop

Add the following block to your Claude Desktop configuration file
(`~/.config/Claude/claude_desktop_config.json` on Linux):

```json
{
  "mcpServers": {
    "socketspy": {
      "command": "/usr/local/bin/socketspy",
      "args": ["--mcp", "--iface", "vcan0"]
    }
  }
}
```

Replace `vcan0` with your actual CAN interface. The MCP server binds only to
`127.0.0.1` and accepts no remote connections.

## REST API Quick Reference

| Method | Path          | Description                              |
|--------|---------------|------------------------------------------|
| GET    | /api/frames   | Returns last N frames as JSON array      |
| POST   | /api/send     | Transmit a frame `{"id":0x123,"data":"DEADBEEF"}` |
| WS     | /api/stream   | WebSocket stream of frames in real time  |

All endpoints listen on `127.0.0.1:8765` only.

Example:
```bash
curl http://127.0.0.1:8765/api/frames?n=10
curl -X POST http://127.0.0.1:8765/api/send \
     -H 'Content-Type: application/json' \
     -d '{"id":291,"data":"DEADBEEF","iface":"vcan0"}'
```

## Lua Scripting Quick Reference

Lua scripts run inside a sandboxed interpreter. `os.execute` is disabled.

**Print every frame on vcan0:**
```lua
spy.on_frame("vcan0", function(frame)
    print(string.format("0x%03X [%d] %s", frame.id, frame.dlc, frame.hex))
end)
```

**Decode a signal manually:**
```lua
spy.on_frame("vcan0", function(frame)
    if frame.id == 0x200 then
        local rpm = spy.extract_bits(frame.data, 0, 16) * 0.25
        print("RPM:", rpm)
    end
end)
```

**Send a periodic frame:**
```lua
spy.periodic("vcan0", 0x100, "0102030405060708", 100)  -- every 100 ms
```

## Contributing

See [CONTRIBUTING.md](.github/CONTRIBUTING.md).

## License

MIT — see [LICENSE](LICENSE).
