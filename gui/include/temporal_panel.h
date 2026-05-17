#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QTimer>
#include <QHash>
#include <QVector>
#include <QPushButton>
#include "cancore.h"

namespace socketspy::gui {

class TemporalPanel : public QWidget {
    Q_OBJECT

public:
    explicit TemporalPanel(QWidget* parent = nullptr);

public slots:
    void onFrameReceived(const socketspy::core::CanFrame& frame);

private slots:
    void updateTable();
    void onClear();

private:
    void setupUi();

    QTableWidget*              m_table{nullptr};
    QPushButton*               m_clearBtn{nullptr};
    QTimer*                    m_timer{nullptr};

    QHash<uint32_t, qint64>        m_lastTs;
    QHash<uint32_t, QVector<double>> m_intervals;
    QHash<uint32_t, int>           m_count;
};

} // namespace socketspy::gui
