#include "signal_graph.h"
#include "dbc_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QInputDialog>
#include <QMenu>
#include <QMouseEvent>
#include <QResizeEvent>
#include <algorithm>
#include <span>

using namespace socketspy::core;
using namespace socketspy::dbc;

// Permanently undef Qt's `signals` macro so we can access dbc::Message::signals.
#include "dbc_compat.h"
#include "gui_palette.h"

static std::vector<std::string> msgSignalNames(const DbcDatabase& dbc, uint32_t id) {
    for (const auto& msg : dbc.messages)
        if (msg.id == id) {
            std::vector<std::string> r;
            for (const auto& s : msg.signals) r.push_back(s.name);
            return r;
        }
    return {};
}

namespace socketspy::gui {

// ── GraphChartView ──────────────────────────────────────────────────────────

GraphChartView::GraphChartView(QChart* chart, QWidget* parent)
    : QChartView(chart, parent) {}

void GraphChartView::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::RightButton)
        emit rightClickedAt(QPointF(e->pos()));
    else
        QChartView::mousePressEvent(e);
}

void GraphChartView::resizeEvent(QResizeEvent* e) {
    QChartView::resizeEvent(e);
    emit resized();
}

// ── SignalGraphPanel ────────────────────────────────────────────────────────

SignalGraphPanel::SignalGraphPanel(QWidget* parent) : QWidget(parent) {
    m_dbc = std::make_unique<DbcDatabase>();
    setupUi();
}

SignalGraphPanel::~SignalGraphPanel() = default;

void SignalGraphPanel::setupUi() {
    m_chart = new QChart;
    m_chart->legend()->setVisible(true);
    m_chart->setMargins(QMargins(8, 8, 8, 8));
    m_chart->setAnimationOptions(QChart::NoAnimation);

    m_axisX = new QValueAxis(m_chart);
    m_axisX->setTitleText("Time (s)");
    m_axisX->setRange(0.0, kWindowSec);
    m_axisX->setTickCount(6);
    m_axisX->setMinorTickCount(1);

    m_axisY = new QValueAxis(m_chart);
    m_axisY->setTitleText("Value");
    m_axisY->setRange(0.0, 1.0);

    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);

    applyChartTheme();

    m_view = new GraphChartView(m_chart, this);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setContextMenuPolicy(Qt::PreventContextMenu);
    connect(m_view, &GraphChartView::rightClickedAt,
            this, &SignalGraphPanel::onChartRightClick);
    connect(m_view, &GraphChartView::resized,
            this, &SignalGraphPanel::updateMarkerPositions);

    m_clearBtn = new QPushButton("Clear All", this);
    m_clearBtn->setObjectName("clearBtn");
    connect(m_clearBtn, &QPushButton::clicked, this, &SignalGraphPanel::onClearAll);

    m_markerBtn = new QPushButton("\xe2\x8a\x95 Marker", this);
    m_markerBtn->setProperty("secondary", true);
    connect(m_markerBtn, &QPushButton::clicked, this, &SignalGraphPanel::onAddMarkerNow);

    m_scrollTimer = new QTimer(this);
    m_scrollTimer->setInterval(200);
    connect(m_scrollTimer, &QTimer::timeout, this, &SignalGraphPanel::onScrollAxis);

    auto* toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(8, 4, 8, 0);
    toolbar->setSpacing(6);
    toolbar->addWidget(m_clearBtn);
    toolbar->addWidget(m_markerBtn);
    toolbar->addStretch();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(toolbar);
    layout->addWidget(m_view);
}

void SignalGraphPanel::styleAxis(QValueAxis* ax) const {
    ax->setLabelsBrush(QBrush(QColor("#7c8fa6")));
    ax->setTitleBrush(QBrush(QColor("#7c8fa6")));
    ax->setLinePen(QPen(QColor("#3d5270"), 1));
    ax->setGridLinePen(QPen(QColor("#2a3a52"), 1, Qt::DashLine));
    ax->setMinorGridLinePen(QPen(QColor("#1d2a3d"), 1, Qt::DotLine));
    ax->setLabelsColor(QColor("#7c8fa6"));
    ax->setTitleFont(QFont("Segoe UI", 9));
    ax->setLabelsFont(QFont("Segoe UI", 9));
}

