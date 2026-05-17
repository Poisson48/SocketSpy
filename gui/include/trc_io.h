#pragma once
#include <QList>
#include <QString>
#include "cancore.h"

namespace socketspy::gui {

// ---------------------------------------------------------------------------
// TRC format v2.x — PEAK PCAN Trace format
// Writer: frames → .trc file
// Reader: .trc file → frames list
// ---------------------------------------------------------------------------

class TrcWriter {
public:
    static bool write(const QList<socketspy::core::CanFrame>& frames,
                      const QString& path);
};

class TrcReader {
public:
    static QList<socketspy::core::CanFrame> read(const QString& path);
};

} // namespace socketspy::gui
