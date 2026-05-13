#include "signal_graph.h"
#include "dbc_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <algorithm>
#include <span>

using namespace socketspy::core;
using namespace socketspy::dbc;

// Access Message::signals (field named "signals") without the Qt macro expanding it.
#pragma push_macro("signals")
#undef signals
static std::vector<std::string> msgSignalNames(const DbcDatabase& dbc, uint32_t id) {
    for (const auto& msg : dbc.messages)
        if (msg.id == id) {
            std::vector<std::string> r;
            for (const auto& s : msg.signals) r.push_back(s.name);
            return r;
        }
    return {};
}
#pragma pop_macro("signals")

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
    connect(m_clearBtn, &QPushButton::clicked, this, &SignalGraphPanel::onClearAll);

    m_scrollTimer = new QTimer(this);
    m_scrollTimer->setInterval(200);
    connect(m_scrollTimer, &QTimer::timeout, this, &SignalGraphPanel::onScrollAxis);

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

void SignalGraphPanel::addTrace(TrackedSignal t) {
    if ((int)m_traces.size() >= kMaxTraces) return;
    auto* series = new QLineSeries(m_chart);
    series->setName(t.isRaw
        ? QString("0x%1[B%2]").arg(t.msgId, 0, 16).toUpper().arg(t.rawByteIdx)
        : QString::fromStdString(t.signalName));
    m_chart->addSeries(series);
    series->attachAxis(m_axisX);
    series->attachAxis(m_axisY);
    t.series = series;
    m_traces.push_back(std::move(t));
    rescaleY();
    if (!m_scrollTimer->isActive()) m_scrollTimer->start();
}

void SignalGraphPanel::addSignal(QString signalName, uint32_t msgId) {
    if (!m_dbcLoaded) return;
    for (const auto& t : m_traces)
        if (!t.isRaw && t.msgId == msgId && t.signalName == signalName.toStdString()) return;
    double minVal = 0.0, maxVal = 1.0;
    dbc_helper::signal_range(*m_dbc, msgId, signalName.toStdString(), minVal, maxVal);
    addTrace({signalName.toStdString(), msgId, nullptr, 0.0, minVal, maxVal, false, 0});
}

void SignalGraphPanel::addRawSignal(uint32_t msgId, int byteIdx) {
    for (const auto& t : m_traces)
        if (t.isRaw && t.msgId == msgId && t.rawByteIdx == byteIdx) return;
    addTrace({"", msgId, nullptr, 0.0, 0.0, 255.0, true, byteIdx});
}

void SignalGraphPanel::addFrameSignals(uint32_t id) {
    if (m_dbcLoaded) {
        auto it = std::find_if(m_dbc->messages.begin(), m_dbc->messages.end(),
                               [id](const auto& m){ return m.id == id; });
        if (it != m_dbc->messages.end()) {
            for (const auto& name : msgSignalNames(*m_dbc, id))
                addSignal(QString::fromStdString(name), id);
            return;
        }
    }
    for (int i = 0; i < 8 && (int)m_traces.size() < kMaxTraces; ++i)
        addRawSignal(id, i);
}

QList<GraphSignalConfig> SignalGraphPanel::trackedSignals() const {
    QList<GraphSignalConfig> result;
    for (const auto& t : m_traces)
        result.append({QString::fromStdString(t.signalName), t.msgId, t.isRaw, t.rawByteIdx});
    return result;
}

void SignalGraphPanel::restoreSignals(const QList<GraphSignalConfig>& list) {
    onClearAll();
    for (const auto& s : list) {
        if (s.isRaw) addRawSignal(s.msgId, s.rawByteIdx);
        else         addSignal(s.name, s.msgId);
    }
}

void SignalGraphPanel::onFrameReceived(CanFrame frame) {
    if (m_traces.empty()) return;

    double timeSec = static_cast<double>(frame.timestamp_us) / 1e6;
    if (m_firstFrame) { m_startTimeSec = timeSec; m_firstFrame = false; }
    double relT = timeSec - m_startTimeSec;

    std::span<const uint8_t> data(frame.data, frame.dlc);
    for (auto& trace : m_traces) {
        if (trace.msgId != frame.id) continue;
        double val = 0.0;
        if (trace.isRaw) {
            if (trace.rawByteIdx >= frame.dlc) continue;
            val = frame.data[trace.rawByteIdx];
        } else {
            if (!m_dbcLoaded) continue;
            auto decoded = dbc_helper::decode_signal(*m_dbc, frame.id, trace.signalName, data);
            if (!decoded) continue;
            val = *decoded;
        }
        trace.lastValue = val;
        trace.series->append(relT, val);
        while (trace.series->count() > 1200) {
            auto pts = trace.series->points();
            trace.series->remove(pts.first());
        }
    }
}

void SignalGraphPanel::onClearAll() {
    for (auto& t : m_traces) { m_chart->removeSeries(t.series); delete t.series; }
    m_traces.clear();
    m_firstFrame = true;
    m_scrollTimer->stop();
    rescaleY();
}

void SignalGraphPanel::onScrollAxis() {
    if (m_firstFrame) return;
    double maxT = 0.0;
    for (const auto& t : m_traces)
        if (t.series->count() > 0) maxT = std::max(maxT, t.series->points().last().x());
    double lo = std::max(0.0, maxT - kWindowSec);
    m_axisX->setRange(lo, lo + kWindowSec);
    rescaleY();
}

void SignalGraphPanel::rescaleY() {
    if (m_traces.empty()) { m_axisY->setRange(0.0, 1.0); return; }
    double lo = m_traces[0].minVal, hi = m_traces[0].maxVal;
    for (const auto& t : m_traces) { lo = std::min(lo, t.minVal); hi = std::max(hi, t.maxVal); }
    if (hi <= lo) hi = lo + 1.0;
    m_axisY->setRange(lo, hi);
}

} // namespace socketspy::gui
