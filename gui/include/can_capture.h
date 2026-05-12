#pragma once
#include <QThread>
#include <QString>
#include <atomic>
#include <mutex>
#include <cstdint>
#include "cancore.h"
#include "trigger_config.h"

namespace socketspy::gui {

class CanCapture : public QThread {
    Q_OBJECT

public:
    explicit CanCapture(QString iface, QObject* parent = nullptr);
    ~CanCapture() override;

    void stop();
    void setTrigger(socketspy::gui::TriggerConfig cfg);

signals:
    void frameReceived(socketspy::core::CanFrame frame);
    void statsUpdated(uint64_t frames_per_sec);
    void errorOccurred(QString message);
    void triggerFired();

protected:
    void run() override;

private:
    QString m_iface;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_triggerArmed{false};
    std::mutex        m_triggerMutex;
    TriggerConfig     m_trigger;
};

} // namespace socketspy::gui
