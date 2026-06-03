#include "busload_dashboard_panel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPolygonF>
#include <QColor>
#include <algorithm>

#include "gui_palette.h"

namespace socketspy::gui {

// ───────────────────────────────────────────────────────────────────────────
// BusLoadSparkline
// ───────────────────────────────────────────────────────────────────────────

BusLoadSparkline::BusLoadSparkline(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(64);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void BusLoadSparkline::push(double value) {
    if (value < 0.0) value = 0.0;
    m_history.append(value);
    while (m_history.size() > m_maxPoints)
        m_history.removeFirst();
    update();
}

void BusLoadSparkline::clear() {
    m_history.clear();
    update();
}

void BusLoadSparkline::paintEvent(QPaintEvent* /*ev*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(1, 1, -1, -1);

    // Background + frame.
    p.fillRect(rect(), QColor(0, 0, 0, 30));
    p.setPen(QPen(QColor(Palette::kDeadGray), 1));
    p.drawRect(r);

    if (m_history.size() < 2)
        return;

    double maxV = 0.0;
    for (double v : m_history)
        maxV = std::max(maxV, v);
    if (maxV <= 0.0)
        maxV = 1.0;

    const int    n  = m_history.size();
    const double dx = r.width() / static_cast<double>(n - 1);

    QPolygonF line;
    line.reserve(n);
    for (int i = 0; i < n; ++i) {
        const double x = r.left() + dx * i;
        const double y = r.bottom() - (m_history[i] / maxV) * r.height();
        line << QPointF(x, y);
    }

    // Filled area under the curve.
    QPolygonF area = line;
    area << QPointF(r.right(),  r.bottom());
    area << QPointF(r.left(),   r.bottom());

    QColor fill(Palette::kSigIndigo);
    fill.setAlpha(60);
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawPolygon(area);

    p.setPen(QPen(QColor(Palette::kSigIndigo), 2));
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(line);
}

// ───────────────────────────────────────────────────────────────────────────
// BusLoadDashboardPanel
// ───────────────────────────────────────────────────────────────────────────

namespace {

// Friendly label per ErrorType — index = static_cast<int>(ErrorType).
const char* errorTypeName(int idx) {
    switch (idx) {
        case static_cast<int>(socketspy::core::ErrorType::None):       return "None";
        case static_cast<int>(socketspy::core::ErrorType::BitError):   return "Bit Error";
        case static_cast<int>(socketspy::core::ErrorType::StuffError): return "Stuff Error";
        case static_cast<int>(socketspy::core::ErrorType::FormError):  return "Form Error";
        case static_cast<int>(socketspy::core::ErrorType::AckError):   return "Ack Error";
        case static_cast<int>(socketspy::core::ErrorType::CrcError):   return "CRC Error";
        case static_cast<int>(socketspy::core::ErrorType::BusOff):     return "Bus Off";
        case static_cast<int>(socketspy::core::ErrorType::BusError):   return "Bus Error";
        default:                                                       return "?";
    }
}

} // namespace

BusLoadDashboardPanel::BusLoadDashboardPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void BusLoadDashboardPanel::setupUi() {
    // --- Helper to build a "big number" metric card -------------------------
    auto makeCard = [this](const QString& title, const QString& accent,
                           QLabel** valueOut) -> QGroupBox* {
        auto* box = new QGroupBox(title, this);
        auto* v   = new QVBoxLayout(box);
        v->setContentsMargins(10, 6, 10, 8);
        v->setSpacing(2);

        auto* value = new QLabel(QStringLiteral("0"), box);
        QFont f = value->font();
        f.setPointSizeF(f.pointSizeF() * 2.2);
        f.setBold(true);
        value->setFont(f);
        value->setStyleSheet(QStringLiteral("color:%1;").arg(accent));
        value->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        v->addWidget(value);
        *valueOut = value;
        return box;
    };

    auto* cards   = new QGridLayout;
    cards->setSpacing(8);
    cards->addWidget(makeCard(tr("Frames / s"),     Palette::kSigGreen,  &m_fpsValue),     0, 0);
    cards->addWidget(makeCard(tr("Peak fps"),       Palette::kSigCyan,   &m_peakFpsValue), 0, 1);
    cards->addWidget(makeCard(tr("Bus Load"),       Palette::kSigAmber,  &m_loadValue),    0, 2);
    cards->addWidget(makeCard(tr("Unique IDs"),     Palette::kSigIndigo, &m_uniqueValue),  0, 3);
    cards->addWidget(makeCard(tr("Error Frames"),   Palette::kSigRed,    &m_errorValue),   0, 4);
    cards->addWidget(makeCard(tr("Bitrate"),        Palette::kSigViolet, &m_bitrateValue), 0, 5);
    m_bitrateValue->setText(QString::number(m_bitrate));

    // --- Sparkline of bus-load history --------------------------------------
    auto* sparkBox = new QGroupBox(tr("Bus-Load History (%)"), this);
    auto* sparkLay = new QVBoxLayout(sparkBox);
    sparkLay->setContentsMargins(8, 6, 8, 8);
    m_sparkline = new BusLoadSparkline(sparkBox);
    sparkLay->addWidget(m_sparkline);

    // --- Error breakdown table ----------------------------------------------
    m_errorTable = new QTableWidget(0, 2, this);
    m_errorTable->setHorizontalHeaderLabels({ tr("Error Type"), tr("Count") });
    m_errorTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_errorTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_errorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_errorTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_errorTable->verticalHeader()->hide();
    m_errorTable->verticalHeader()->setDefaultSectionSize(22);
    m_errorTable->setAlternatingRowColors(true);

    auto* errBox = new QGroupBox(tr("Error Breakdown"), this);
    auto* errLay = new QVBoxLayout(errBox);
    errLay->setContentsMargins(8, 6, 8, 8);
    errLay->addWidget(m_errorTable);

    // Pre-populate one row per ErrorType (skip None at index 0).
    m_errorTable->setRowCount(kNumErrorTypes - 1);
    for (int i = 1; i < kNumErrorTypes; ++i) {
        auto* name = new QTableWidgetItem(tr(errorTypeName(i)));
        m_errorTable->setItem(i - 1, 0, name);
        auto* cnt = new QTableWidgetItem(QStringLiteral("0"));
        cnt->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_errorTable->setItem(i - 1, 1, cnt);
    }

    // --- Toolbar ------------------------------------------------------------
    m_clearBtn = new QPushButton(tr("Clear"), this);
    auto* toolbar = new QHBoxLayout;
    toolbar->addWidget(m_clearBtn);
    toolbar->addStretch();

    // --- Assemble -----------------------------------------------------------
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    layout->addLayout(toolbar);
    layout->addLayout(cards);
    layout->addWidget(sparkBox);
    layout->addWidget(errBox, 1);

    // --- Timer --------------------------------------------------------------
    m_timer = new QTimer(this);
    m_timer->setInterval(250);
    m_timer->start();

    connect(m_timer,    &QTimer::timeout,      this, &BusLoadDashboardPanel::onTick);
    connect(m_clearBtn, &QPushButton::clicked, this, &BusLoadDashboardPanel::onClear);
}

uint64_t BusLoadDashboardPanel::estimateBits(const socketspy::core::CanFrame& frame) {
    const uint8_t dlc = frame.dlc;
    // For classic CAN, dlc maps directly to byte count (0-8). FD-encoded DLCs
    // (9-15) are clamped so the estimate stays sane for either frame kind.
    const uint64_t dataBytes = (dlc <= 8) ? dlc : 8;

    const bool extended =
        (frame.id & 0x80000000u) != 0u ||      // SocketCAN EFF flag, if set
        (frame.id & ~0x7FFu) != 0u;            // ID needs more than 11 bits

    const uint64_t overhead = extended ? 67u : 47u;
    return overhead + dataBytes * 8u;
}

void BusLoadDashboardPanel::onFrameReceived(const socketspy::core::CanFrame& frame) {
    ++m_totalFrames;
    ++m_framesInWindow;
    m_bitsInWindow += estimateBits(frame);

    // Track unique IDs (mask off the SocketCAN flag bits so 11/29-bit ids match).
    m_uniqueIds.insert(frame.id & 0x1FFFFFFFu);

    const socketspy::core::ErrorType et = socketspy::core::classify_error(frame);
    if (et != socketspy::core::ErrorType::None) {
        ++m_totalErrors;
        const int idx = static_cast<int>(et);
        if (idx > 0 && idx < kNumErrorTypes)
            ++m_errorCounts[idx];
    }
}

void BusLoadDashboardPanel::onTick() {
    // Window length in seconds (timer interval is fixed at 250 ms).
    constexpr double kWindowSec = 0.250;

    const double fps = static_cast<double>(m_framesInWindow) / kWindowSec;
    m_peakFps = std::max(m_peakFps, fps);

    // Bus load: estimated bits transmitted this window / channel capacity.
    const double capacityBits = static_cast<double>(m_bitrate) * kWindowSec;
    double load = (capacityBits > 0.0)
                      ? (static_cast<double>(m_bitsInWindow) / capacityBits) * 100.0
                      : 0.0;
    load = std::clamp(load, 0.0, 100.0);

    m_fpsValue->setText(QString::number(fps, 'f', 0));
    m_peakFpsValue->setText(QString::number(m_peakFps, 'f', 0));
    m_loadValue->setText(QString::number(load, 'f', 1) + QStringLiteral(" %"));
    m_uniqueValue->setText(QString::number(m_uniqueIds.size()));
    m_errorValue->setText(QString::number(m_totalErrors));

    m_sparkline->push(load);

    // Refresh the per-error-type counts (rows 0..kNumErrorTypes-2 → types 1..N).
    for (int i = 1; i < kNumErrorTypes; ++i) {
        if (auto* item = m_errorTable->item(i - 1, 1))
            item->setText(QString::number(m_errorCounts[i]));
    }

    // Reset the rolling window accumulators.
    m_framesInWindow = 0;
    m_bitsInWindow   = 0;
}

void BusLoadDashboardPanel::setBitrate(int bitrate) {
    if (bitrate <= 0)
        return;
    m_bitrate = static_cast<uint64_t>(bitrate);
    if (m_bitrateValue)
        m_bitrateValue->setText(QString::number(m_bitrate));
}

void BusLoadDashboardPanel::onClear() {
    m_framesInWindow = 0;
    m_bitsInWindow   = 0;
    m_totalFrames    = 0;
    m_totalErrors    = 0;
    m_peakFps        = 0.0;
    m_uniqueIds.clear();
    std::fill(std::begin(m_errorCounts), std::end(m_errorCounts), 0u);

    m_fpsValue->setText(QStringLiteral("0"));
    m_peakFpsValue->setText(QStringLiteral("0"));
    m_loadValue->setText(QStringLiteral("0.0 %"));
    m_uniqueValue->setText(QStringLiteral("0"));
    m_errorValue->setText(QStringLiteral("0"));

    for (int i = 1; i < kNumErrorTypes; ++i) {
        if (auto* item = m_errorTable->item(i - 1, 1))
            item->setText(QStringLiteral("0"));
    }
    m_sparkline->clear();
}

} // namespace socketspy::gui
