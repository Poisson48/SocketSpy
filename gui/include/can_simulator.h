#pragma once
#include <QObject>
#include <QTimer>
#include <vector>
#include "sim_profile.h"
#include "cancore.h"

namespace socketspy::gui {

class CanSimulator : public QObject {
    Q_OBJECT

public:
    explicit CanSimulator(QObject* parent = nullptr);
    ~CanSimulator() override;

    void load(const SimProfile& profile);
    void start();
    void stop();
    bool isRunning() const;

    void    setSignalValue(int node_idx, int msg_idx, int sig_idx, double value);
    double  signalValue(int node_idx, int msg_idx, int sig_idx) const;
    int64_t elapsedMs() const { return m_elapsed_ms; }
    int64_t scenarioDuration() const;
    void    resetElapsed();

    // Runtime waveform configuration (effective immediately, even while running)
    void setSignalWaveform(int node_idx, int msg_idx, int sig_idx,
                           WaveformType waveform, double min, double max,
                           int num_points, int step_ms);

    const SimSignal* signalAt(int ni, int mi, int si) const;
          SimSignal* signalMut(int ni, int mi, int si);

signals:
    void frameGenerated(socketspy::core::CanFrame frame);
    void animationTick();

private slots:
    void tickAnimations();

private:
    void encodeAndEmit(int node_idx, int msg_idx);
    static void encodeSignal(uint8_t* data, const SimSignal& sig);
    static double interpolateScenario(const std::vector<SimScenarioPoint>& pts, int64_t t_ms);
    static std::vector<SimScenarioPoint> generateWaveform(WaveformType waveform,
                                                          double min, double max,
                                                          int num_points, int step_ms);

    SimProfile           m_profile;
    std::vector<QTimer*> m_timers;
    bool                 m_running{false};
    int64_t              m_elapsed_ms{0};

    static constexpr uint8_t kSimIfaceIdx        = 255;
    static constexpr int     kMaxTimers          = 50;
    static constexpr int     kAnimInterval       = 50;
    static constexpr int     kGlobalTimerInterval = 10;
};

} // namespace socketspy::gui
