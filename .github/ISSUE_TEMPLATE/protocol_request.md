---
name: Protocol Request
about: Request decode support for a CAN-based protocol
title: "[Protocol] "
labels: protocol
assignees: ''
---

## Protocol Name

Full name and common abbreviation (e.g. "Controller Area Network open — CANopen").

## Specification Reference

- Standard number or document name (e.g. CiA 301 v4.2.0)
- URL to the publicly available specification, if one exists
- Is the full specification freely available, or is it behind a paywall?

## Sample Frames

If you have sample candump captures or frame descriptions, paste them here.
Use candump log format if possible:

```
(1714000000.000000) vcan0 701#05
(1714000000.001000) vcan0 181#FF00000000000000
```

Frame descriptions (what each field means) are helpful even if you cannot
share the actual frames.

## Use Case

Describe who would benefit from this protocol support and how.
Include whether this is for decode-only (passive monitoring) or also for
transmit/response (active testing).

## Existing Open Source References

List any open source implementations you are aware of (canopen-stack,
lely-core, etc.). These help with cross-checking decode logic.
