#pragma once
#include <cstdint>
#include <cstring>
#include "cancore.h"

namespace socketspy::gui {

struct TriggerConfig {
    enum class Action { StartRecord, StopRecord, Bookmark };

    bool     enabled    = false;
    uint32_t id         = 0;
    uint32_t id_mask    = 0xFFFFFFFF;
    uint8_t  data[8]    = {};
    uint8_t  data_mask[8] = {};
    uint8_t  data_len   = 0;
    Action   action     = Action::StartRecord;

    [[nodiscard]] bool match(const socketspy::core::CanFrame& f) const noexcept {
        if (!enabled) return false;
        if ((f.id & id_mask) != (id & id_mask)) return false;
        if (data_len == 0) return true;
        if (f.dlc < data_len) return false;
        for (uint8_t i = 0; i < data_len; ++i) {
            if ((f.data[i] & data_mask[i]) != (data[i] & data_mask[i]))
                return false;
        }
        return true;
    }
};

} // namespace socketspy::gui
