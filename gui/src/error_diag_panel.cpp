#include "error_diag_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QDateTime>
#include <QColor>
#include <QBrush>
#include <QFont>

namespace socketspy::gui {

using socketspy::core::CanFrame;
using socketspy::core::ErrorType;

namespace {

// Human-readable name for each ErrorType (indexed by enum value).
const char* errorTypeName(ErrorType t) {
    switch (t) {
        case ErrorType::None:       return "None";
        case ErrorType::BitError:   return "Bit Error";
        case ErrorType::StuffError: return "Stuff Error";
        case ErrorType::FormError:  return "Form Error";
        case ErrorType::AckError:   return "ACK Error";
        case ErrorType::CrcError:   return "CRC Error";
        case ErrorType::BusOff:     return "Bus-Off";
        case ErrorType::BusError:   return "Bus Error";
    }
    return "Unknown";
}

// A row colour to make the severity scannable at a glance.
QColor rowColour(ErrorType t) {
    switch (t) {
        case ErrorType::BusOff:    return QColor(120, 20, 20);   // deep red
        case ErrorType::BusError:  return QColor(110, 45, 20);   // dark orange
        case ErrorType::AckError:
        case ErrorType::CrcError:  return QColor(90, 70, 20);    // amber
        default:                   return QColor(60, 55, 30);    // muted
    }
}

// Critical faults that should raise the red banner.
bool isCritical(ErrorType t) {
    return t == ErrorType::BusOff || t == ErrorType::BusError;
}

QString rawBytes(const uint8_t* data, uint8_t dlc) {
    QString out;
    const int n = (dlc > 64) ? 64 : dlc;
    for (int i = 0; i < n; ++i) {
        if (i) out += ' ';
        out += QString("%1").arg(data[i], 2, 16, QChar('0')).toUpper();
    }
    return out;
}

} // namespace

ErrorDiagPanel::ErrorDiagPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void ErrorDiagPanel::setupUi() {
    // --- Banner (hidden until a critical fault appears) ---
    m_banner = new QLabel(this);
    m_banner->setWordWrap(true);
    m_banner->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_banner->setMargin(8);
    m_banner->setStyleSheet(
        "background:#7a1414; color:#ffffff; border:1px solid #c0392b;"
        "border-radius:4px; font-weight:bold;");
    m_banner->hide();

    // --- Summary strip ---
    m_summary = new QLabel(this);
    m_summary->setTextFormat(Qt::RichText);
    m_summary->setStyleSheet("padding:2px;");

    // --- Toolbar ---
    m_pauseBtn = new QPushButton(tr("Pause"), this);
    m_pauseBtn->setCheckable(true);
    m_clearBtn = new QPushButton(tr("Clear"), this);

    auto* toolbar = new QHBoxLayout;
    toolbar->addWidget(m_summary, 1);
    toolbar->addWidget(m_pauseBtn);
    toolbar->addWidget(m_clearBtn);

    // --- Table ---
    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({
        tr("Time"), tr("Error Type"), tr("CAN ID"), tr("Raw Bytes")
    });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(22);
    m_table->setAlternatingRowColors(false);

    // --- Main layout ---
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addWidget(m_banner);
    layout->addLayout(toolbar);
    layout->addWidget(m_table, 1);

    // --- 500 ms refresh / banner-decay timer ---
    m_timer = new QTimer(this);
    m_timer->setInterval(500);
    m_timer->start();

    connect(m_timer,    &QTimer::timeout,        this, &ErrorDiagPanel::refresh);
    connect(m_pauseBtn, &QPushButton::toggled,   this, &ErrorDiagPanel::onPauseToggled);
    connect(m_clearBtn, &QPushButton::clicked,   this, &ErrorDiagPanel::onClear);

    updateSummary();
}

void ErrorDiagPanel::onFrameReceived(const CanFrame& frame) {
    if (m_paused) return;

    const ErrorType type = socketspy::core::classify_error(frame);
    if (type == ErrorType::None) return;

    const auto idx = static_cast<size_t>(type);
    if (idx < m_counts.size()) ++m_counts[idx];
    ++m_totalErrors;

    ErrorEvent ev;
    ev.timestampUs = static_cast<qint64>(frame.timestamp_us);
    ev.type        = type;
    ev.id          = frame.id;
    ev.dlc         = frame.dlc;
    const int n = (frame.dlc > 64) ? 64 : frame.dlc;
    for (int i = 0; i < n; ++i) ev.data[static_cast<size_t>(i)] = frame.data[i];

    m_pending.append(ev);

    // Raise the banner immediately on a critical fault; the timer decays it.
    if (isCritical(type)) m_bannerTicks = 6;   // ~3 s at 500 ms/tick
}

void ErrorDiagPanel::refresh() {
    // --- Banner decay ---
    if (m_bannerTicks > 0) {
        --m_bannerTicks;

        // Pick the most severe critical type seen for the hint text.
        const bool busOff = (m_counts[static_cast<size_t>(ErrorType::BusOff)] > 0);
        QString hint;
        if (busOff) {
            hint = tr("BUS-OFF detected: check termination / bitrate, then "
                      "re-up the interface:\n"
                      "  sudo ip link set <iface> down && sudo ip link set <iface> up");
        } else {
            hint = tr("BUS ERROR detected: heavy error activity on the bus. "
                      "Check wiring, termination (120\xce\xa9) and that all nodes "
                      "share the same bitrate.");
        }
        m_banner->setText(hint);
        // Flash: alternate the banner background each tick so it pulses.
        const bool bright = (m_bannerTicks % 2) == 0;
        m_banner->setStyleSheet(
            QString("background:%1; color:#ffffff; border:1px solid #c0392b;"
                    "border-radius:4px; font-weight:bold;")
                .arg(bright ? "#c0392b" : "#7a1414"));
        m_banner->show();
    } else if (m_banner->isVisible()) {
        m_banner->hide();
    }

    // --- Drain pending events into the table ---
    if (!m_pending.isEmpty()) {
        m_table->setUpdatesEnabled(false);

        for (const ErrorEvent& ev : m_pending) {
            const int row = m_table->rowCount();
            m_table->insertRow(row);

            const QDateTime ts =
                QDateTime::fromMSecsSinceEpoch(ev.timestampUs / 1000);
            const QColor bg = rowColour(ev.type);

            auto setCell = [&](int col, const QString& text, bool mono) {
                auto* item = new QTableWidgetItem(text);
                item->setBackground(QBrush(bg));
                item->setForeground(QBrush(QColor(235, 235, 235)));
                if (mono) item->setFont(QFont("Monospace", 9));
                m_table->setItem(row, col, item);
            };

            setCell(0, ts.toString("HH:mm:ss.zzz"),               false);
            setCell(1, tr(errorTypeName(ev.type)),                false);
            setCell(2, QString("0x%1").arg(ev.id & 0x1FFFFFFFu, 0, 16).toUpper(), true);
            setCell(3, rawBytes(ev.data.data(), ev.dlc),          true);
        }

        m_pending.clear();

        // Cap at kMaxRows, dropping oldest from the top (scrolling buffer).
        while (m_table->rowCount() > kMaxRows)
            m_table->removeRow(0);

        m_table->setUpdatesEnabled(true);
        m_table->scrollToBottom();
    }

    updateSummary();
}

void ErrorDiagPanel::updateSummary() {
    // Build a compact rich-text strip of per-type counters.
    QString s = QString("<b>%1:</b> %2").arg(tr("Total errors")).arg(m_totalErrors);

    auto addType = [&](ErrorType t, const char* colour) {
        const quint64 c = m_counts[static_cast<size_t>(t)];
        if (c == 0) return;
        s += QString("&nbsp;&nbsp;<span style='color:%1'>%2: <b>%3</b></span>")
                 .arg(colour, tr(errorTypeName(t))).arg(c);
    };

    addType(ErrorType::BusOff,     "#ff6060");
    addType(ErrorType::BusError,   "#ff9050");
    addType(ErrorType::AckError,   "#ffd060");
    addType(ErrorType::CrcError,   "#ffd060");
    addType(ErrorType::BitError,   "#c0c0c0");
    addType(ErrorType::StuffError, "#c0c0c0");
    addType(ErrorType::FormError,  "#c0c0c0");

    m_summary->setText(s);
}

void ErrorDiagPanel::onPauseToggled(bool paused) {
    m_paused = paused;
    m_pauseBtn->setText(paused ? tr("Resume") : tr("Pause"));
}

void ErrorDiagPanel::onClear() {
    m_pending.clear();
    m_counts.fill(0);
    m_totalErrors = 0;
    m_bannerTicks = 0;
    m_banner->hide();
    m_table->setRowCount(0);
    updateSummary();
}

} // namespace socketspy::gui
