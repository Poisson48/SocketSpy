// dbc_helper.h must be included before any Qt headers to avoid the
// `signals` macro collision with socketspy::dbc::Message::signals.
#include "dbc_helper.h"
#include "signal_validator_panel.h"
#include "cancore.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QColor>
#include <QBrush>
#include <span>
#include <algorithm>
#include <utility>

using namespace socketspy::core;

namespace socketspy::gui {

namespace {

// Strip the SocketCAN EFF/RTR/ERR flag bits so the value matches the
// DBC-stored message id (which never carries those flags).
inline uint32_t maskId(uint32_t raw) {
    return raw & 0x1FFFFFFFu;
}

QString kindText(FindingKind k) {
    switch (k) {
        case FindingKind::OutOfRange:     return SignalValidatorPanel::tr("Out of range");
        case FindingKind::UndocumentedId: return SignalValidatorPanel::tr("Undocumented ID");
        case FindingKind::DlcMismatch:    return SignalValidatorPanel::tr("DLC mismatch");
    }
    return {};
}

// Background colour for a finding's row, by severity.
QColor severityColor(FindingSeverity sev) {
    switch (sev) {
        case FindingSeverity::Error:   return QColor(120, 32, 32);   // deep red
        case FindingSeverity::Warning: return QColor(120, 96, 24);   // amber
        case FindingSeverity::Info:    return QColor(40, 64, 96);    // muted blue
    }
    return QColor(48, 48, 48);
}

} // namespace

SignalValidatorPanel::SignalValidatorPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void SignalValidatorPanel::setupUi() {
    m_pauseBtn = new QPushButton(tr("Pause"), this);
    m_pauseBtn->setCheckable(true);
    m_clearBtn = new QPushButton(tr("Clear"), this);

    m_dbcLabel = new QLabel(tr("No DBC loaded"), this);

    // ---- Counter strip ----
    m_errorCountLabel = new QLabel(tr("Errors: 0"), this);
    m_warnCountLabel  = new QLabel(tr("Warnings: 0"), this);
    m_okCountLabel    = new QLabel(tr("OK frames: 0 / 0"), this);

    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(12);
    toolbar->addWidget(m_dbcLabel);
    toolbar->addSpacing(16);
    toolbar->addWidget(m_errorCountLabel);
    toolbar->addWidget(m_warnCountLabel);
    toolbar->addWidget(m_okCountLabel);
    toolbar->addStretch();
    toolbar->addWidget(m_pauseBtn);
    toolbar->addWidget(m_clearBtn);

    // ---- Findings table ----
    m_table = new QTableWidget(0, 6, this);
    m_table->setHorizontalHeaderLabels({
        tr("ID (hex)"), tr("Signal / Message"),
        tr("Observed"), tr("Expected"),
        tr("Issue"), tr("Hits")
    });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->resizeSection(1, 220);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(24);
    m_table->setAlternatingRowColors(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addLayout(toolbar);
    layout->addWidget(m_table, 1);

    // Read-only analysis: a periodic refresh keeps the UI cheap under heavy bus
    // load instead of rebuilding the table on every single frame.
    m_timer = new QTimer(this);
    m_timer->setInterval(400);
    m_timer->start();

    connect(m_timer,    &QTimer::timeout,      this, &SignalValidatorPanel::refreshTable);
    connect(m_clearBtn, &QPushButton::clicked, this, &SignalValidatorPanel::onClear);
    connect(m_pauseBtn, &QPushButton::toggled, this, &SignalValidatorPanel::onTogglePause);

    updateCounters();
}

void SignalValidatorPanel::onDbcLoaded(socketspy::dbc::DbcDatabase db) {
    m_expected.clear();

    for (const auto& msg : db.messages) {
        ExpectedMessage em;
        em.id   = maskId(msg.id);
        em.name = QString::fromStdString(msg.name);
        em.dlc  = msg.dlc;

        // The DBC Message member is literally named `signals`, which clashes
        // with the Qt `signals` keyword macro that is active here. Disable the
        // macro around this member-access loop, mirroring the guard in
        // main_window.h, then restore it afterwards.
#pragma push_macro("signals")
#undef signals
        const auto& msgSignals = msg.signals;
#pragma pop_macro("signals")
        for (const auto& sig : msgSignals) {
            ExpectedSignal es;
            es.name          = QString::fromStdString(sig.name);
            es.factor        = sig.factor;
            es.offset        = sig.offset;
            es.min_val       = sig.min_val;
            es.max_val       = sig.max_val;
            es.start_bit     = sig.start_bit;
            es.bit_length    = sig.bit_length;
            es.little_endian = (sig.byte_order == socketspy::dbc::ByteOrder::LittleEndian);
            es.unit          = QString::fromStdString(sig.unit);
            es.has_range     = (sig.max_val > sig.min_val);
            em.signals_.push_back(es);
        }
        m_expected.insert(em.id, em);
    }

    m_dbc       = std::move(db);
    m_dbcLoaded = !m_expected.isEmpty();
    m_dbcLabel->setText(m_dbcLoaded
        ? tr("DBC loaded: %1 messages").arg(m_expected.size())
        : tr("No DBC loaded"));

    // A new DBC invalidates every prior finding.
    onClear();
}

void SignalValidatorPanel::onFrameReceived(const socketspy::core::CanFrame& frame) {
    if (m_paused) return;

    // Without a DBC there is nothing to validate against.
    if (!m_dbcLoaded) return;

    // Ignore bus-error frames — they carry no payload to decode.
    if (frame.flags & static_cast<uint8_t>(FrameFlags::Error)) return;

    ++m_framesSeen;

    const uint32_t id = maskId(frame.id);
    auto it = m_expected.constFind(id);

    // --- Undocumented ID: no DBC message matches this observed ID. ---
    if (it == m_expected.constEnd()) {
        addFinding(FindingKind::UndocumentedId, FindingSeverity::Error,
                   id,
                   QString("0x%1").arg(id, 0, 16).toUpper(),
                   tr("DLC %1").arg(frame.dlc),
                   tr("(not in DBC)"),
                   kindText(FindingKind::UndocumentedId),
                   0.0);
        return;
    }

    const ExpectedMessage& em = it.value();
    bool frameClean = true;

    // --- DLC mismatch: observed length differs from the DBC definition. ---
    if (em.dlc != 0 && frame.dlc != em.dlc) {
        frameClean = false;
        addFinding(FindingKind::DlcMismatch, FindingSeverity::Warning,
                   id,
                   em.name.isEmpty()
                       ? QString("0x%1").arg(id, 0, 16).toUpper()
                       : em.name,
                   tr("DLC %1").arg(frame.dlc),
                   tr("DLC %1").arg(em.dlc),
                   kindText(FindingKind::DlcMismatch),
                   frame.dlc);
    }

    // --- Out-of-range: decode each signal and check [min, max]. ---
    std::span<const uint8_t> data(frame.data, frame.dlc);
    for (const ExpectedSignal& es : em.signals_) {
        if (!es.has_range) continue;  // no meaningful window to check

        // Reuse the shared decode routine; it applies factor/offset and returns
        // the physical value. Match against the masked id stored in the DBC.
        auto phys = socketspy::gui::dbc_helper::decode_signal(
            m_dbc, id, es.name.toStdString(), data);
        if (!phys.has_value()) continue;  // frame too short for this signal

        const double v = *phys;
        if (v < es.min_val || v > es.max_val) {
            frameClean = false;
            const QString unitSuffix = es.unit.isEmpty()
                ? QString() : (" " + es.unit);
            const QString subject = (em.name.isEmpty()
                ? QString("0x%1").arg(id, 0, 16).toUpper()
                : em.name) + "::" + es.name;
            addFinding(FindingKind::OutOfRange, FindingSeverity::Error,
                       id, subject,
                       QString::number(v, 'g', 6) + unitSuffix,
                       QString("[%1, %2]%3")
                           .arg(es.min_val, 0, 'g', 6)
                           .arg(es.max_val, 0, 'g', 6)
                           .arg(unitSuffix),
                       kindText(FindingKind::OutOfRange),
                       v);
        }
    }

    if (frameClean)
        ++m_framesOk;
}

void SignalValidatorPanel::addFinding(FindingKind kind, FindingSeverity sev,
                                      uint32_t id, const QString& subject,
                                      const QString& observed, const QString& expected,
                                      const QString& issue, double value) {
    const QString key = QString::number(static_cast<int>(kind)) + "|" +
                        QString::number(id) + "|" + subject;
    auto it = m_findings.find(key);
    if (it == m_findings.end()) {
        Finding f;
        f.kind      = kind;
        f.severity  = sev;
        f.id        = id;
        f.subject   = subject;
        f.observed  = observed;
        f.expected  = expected;
        f.issue     = issue;
        f.count     = 1;
        f.lastValue = value;
        m_findings.insert(key, f);
    } else {
        Finding& f = it.value();
        ++f.count;
        f.observed  = observed;
        f.lastValue = value;
    }
    m_dirty = true;
}

void SignalValidatorPanel::refreshTable() {
    updateCounters();

    if (!m_dirty) return;
    m_dirty = false;

    // Order findings: errors first, then warnings, then by hit count.
    QVector<const Finding*> rows;
    rows.reserve(m_findings.size());
    for (auto it = m_findings.cbegin(); it != m_findings.cend(); ++it)
        rows.push_back(&it.value());

    std::sort(rows.begin(), rows.end(), [](const Finding* a, const Finding* b) {
        if (a->severity != b->severity)
            return static_cast<int>(a->severity) > static_cast<int>(b->severity);
        return a->count > b->count;
    });

    m_table->setUpdatesEnabled(false);
    m_table->setRowCount(static_cast<int>(rows.size()));

    int row = 0;
    for (const Finding* f : rows) {
        const QColor bg = severityColor(f->severity);

        auto setCell = [&](int col, const QString& text, int align) {
            auto* item = m_table->item(row, col);
            if (!item) {
                item = new QTableWidgetItem;
                m_table->setItem(row, col, item);
            }
            item->setText(text);
            item->setTextAlignment(static_cast<Qt::Alignment>(align) | Qt::AlignVCenter);
            item->setBackground(QBrush(bg));
        };

        setCell(0, QString("0x%1").arg(f->id, 0, 16).toUpper(), Qt::AlignLeft);
        setCell(1, f->subject,  Qt::AlignLeft);
        setCell(2, f->observed, Qt::AlignRight);
        setCell(3, f->expected, Qt::AlignRight);
        setCell(4, f->issue,    Qt::AlignLeft);
        setCell(5, QString::number(f->count), Qt::AlignRight);
        ++row;
    }

    m_table->setUpdatesEnabled(true);
}

void SignalValidatorPanel::updateCounters() {
    int errors = 0, warnings = 0;
    for (auto it = m_findings.cbegin(); it != m_findings.cend(); ++it) {
        switch (it.value().severity) {
            case FindingSeverity::Error:   ++errors;   break;
            case FindingSeverity::Warning: ++warnings; break;
            case FindingSeverity::Info:                break;
        }
    }
    m_errorCountLabel->setText(tr("Errors: %1").arg(errors));
    m_warnCountLabel->setText(tr("Warnings: %1").arg(warnings));
    m_okCountLabel->setText(tr("OK frames: %1 / %2")
        .arg(m_framesOk).arg(m_framesSeen));
}

void SignalValidatorPanel::onClear() {
    m_findings.clear();
    m_framesSeen = 0;
    m_framesOk   = 0;
    m_table->setRowCount(0);
    m_dirty = false;
    updateCounters();
}

void SignalValidatorPanel::onTogglePause() {
    m_paused = m_pauseBtn->isChecked();
    m_pauseBtn->setText(m_paused ? tr("Resume") : tr("Pause"));
}

} // namespace socketspy::gui
