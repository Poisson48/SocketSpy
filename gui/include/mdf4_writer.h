#pragma once
#include <QString>
#include <vector>
#include "cancore.h"

namespace socketspy::gui {

// Minimal MDF4 (Measurement Data Format v4) writer.
// Writes a self-consistent MDF4 file containing raw CAN frames.
// Block structure: ID → HD → FH → DG → CG → CN → CN → DT

class Mdf4Writer {
public:
    static bool write(const QString& path,
                      const std::vector<socketspy::core::CanFrame>& frames);
};

} // namespace socketspy::gui
