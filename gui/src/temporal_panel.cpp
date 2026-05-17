#include "temporal_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <cmath>
#include <algorithm>

namespace socketspy::gui {

TemporalPanel::TemporalPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void TemporalPanel::setupUi() {
    m_clearBtn = new QPushButton(tr("Clear"), this);

    auto* toolbar = new QHBoxLayout;
    toolbar->addWidget(m_clearBtn);
    toolbar->addStretch();

    m_table = new QTableWidget(0, 7, this);
    m_table->setHorizontalHeaderLabels({
        tr("ID (hex)"), tr("Count"),
        tr("Last \xce\x94t (ms)"), tr("Min \xce\x94t"),
        tr("Avg \xce\x94t"), tr("Max \xce\x94t"),
        tr("Jitter %")
    });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(22);
    m_table->setAlternatingRowColors(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addLayout(toolbar);
    layout->addWidget(m_table, 1);

    m_timer = new QTimer(this);
    m_timer->setInterval(500);
    m_timer->start();

    connect(m_timer,    &QTimer::timeout,      this, &TemporalPanel::updateTable);
    connect(m_clearBtn, &QPushButton::clicked, this, &TemporalPanel::onClear);
}

void TemporalPanel::onFrameReceived(const socketspy::core::CanFrame& frame) {
    const uint32_t id  = frame.id;
    const qint64   now = static_cast<qint64>(frame.timestamp_us);

    m_count[id]++;

    if (m_lastTs.contains(id)) {
        const double dt = static_cast<double>(now - m_lastTs[id]) / 1000.0; // us -> ms
        if (dt > 0.0) {
            auto& vec = m_intervals[id];
            vec.append(dt);
            if (vec.size() > 100)
                vec.removeFirst();
        }
    }
    m_lastTs[id] = now;
}

void TemporalPanel::updateTable() {
    // Collect IDs that have at least 2 intervals
    struct Row {
        uint32_t id;
        int      count;
        double   last;
        double   minDt;
        double   avgDt;
        double   maxDt;
        double   jitter;
    };

    QVector<Row> rows;
    rows.reserve(m_intervals.size());

    for (auto it = m_intervals.cbegin(); it != m_intervals.cend(); ++it) {
        const QVector<double>& vec = it.value();
        if (vec.size() < 2) continue;

        const uint32_t id = it.key();

        double minDt = vec[0], maxDt = vec[0], sum = 0.0;
        for (double v : vec) {
            if (v < minDt) minDt = v;
            if (v > maxDt) maxDt = v;
            sum += v;
        }
        const double mean = sum / static_cast<double>(vec.size());

        double variance = 0.0;
        for (double v : vec)
            variance += (v - mean) * (v - mean);
        const double stddev = std::sqrt(variance / static_cast<double>(vec.size()));
        const double jitter = (mean > 0.0) ? (stddev / mean * 100.0) : 0.0;

        rows.append({id,
                     m_count.value(id, 0),
                     vec.last(),
                     minDt, mean, maxDt, jitter});
    }

    // Sort by Count descending
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        return a.count > b.count;
    });

    m_table->setRowCount(static_cast<int>(rows.size()));

    auto setCell = [&](int row, int col, const QString& text) {
        auto* item = new QTableWidgetItem(text);
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(row, col, item);
    };

    int row = 0;
    for (const Row& r : rows) {
        setCell(row, 0, QString("%1").arg(r.id, 8, 16, QChar('0')).toUpper());
        setCell(row, 1, QString::number(r.count));
        setCell(row, 2, QString::number(r.last,   'f', 2));
        setCell(row, 3, QString::number(r.minDt,  'f', 2));
        setCell(row, 4, QString::number(r.avgDt,  'f', 2));
        setCell(row, 5, QString::number(r.maxDt,  'f', 2));
        setCell(row, 6, QString::number(r.jitter, 'f', 1));
        ++row;
    }
}

void TemporalPanel::onClear() {
    m_lastTs.clear();
    m_intervals.clear();
    m_count.clear();
    m_table->setRowCount(0);
}

} // namespace socketspy::gui