void SignalGraphPanel::applyChartTheme() {
    m_chart->setBackgroundBrush(QBrush(QColor("#1a2235")));
    m_chart->setBackgroundPen(QPen(Qt::NoPen));
    m_chart->setBackgroundRoundness(0);
    m_chart->setPlotAreaBackgroundBrush(QBrush(QColor("#111827")));
    m_chart->setPlotAreaBackgroundVisible(true);
    m_chart->setTitleBrush(QBrush(QColor("#f1f5f9")));

    m_chart->legend()->setBackgroundVisible(true);
    m_chart->legend()->setBrush(QBrush(QColor("#1a2235")));
    m_chart->legend()->setPen(QPen(QColor("#2a3a52")));
    m_chart->legend()->setLabelBrush(QBrush(QColor("#f1f5f9")));
    m_chart->legend()->setFont(QFont("Segoe UI", 9));

    styleAxis(m_axisX);
    styleAxis(m_axisY);
}

void SignalGraphPanel::onDbcLoaded(DbcDatabase db) {
    *m_dbc      = std::move(db);
    m_dbcLoaded = true;
}

void SignalGraphPanel::addTrace(TrackedSignal t) {
    if ((int)m_traces.size() >= kMaxTraces) return;
    const int idx = static_cast<int>(m_traces.size());
    const QColor color = Palette::kSigColors[idx % Palette::kNumSigColors];

    auto* series = new QLineSeries(m_chart);
    QPen pen(color);
    pen.setWidth(2);
    series->setPen(pen);
    series->setName(t.displayName());
    m_chart->addSeries(series);
    series->attachAxis(m_axisX);

    if (!t.isRaw) {
        // Per-signal Y axis with DBC-sourced range and unit
        auto* ay = new QValueAxis(m_chart);
        double lo = t.minVal, hi = t.maxVal;
        // If the DBC defines no range (both zero) or range is degenerate,
        // use a sensible default and let auto-scaling take over at runtime.
        if (lo == 0.0 && hi == 0.0) {
            lo = -100.0; hi = 100.0;
        } else if (hi <= lo) {
            // Boolean / single-value signal: force a visible unit range
            lo = std::min(lo, 0.0);
            hi = lo + 1.0;
        }
        ay->setRange(lo, hi);
        if (!t.unit.empty())
            ay->setTitleText(QString::fromStdString(t.unit));
        styleAxis(ay);
        ay->setLabelsBrush(QBrush(color));   // tint axis labels to match series
        ay->setTitleBrush(QBrush(color));
        ay->setLabelsColor(color);
        // Alternate left/right to reduce overlap when multiple signals present
        Qt::Alignment side = (idx % 2 == 0) ? Qt::AlignLeft : Qt::AlignRight;
        m_chart->addAxis(ay, side);
        series->attachAxis(ay);
        t.axisY = ay;
    } else {
        series->attachAxis(m_axisY);
        rescaleY();
    }

    t.series = series;
    m_traces.push_back(std::move(t));
    if (!m_scrollTimer->isActive()) m_scrollTimer->start();
}

void SignalGraphPanel::addSignal(QString signalName, uint32_t msgId) {
    if (!m_dbcLoaded) return;
    const std::string sigStd = signalName.toStdString();
    for (const auto& t : m_traces)
        if (!t.isRaw && t.msgId == msgId && t.signalName == sigStd) return;
    // Initialize to 0/0 so that addTrace() detects "no DBC range defined"
    // and falls back to the sensible default with auto-scaling enabled.
    double minVal = 0.0, maxVal = 0.0;
    dbc_helper::signal_range(*m_dbc, msgId, sigStd, minVal, maxVal);
    std::string unit = dbc_helper::signal_unit(*m_dbc, msgId, sigStd);
    addTrace({sigStd, {}, unit, msgId, nullptr, nullptr, 0.0, minVal, maxVal, false, 0});
}

