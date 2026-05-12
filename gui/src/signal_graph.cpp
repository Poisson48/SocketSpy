#include "signal_graph.h"
#include "dbc_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <algorithm>
#include <span>

using namespace socketspy::core;
using namespace socketspy::dbc;

namespace socketspy::gui {

SignalGraphPanel::SignalGraphPanel(QWidget* parent) : QWidget(parent) {
    m_dbc = std::make_unique<DbcDatabase>();
    setupUi();
}

SignalGraphPanel::~SignalGraphPanel() = default;

void SignalGraphPanel::setupUi() {
    m_chart = new QChart;
    m_chart->setTitle("Signal Traces");
    m_chart->legend()->setVisible(true);

    m_axisX = new QValueAxis(m_chart);
    m_axisX->setTitleText("Time (s)");
    m_axisX->setRange(0.0, kWindowSec);

    m_axisY = new QValueAxis(m_chart);
    m_axisY->setTitleText("Value");
    m_axisY->setRange(0.0, 1.0);

    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);

    m_view = new QChartView(m_chart, this);
    m_view->setRenderHint(QPainter::Antialiasing);

    m_clearBtn = new QPushButton("Clear All", this);
    connect(m_clearBtn, &QPushButton::clicked,
            this, &SignalGraphPanel::onClearAll);

    m_scrollTimer = new QTimer(this);
    m_scrollTimer->setInterval(200);
    connect(m_scrollTimer, &QTimer::timeout,
            this, &SignalGraphPanel::onScrollAxis);

    auto* toolbar = new QHBoxLayout;
    toolbar->addWidget(m_clearBtn);
    toolbar->addStretch();

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(toolbar);
    layout->addWidget(m_view);
}

void SignalGraphPanel::onDbcLoaded(DbcDatabase db) {
    *m_dbc      = std::move(db);
    m_dbcLoaded = true;
}

void SignalGraphPanel::addSignal(QString signalName, uint32_t msgId) {
    if (!m_dbcLoaded) return;
    if (static_cast<int>(m_traces.size()) >= kMaxTraces) return;

    for (const auto& t : m_traces)
        if (t.msgId == msgId && t.signalName == signalName.toStdString()) return;

    double minVal = 0.0, maxVal = 1.0;
    dbc_helper::signal_range(*m_dbc, msgId, signalName.toStdString(),
                             minVal, maxVal);

    auto* series = new QLineSeries(m_chart);
    series->setName(signalName);
    m_chart->addSeries(series);
    series->attachAxis(m_axisX);
    series->attachAxis(m_axisY);

    m_traces.push_back({signalName.toStdString(), msgId, series,
                        0.0, minVal, maxVal});
    rescaleY();
    if (!m_scrollTimer->isActive()) m_scrollTimer->start();
}

void SignalGraphPanel::onFrameReceived(CanFrame frame) {
    if (!m_dbcLoaded || m_traces.empty()) return;

    double timeSec = static_cast<double>(frame.timestamp_us) / 1e6;
    if (m_firstFrame) {
        m_startTimeSec = timeSec;
        m_firstFrame   = false;
    }
    double relT = timeSec - m_startTimeSec;

    std::span<const uint8_t> data(frame.data, frame.dlc);
    for (auto& trace : m_traces) {
        if (trace.msgId != frame.id) continue;
        auto val = dbc_helper::decode_signal(
            *m_dbc, frame.id, trace.signalName, data);
        if (!val) continue;
        trace.lastValue = *val;
        trace.series->append(relT, *val);
        // Keep series bounded: drop older points beyond window
        while (trace.series->count() > 1200) {
            auto pts = trace.series->points();
            trace.series->remove(pts.first());
        }
    }
}

void SignalGraphPanel::onClearAll() {
    for (auto& t : m_traces)
        t.series->clear();
    m_firstFrame = true;
    m_scrollTimer->stop();
}

void SignalGraphPanel::onScrollAxis() {
    if (m_firstFrame) return;
    double maxT = 0.0;
    for (const auto& t : m_traces) {
        if (t.series->count() > 0)
            maxT = std::max(maxT, t.series->points().last().x());
    }
    double lo = std::max(0.0, maxT - kWindowSec);
    m_axisX->setRange(lo, lo + kWindowSec);
    rescaleY();
}

void SignalGraphPanel::rescaleY() {
    if (m_traces.empty()) { m_axisY->setRange(0.0, 1.0); return; }
    double lo = m_traces[0].minVal, hi = m_traces[0].maxVal;
    for (const auto& t : m_traces) {
        lo = std::min(lo, t.minVal);
        hi = std::max(hi, t.maxVal);
    }
    if (hi <= lo) hi = lo + 1.0;
    m_axisY->setRange(lo, hi);
}

} // namespace socketspy::gui
