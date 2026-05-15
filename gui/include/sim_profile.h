#pragma once
#include <QString>
#include <vector>
#include <cstdint>

namespace socketspy::gui {

struct SimScenarioPoint {
    int64_t t_ms;
    double  value;
};

enum class WaveformType {
    None,    // static / manual (no animation)
    Sine,
    Ramp,    // sawtooth up
    Square,
    Random
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

    // Runtime-configurable waveform (overrides/supplements scenario)
    WaveformType waveform   {WaveformType::None};
    int          num_points {100};   // points per cycle
    int          step_ms    {50};    // ms between steps (= update rate)
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
bool       save_sim_profile(const SimProfile& profile, const QString& json_path);

} // namespace socketspy::gui