void SignalGraphPanel::addRawSignal(uint32_t msgId, int byteIdx) {
    for (const auto& t : m_traces)
        if (t.isRaw && t.msgId == msgId && t.rawByteIdx == byteIdx) return;
    addTrace({"", {}, {}, msgId, nullptr, nullptr, 0.0, 0.0, 255.0, true, byteIdx});
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
        result.append({QString::fromStdString(t.signalName), t.label,
                        t.msgId, t.isRaw, t.rawByteIdx});
    return result;
}

void SignalGraphPanel::restoreSignals(const QList<GraphSignalConfig>& list,
                                      const QHash<QString,QString>& aliases) {
    onClearAll();
    for (const auto& s : list) {
        if (s.isRaw) addRawSignal(s.msgId, s.rawByteIdx);
        else         addSignal(s.name, s.msgId);
    }
    // Apply per-signal labels from config
    for (int i = 0; i < list.size() && i < (int)m_traces.size(); ++i) {
        if (!list[i].label.isEmpty()) {
            m_traces[i].label = list[i].label;
            m_traces[i].series->setName(m_traces[i].displayName());
        }
    }
    // Apply project-level aliases (canonical → display name)
    for (auto& t : m_traces) {
        const auto it = aliases.find(t.canonicalName());
        if (it != aliases.end() && !it.value().isEmpty()) {
            t.label = it.value();
            t.series->setName(t.displayName());
        }
    }
}

void SignalGraphPanel::renameTrace(int idx) {
    if (idx < 0 || idx >= (int)m_traces.size()) return;
    auto& t = m_traces[idx];
    bool ok;
    QString newLabel = QInputDialog::getText(
        this, tr("Rename Signal"),
        tr("New name for \"%1\":").arg(t.displayName()),
        QLineEdit::Normal, t.displayName(), &ok);
    if (!ok || newLabel.trimmed().isEmpty()) return;
    newLabel = newLabel.trimmed();
    t.label = newLabel;
    t.series->setName(newLabel);
    emit signalAliased(t.canonicalName(), newLabel);
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

        // Auto-scale the per-signal Y axis so that incoming values are always
        // visible.  We only ever expand the range — never shrink it — so that
        // historical points remain in view.
        if (!trace.isRaw && trace.axisY) {
            double axMin = trace.axisY->min();
            double axMax = trace.axisY->max();
            bool changed = false;
            if (val < axMin) {
                // Expand downward with a 10 % margin (or at least 1 unit).
                double span   = axMax - axMin;
                double margin = std::max(std::abs(val) * 0.1, span * 0.1);
                margin = std::max(margin, 1.0);
                trace.axisY->setMin(val - margin);
                changed = true;
            }
            if (val > axMax) {
                double span   = axMax - axMin;
                double margin = std::max(std::abs(val) * 0.1, span * 0.1);
                margin = std::max(margin, 1.0);
                trace.axisY->setMax(val + margin);
                changed = true;
            }
            // Keep the stored minVal/maxVal in sync so rescaleY() stays consistent
            if (changed) {
                trace.minVal = trace.axisY->min();
                trace.maxVal = trace.axisY->max();
            }
        }

        while (trace.series->count() > kMaxSeriesPts) {
            auto pts = trace.series->points();
            trace.series->remove(pts.first());
        }
    }
}

void SignalGraphPanel::onClearAll() {
    for (auto& t : m_traces) {
        m_chart->removeSeries(t.series);
        delete t.series;
        if (t.axisY) { m_chart->removeAxis(t.axisY); delete t.axisY; }
    }
    m_traces.clear();
    clearMarkers();
    m_firstFrame  = true;
    m_currentMaxT = 0.0;
    m_scrollTimer->stop();
    rescaleY();
}

void SignalGraphPanel::onScrollAxis() {
    if (m_firstFrame) return;
    double maxT = 0.0;
    for (const auto& t : m_traces)
        if (t.series->count() > 0) maxT = std::max(maxT, t.series->points().last().x());
    m_currentMaxT = maxT;
    double lo = std::max(0.0, maxT - kWindowSec);
    m_axisX->setRange(lo, lo + kWindowSec);
    // Per-signal axes are auto-scaled live in onFrameReceived(); only update the
    // shared fallback axis used by raw byte traces.
    rescaleY();
    updateMarkerPositions();
}

