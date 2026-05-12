#pragma once
#include <QFile>
#include <QTextStream>
#include <QString>
#include "cancore.h"

namespace socketspy::gui {

class LogRecorder {
public:
    void open(const QString& path);
    void close();
    void write(const socketspy::core::CanFrame& frame, const QString& iface);
    bool isOpen() const;

private:
    QFile       m_file;
    QTextStream m_stream;
};

} // namespace socketspy::gui
