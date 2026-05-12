# SocketSpy Master Prompt v1.0.0

## Philosophy (NON-NEGOTIABLE)

- 100% local operation: no network calls, no telemetry, no analytics
- MIT license; all vendored code must be MIT, BSD, or Apache 2.0 compatible
- REST API and MCP server bind to 127.0.0.1 only — enforced at runtime and by test
- Lua sandbox: no `os.execute`, no `io.popen`, no `require`, no `loadfile`
- Zero outbound network syscalls verified by strace in CI
- File size hard limits:
  - `.cpp` / `.c`: 300 lines maximum
  - `.h` / `.hpp`: 150 lines maximum
  - `CMakeLists.txt` (per directory): 100 lines maximum
  - Shell scripts: 80 lines maximum
  - Violations fail CI

## Code Quality

- C++23, Qt 6.6+, Lua 5.4 (or LuaJIT as option)
- clang-format (BasedOnStyle: Google, IndentWidth: 4, ColumnLimit: 100)
- clang-tidy with modernize-*, performance-*, bugprone-*, clang-analyzer-*
- All new code must pass clang-format and clang-tidy with zero warnings
- Zero Valgrind errors on core binary
- AFL++ fuzz on DBC parser and EDS parser, 60 s minimum per CI run
- Google Test for unit tests; each module has a tests/ subdirectory

## Context Persistence

- Each agent maintains `.socketspy/agents/{name}/context.json`
- The orchestrator state is in `.socketspy/orchestrator.json`
- Before starting work, every agent reads orchestrator.json and its own context.json
- After completing work, every agent updates its context.json with:
  - completed[], in_progress[], decisions[], file_sizes{}
- Deviations from this master prompt go in `.socketspy/DEVIATIONS.md`

## Human Actions

Certain operations require a human to run commands in a real terminal:

- `bash scripts/dev/bootstrap.sh` — initial full setup
- `bash scripts/dev/setup_vcan.sh` — vcan interface creation (requires sudo)
- `sudo modprobe vcan` — kernel module load
- AppImage packaging (requires linuxdeployqt)
- GitHub release creation

These are listed in `orchestrator.json` → `pending_human_actions`.

## Autonomous Testing

Agents must write tests alongside feature code. Rules:

1. Unit tests live in `{module}/tests/`
2. Integration tests live in `tests/integration/`
3. Fuzz targets live in `tests/fuzz/`
4. Every public API function has at least one unit test
5. Tests run via `ctest --preset dev` or `ctest --preset ci`
6. No test may make outbound network calls
7. No test may write outside `/tmp` or the build directory

## Dependency Scripts

Scripts live in `scripts/dev/`. Line limit: 80 lines each.

- `bootstrap.sh` — full setup, calls all others in order
- `install_deps.sh` — system packages + vcpkg bootstrap
- `install_qt_static.sh` — Qt 6.6 static via aqtinstall (release only)
- `setup_vcan.sh` — vcan0/vcan1 setup
- `run_tests.sh` — build + test + valgrind
- `run_integration.sh` — integration tests on vcan
- `run_fuzz.sh` — AFL++ fuzz 60 s per target

## Repository Structure

```
SocketSpy/
├── core/           # CAN capture, ring buffer, io_uring backend
├── protocols/      # CANopen, J1939, OBD-II, UDS, ISO-TP, NMEA 2000
├── dbc/            # DBC lexer/parser/editor/validator
├── gui/            # Qt6 Widgets main application
│   └── src/
│       ├── monitor/    # Live frame monitor panel
│       ├── graphs/     # Signal graphing panel
│       ├── transmit/   # TX / injection panel
│       ├── replay/     # Session replay panel
│       ├── editor/     # DBC editor panel
│       ├── re/         # Reverse engineering toolkit
│       ├── scripting/  # Lua script editor panel
│       └── settings/   # Settings dialog
├── scripting/      # Lua VM wrapper and sandbox
├── api/            # REST server, MCP server, WebSocket stream
├── cmake/          # CMake modules and packaging helpers
├── scripts/        # Dev, tools, and demo scripts
├── tests/          # Integration and fuzz test infrastructure
└── docs/           # Developer documentation
```