void SignalGraphPanel::rescaleY() {
    // Only the shared axis (used by raw traces) needs rescaling here.
    // DBC-decoded signals each have their own QValueAxis with a static range.
    double lo = 0.0, hi = 255.0;
    bool hasRaw = false;
    for (const auto& t : m_traces) {
        if (!t.isRaw) continue;
        if (!hasRaw) { lo = t.minVal; hi = t.maxVal; hasRaw = true; }
        else { lo = std::min(lo, t.minVal); hi = std::max(hi, t.maxVal); }
    }
    if (hi <= lo) hi = lo + 1.0;
    m_axisY->setRange(lo, hi);
}

void SignalGraphPanel::onChartRightClick(QPointF viewPos) {
    QPointF scenePos = m_view->mapToScene(viewPos.toPoint());
    QPointF dataVal  = m_chart->mapToValue(scenePos);

    QMenu menu(this);

    auto* addHere = menu.addAction(
        tr("\xe2\x8a\x95 Add marker at t=%.2fs…").arg(dataVal.x()));

    menu.addSeparator();

    QList<QAction*> renameActions;
    for (int i = 0; i < (int)m_traces.size(); ++i) {
        renameActions << menu.addAction(
            tr("\xe2\x9c\x8e Rename \"%1\"…").arg(m_traces[i].displayName()));
    }

    if (!m_markers.empty()) {
        menu.addSeparator();
        menu.addAction(tr("Clear Markers"));
    }

    QAction* act = menu.exec(m_view->mapToGlobal(viewPos.toPoint()));
    if (!act) return;

    const QString actText = act->text();

    if (act == addHere) {
        bool ok;
        QString label = QInputDialog::getText(
            this, tr("Add Marker"), tr("Marker label:"),
            QLineEdit::Normal, {}, &ok);
        if (ok && !label.trimmed().isEmpty())
            addMarker(m_startTimeSec + dataVal.x(), label.trimmed());
        return;
    }
    if (actText == tr("Clear Markers")) { clearMarkers(); return; }
    for (int i = 0; i < renameActions.size(); ++i) {
        if (act == renameActions[i]) { renameTrace(i); return; }
    }
}

void SignalGraphPanel::onAddMarkerNow() {
    if (m_firstFrame) return;
    bool ok;
    QString label = QInputDialog::getText(
        this, tr("Add Marker"), tr("Marker label:"),
        QLineEdit::Normal, {}, &ok);
    if (ok && !label.trimmed().isEmpty())
        addMarker(m_startTimeSec + m_currentMaxT, label.trimmed());
}

void SignalGraphPanel::addMarker(double timeSec, const QString& label) {
    const QColor color = Palette::kMarkerColors[m_markers.size() % Palette::kNumMarkerColors];

    auto* line = new QGraphicsLineItem;
    line->setPen(QPen(color, 1, Qt::DashLine));
    line->setZValue(20);
    m_view->scene()->addItem(line);

    auto* text = new QGraphicsTextItem(label);
    text->setDefaultTextColor(color);
    text->setFont(QFont("Segoe UI", 9, QFont::DemiBold));
    text->setZValue(21);
    m_view->scene()->addItem(text);

    m_markers.push_back({timeSec, label, color, line, text});
    updateMarkerPositions();
}

void SignalGraphPanel::clearMarkers() {
    for (auto& mk : m_markers) {
        if (mk.line) { m_view->scene()->removeItem(mk.line); delete mk.line; }
        if (mk.text) { m_view->scene()->removeItem(mk.text); delete mk.text; }
    }
    m_markers.clear();
}

void SignalGraphPanel::updateMarkerPositions() {
    if (m_markers.empty()) return;
    QRectF plotArea = m_chart->plotArea();
    for (auto& mk : m_markers) {
        double relT = mk.timeSec - m_startTimeSec;
        QPointF pos = m_chart->mapToPosition(QPointF(relT, 0.0));
        double x = pos.x();
        bool visible = (x >= plotArea.left() && x <= plotArea.right());
        mk.line->setVisible(visible);
        mk.text->setVisible(visible);
        if (visible) {
            mk.line->setLine(x, plotArea.top(), x, plotArea.bottom());
            mk.text->setPos(x + 3.0, plotArea.top() + 3.0);
        }
    }
}

} // namespace socketspy::gui
