# Security Policy

## Network Attack Surface

SocketSpy is a local tool. It has no cloud components and makes no outbound
network connections.

- The REST API listens on `127.0.0.1:8765` only. Binding to `0.0.0.0` or any
  non-loopback address is rejected at startup by a compile-time assertion and a
  runtime check in `api/src/rest/rest_server.cpp`.
- The MCP server listens on `127.0.0.1:8766` only, same enforcement.
- The WebSocket stream endpoint (`/api/stream`) is served on the same
  loopback-only listener.

If `strace socketspy` shows any `connect()`, `bind()`, or `sendto()` syscall
with a non-loopback destination, that is a bug.

## Lua Sandbox

Lua scripts execute inside a restricted sandbox:

- `os.execute`, `os.exit`, `io.popen`, `loadfile`, `dofile`, and `require`
  are removed from the Lua environment before any user script is loaded.
- Scripts cannot open arbitrary file descriptors.
- CPU time per script tick is limited to 5 ms via a Lua debug hook.

## Supported Versions

| Version | Supported |
|---------|-----------|
| 0.1.x   | yes       |

## Reporting a Vulnerability

Report vulnerabilities via **GitHub Security Advisories**:

1. Go to https://github.com/Poisson48/SocketSpy/security/advisories
2. Click "New draft security advisory"
3. Describe the issue, affected versions, and reproduction steps

Do not open a public issue for security-sensitive findings. We aim to
acknowledge reports within 72 hours and publish a fix within 14 days for
confirmed vulnerabilities.
