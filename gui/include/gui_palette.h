#pragma once
// ─── SocketSpy GUI colour palette ────────────────────────────────────────────
// Single source of truth for every named colour used across panels.
// Include this header *after* all Qt headers (it only defines compile-time
// QColor values — no translation-unit coupling issues).

#include <QColor>
#include <iterator>  // std::size

namespace socketspy::gui::Palette {

// ── Signal / trace colours (ordered for maximum contrast) ────────────────────
inline constexpr const char* kSigIndigo  = "#6366f1";
inline constexpr const char* kSigGreen   = "#22c55e";
inline constexpr const char* kSigAmber   = "#f59e0b";
inline constexpr const char* kSigRed     = "#ef4444";
inline constexpr const char* kSigCyan    = "#06b6d4";
inline constexpr const char* kSigViolet  = "#a78bfa";
inline constexpr const char* kSigOrange  = "#fb923c";
inline constexpr const char* kSigPink    = "#f472b6";

// Ordered array — index this with `sigIdx % kNumSigColors`
inline const QColor kSigColors[] = {
    QColor(kSigIndigo),  QColor(kSigGreen), QColor(kSigAmber),
    QColor(kSigRed),     QColor(kSigCyan),  QColor(kSigViolet),
    QColor(kSigOrange),  QColor(kSigPink),
};
inline constexpr int kNumSigColors = static_cast<int>(std::size(kSigColors));

// ── Time-marker colours ───────────────────────────────────────────────────────
inline const QColor kMarkerColors[] = {
    QColor(kSigAmber), QColor(kSigGreen), QColor(kSigRed),
    QColor(kSigCyan),  QColor(kSigViolet),
};
inline constexpr int kNumMarkerColors = static_cast<int>(std::size(kMarkerColors));

// ── Semantic / UI colours ─────────────────────────────────────────────────────
inline constexpr const char* kLiveGreen  = "#22c55e"; // LIVE status indicator
inline constexpr const char* kDeadGray   = "#4b5563"; // inactive status

} // namespace socketspy::gui::Palette
