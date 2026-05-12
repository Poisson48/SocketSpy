#pragma once
#include <QThread>
#include <QString>
#include <cstdint>
#include "cancore.h"

namespace socketspy::gui {

class CanCapture : public QThread {
    Q_OBJECT

public:
    explicit CanCapture(QString iface, QObject* parent = nullptr);
    ~CanCapture() override;

    void stop();

signals:
    void frameReceived(socketspy::core::CanFrame frame);
    void statsUpdated(uint64_t frames_per_sec);
    void errorOccurred(QString message);

protected:
    void run() override;

private:
    QString m_iface;
    std::atomic<bool> m_stop{false};
};

} // namespace socketspy::gui
