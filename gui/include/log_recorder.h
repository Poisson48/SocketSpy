#pragma once
#include <QFile>
#include <QTextStream>
#include <QString>
#include "cancore.h"
#include "filter_model.h"

namespace socketspy::gui {

class LogRecorder {
public:
    void open(const QString& path);
    void close();
    void write(const socketspy::core::CanFrame& frame, const QString& iface);
    bool isOpen() const;
    void setFilter(const FrameFilter& f);

private:
    QFile       m_file;
    QTextStream m_stream;
    FrameFilter m_filter;
};

} // namespace socketspy::gui
