---
name: Bug Report
about: Report a reproducible defect in SocketSpy
title: "[Bug] "
labels: bug
assignees: ''
---

## Description

A clear description of what the bug is.

## Steps to Reproduce

1. Start SocketSpy with `./socketspy --iface vcan0`
2. ...
3. ...

## Expected Behavior

What you expected to happen.

## Actual Behavior

What actually happened. Include any error messages or log output.

## System Information

| Field | Value |
|-------|-------|
| Linux distro + version | e.g. Ubuntu 24.04 |
| Kernel version | `uname -r` |
| Qt version | `qmake --version` or cmake output |
| CAN interface type | e.g. gs_usb / PEAK PCAN-USB FD / vcan |
| Interface name | e.g. can0, vcan0 |
| SocketSpy version / commit | `git rev-parse --short HEAD` |
| Build preset used | dev / release / ci |

## Relevant Log Output

```
Paste any relevant output from ~/.local/share/SocketSpy/socketspy.log
or from running with --log-level debug
```

## Additional Context

Any other context about the problem (DBC file used, number of frames/sec, etc.).
