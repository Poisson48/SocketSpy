#pragma once
#include <cstdint>
#include "cancore.h"

namespace socketspy::gui {

struct FrameFilter {
    bool     enabled   = false;
    uint32_t id_min    = 0;
    uint32_t id_max    = 0x1FFFFFFF;
    uint32_t id_mask   = 0xFFFFFFFF;
    int      dlc_min   = 0;
    int      dlc_max   = 15;
    bool     show_std  = true;
    bool     show_ext  = true;
    bool     show_fd   = true;
    bool     show_err  = false;

    bool accepts(const socketspy::core::CanFrame& f) const noexcept;
};

} // namespace socketspy::gui
