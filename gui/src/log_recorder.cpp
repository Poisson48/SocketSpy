#include "log_recorder.h"
#include <QIODevice>

namespace socketspy::gui {

void LogRecorder::open(const QString& path) {
    if (m_file.isOpen()) m_file.close();
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return;
    m_stream.setDevice(&m_file);
    m_stream << "# SocketSpy log v1\n";
    m_stream.flush();
}

void LogRecorder::close() {
    if (m_file.isOpen()) {
        m_stream.flush();
        m_file.close();
    }
}

void LogRecorder::write(const socketspy::core::CanFrame& frame, const QString& iface) {
    if (!m_file.isOpen()) return;

    double ts = static_cast<double>(frame.timestamp_us) / 1'000'000.0;

    bool extended = (frame.id > 0x7FFu);
    QString idStr = extended
        ? QString("%1").arg(frame.id, 8, 16, QChar('0')).toUpper()
        : QString("%1").arg(frame.id, 3, 16, QChar('0')).toUpper();

    QString dataStr;
    dataStr.reserve(frame.dlc * 2);
    for (int i = 0; i < frame.dlc; ++i)
        dataStr += QString("%1").arg(frame.data[i], 2, 16, QChar('0')).toUpper();

    m_stream << QString("(%1) %2 %3#%4\n")
                    .arg(ts, 0, 'f', 6)
                    .arg(iface)
                    .arg(idStr)
                    .arg(dataStr);
    m_stream.flush();
}

bool LogRecorder::isOpen() const {
    return m_file.isOpen();
}

} // namespace socketspy::gui
