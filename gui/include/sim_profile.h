#pragma once
#include <QString>
#include <vector>
#include <cstdint>

namespace socketspy::gui {

struct SimScenarioPoint {
    int64_t t_ms;
    double  value;
};

struct SimSignal {
    QString name;
    int     start_bit;
    int     length;
    double  factor;
    double  offset;
    double  min;
    double  max;
    double  current_value;
    std::vector<SimScenarioPoint> scenario; // empty = static/manual
};

struct SimMessage {
    uint32_t             id;
    int                  period_ms;
    uint8_t              dlc;
    std::vector<SimSignal> sigs;
};

struct SimNode {
    QString                  name;
    std::vector<SimMessage>  messages;
};

struct SimProfile {
    QString               name;
    QString               description;
    std::vector<SimNode>  nodes;
};

SimProfile load_sim_profile(const QString& json_path);

} // namespace socketspy::gui
