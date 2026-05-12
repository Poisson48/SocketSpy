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

    void setSignalValue(int node_idx, int msg_idx, int sig_idx, double value);

signals:
    void frameGenerated(socketspy::core::CanFrame frame);

private:
    void encodeAndEmit(int node_idx, int msg_idx);
    static void encodeSignal(uint8_t* data, const SimSignal& sig);

    SimProfile         m_profile;
    std::vector<QTimer*> m_timers;
    bool               m_running{false};

    static constexpr uint8_t kSimIfaceIdx = 255;
    static constexpr int     kMaxTimers   = 50;
};

} // namespace socketspy::gui
