#pragma once
#include <QString>
#include <vector>
#include "cancore.h"

namespace socketspy::gui {

// Minimal BLF (Binary Logging Format) writer.
// Generates files compatible with Vector CANalyzer (BLF version 2).
// Not compressed (statistics object uses uncompressed blocks).

class BlfWriter {
public:
    // Write frames to path. Returns true on success.
    static bool write(const QString& path,
                      const std::vector<socketspy::core::CanFrame>& frames);
};

} // namespace socketspy::gui
