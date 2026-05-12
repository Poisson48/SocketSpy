#include "filter_model.h"

namespace socketspy::gui {

bool FrameFilter::accepts(const socketspy::core::CanFrame& f) const noexcept {
    if (!enabled) return true;

    using F = socketspy::core::FrameFlags;
    const bool is_fd  = (f.flags & static_cast<uint8_t>(F::FD))    != 0;
    const bool is_err = (f.flags & static_cast<uint8_t>(F::Error)) != 0;
    const bool is_ext = !is_err && (f.id > 0x7FF);
    const bool is_std = !is_err && !is_ext && !is_fd;

    if (is_err && !show_err) return false;
    if (is_fd  && !show_fd)  return false;
    if (is_ext && !show_ext) return false;
    if (is_std && !show_std) return false;

    if (f.dlc < static_cast<uint8_t>(dlc_min) ||
        f.dlc > static_cast<uint8_t>(dlc_max))
        return false;

    const uint32_t masked = f.id & id_mask;
    if (masked < id_min || masked > id_max) return false;

    return true;
}

} // namespace socketspy::gui
