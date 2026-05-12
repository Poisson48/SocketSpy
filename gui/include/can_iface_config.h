#pragma once
#include <QString>
#include <utility>

namespace socketspy::gui {

// Brings a SocketCAN interface down then up with the given bitrate.
// Tries ip link set directly first; falls back to pkexec if permission denied.
// Returns true on success. Does nothing and returns true for vcan* interfaces.
bool canIfaceApply(const QString& iface, int bitrate_bps);

// Returns true for virtual interfaces (vcan*) that have no configurable bitrate.
bool canIfaceIsVirtual(const QString& iface);

// Reads the current bitrate from /sys/class/net/<iface>/can_bittiming/bitrate.
// Returns 0 if unavailable.
int canIfaceReadBitrate(const QString& iface);

// Configures a serial-line CAN (slcan) adapter: runs slcand then brings up slcan0.
// devPath is the serial device (e.g. "/dev/ttyUSB0").
// Returns {true, "slcan0"} on success, {false, ""} on failure.
std::pair<bool, QString> canSlcanSetup(const QString& devPath, int bitrate_bps);

} // namespace socketspy::gui
