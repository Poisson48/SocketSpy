#pragma once
// Couleurs de surlignage partagées par les TU de MonitorPanel.
#include <QColor>

namespace socketspy::gui {

inline const QColor kPinBg    {210, 230, 255};  // light blue — pinned row
inline const QColor kChangedBg{255, 200, 100};  // amber    — data just changed

} // namespace socketspy::gui
