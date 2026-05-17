#pragma once
#include <QString>
#include <QList>
#include <vector>
#include "cancore.h"

namespace socketspy::gui {

// PCAP classic format (LINKTYPE_CAN_SOCKETCAN = 227).
// Write: export captured frames to a Wireshark-compatible .pcap file.
// Read:  import frames from a .pcap file recorded with SocketCAN link type.

class PcapWriter {
public:
    // Write frames to path.  Returns true on success.
    static bool write(const QString& path,
                      const std::vector<socketspy::core::CanFrame>& frames);
};

class PcapReader {
public:
    // Read frames from a PCAP file.  Returns empty list on error.
    static QList<socketspy::core::CanFrame> read(const QString& path);
};

} // namespace socketspy::gui
