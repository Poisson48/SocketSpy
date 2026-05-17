#pragma once
#include <QString>
#include <QList>
#include "cancore.h"

namespace socketspy::gui {

// Writes CAN frames to a Vector ASCII log (.asc) file.
class AscWriter {
public:
    // Returns true on success. Timestamps are relative to the first frame.
    static bool write(const QList<socketspy::core::CanFrame>& frames,
                      const QString& path);
};

// Reads CAN frames from a Vector ASCII log (.asc) file.
class AscReader {
public:
    // Returns parsed frames; ignores unrecognised lines.
    static QList<socketspy::core::CanFrame> read(const QString& path);
};

} // namespace socketspy::gui