## Target Users

1. Automotive / embedded engineers debugging ECU communication
2. Motorsport teams (e.g. Ligier JS P4/P3) monitoring chassis CAN
3. Security researchers performing CAN bus RE
4. Hobbyists with gs_usb adapters
5. AI-assisted workflows via Claude Desktop MCP integration

## Technical Stack

- **Core**: Linux SocketCAN, io_uring, C++23
- **GUI**: Qt 6.6 Widgets + Charts (no QML by default)
- **Scripting**: Lua 5.4 with sandboxed environment
- **REST/WS**: cpp-httplib (header-only, loopback only)
- **MCP**: MCP protocol over stdio or loopback TCP
- **DBC**: Hand-written recursive-descent parser
- **Protocols**: Stateful decode engines per protocol
- **Logging**: spdlog
- **JSON**: nlohmann/json
- **Build**: CMake 3.28 + Ninja + vcpkg
- **Packaging**: AppImage with static Qt

## Feature Set

1. **Live Monitor** — frame table with ID filter, mask filter, rate column, color rules
2. **Signal Decode** — DBC-based decode overlay on monitor, numeric + symbolic values
3. **Graphing** — Qt Charts time-series graph per decoded signal, configurable window
4. **TX / Injection** — single-shot and periodic frame transmission, import from DBC
5. **Replay** — load `.log` (candump format), play at 1x/2x/0.5x, loop, scrub
6. **Lua Scripting** — load `.lua` files, live reload, print console, error highlighting
7. **RE Toolkit** — entropy column, byte-diff mode between captures, pattern heatmap
8. **Protocol Decode** — CANopen NMT/SDO/PDO, J1939 PGN/SPN, UDS, ISO-TP, OBD-II, NMEA 2000
9. **DBC Editor** — tree view of messages/signals, edit in place, save/load, validation
10. **REST API + MCP** — local HTTP + WebSocket API; MCP server for Claude Desktop

## Multi-Agent Split

| Agent      | Owns                                          | Blocked on |
|------------|-----------------------------------------------|------------|
| core       | cancore.h, ring_buffer, io_uring capture      | —          |
| protocols  | CANopen, J1939, UDS, ISO-TP, OBD-II, NMEA2000 | core       |
| dbc        | DBC lexer, parser, AST, validator             | —          |
| gui        | All Qt panels, main window                    | —          |
| scripting  | Lua sandbox, bindings, console                | core       |
| api        | REST server, MCP server, WebSocket            | —          |
| build      | CMake, scripts, CI, scaffolding               | —          |

## README Requirements

README.md must contain:
- One-line description
- Factual feature list (no marketing adjectives)
- Build-from-source (3 commands via bootstrap.sh)
- Quick start with vcan
- Hardware compatibility table
- MCP setup JSON block
- REST API quick reference table
- Lua scripting examples (3 minimum)
- CONTRIBUTING.md link
- MIT license statement

Forbidden words in README: blazing, powerful, seamless, robust, easy, simple,
professional, innovative, next-generation, cutting-edge

## Quality Targets

- Zero outbound network syscalls (verified by strace in CI)
- Zero Valgrind errors on core binary
- clang-format clean (zero diff)
- clang-tidy zero warnings
- All unit tests pass on ubuntu-22.04 and ubuntu-24.04
- AFL++ fuzz: no crashes after 60 s on DBC and EDS parsers

## Start Instructions

1. Read `.socketspy/orchestrator.json`
2. Read your agent's `.socketspy/agents/{name}/context.json`
3. Check `global_blockers` and your `blocked_on` field
4. Work on `next_steps` in order
5. Respect all file size limits
6. Update your context.json when done
7. Log deviations in `.socketspy/DEVIATIONS.md`
