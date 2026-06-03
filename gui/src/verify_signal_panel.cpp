// dbc_helper.h must be included before any Qt headers to avoid the `signals`
// macro collision with socketspy::dbc::Message::signals.
#include "dbc_helper.h"
#include "verify_signal_panel.h"
#include "cancore.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QTimer>
#include <QTableWidget>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QDateTime>
#include <QColor>
#include <QBrush>
#include <QFont>
#include <QStringList>

#include <span>
#include <cmath>
#include <limits>
#include <string>

using namespace socketspy::core;

namespace socketspy::gui {

namespace {

// Strip the SocketCAN EFF/RTR/ERR flag bits so the value matches the
// DBC-stored message id (which never carries those flags).
inline uint32_t maskId(uint32_t raw) {
    return raw & 0x1FFFFFFFu;
}

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

QString fmtValue(double v, const QString& unit) {
    if (std::isnan(v)) return QStringLiteral("--");
    QString s = QString::number(v, 'g', 6);
    if (!unit.isEmpty()) s += QLatin1Char(' ') + unit;
    return s;
}

} // namespace

VerifySignalPanel::VerifySignalPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void VerifySignalPanel::setupUi() {
    // ---- Header / DBC status ----
    m_dbcLabel = new QLabel(tr("No DBC loaded"), this);

    // ---- Verify request form ----
    auto* form = new QGroupBox(tr("Verification request"), this);
    auto* formLayout = new QFormLayout(form);

    m_signalCombo = new QComboBox(form);
    m_signalCombo->setEditable(true);
    m_signalCombo->setMinimumWidth(180);

    m_targetSpin = new QDoubleSpinBox(form);
    m_targetSpin->setRange(-1e9, 1e9);
    m_targetSpin->setDecimals(3);

    m_toleranceSpin = new QDoubleSpinBox(form);
    m_toleranceSpin->setRange(0.0, 1e9);
    m_toleranceSpin->setDecimals(3);
    m_toleranceSpin->setValue(1.0);

    m_conditionCombo = new QComboBox(form);
    m_conditionCombo->setEditable(true);
    m_conditionCombo->setMinimumWidth(180);
    m_conditionCombo->addItem(tr("(none)"));

    m_conditionSpin = new QDoubleSpinBox(form);
    m_conditionSpin->setRange(-1e9, 1e9);
    m_conditionSpin->setDecimals(3);

    m_timeoutSpin = new QSpinBox(form);
    m_timeoutSpin->setRange(100, 600000);
    m_timeoutSpin->setSingleStep(500);
    m_timeoutSpin->setValue(5000);
    m_timeoutSpin->setSuffix(tr(" ms"));

    form->setObjectName(QStringLiteral("verifyForm"));
    formLayout->addRow(tr("Target signal:"),    m_signalCombo);
    formLayout->addRow(tr("Target value:"),      m_targetSpin);
    formLayout->addRow(tr("Tolerance \xc2\xb1:"), m_toleranceSpin);
    formLayout->addRow(tr("Condition signal:"),  m_conditionCombo);
    formLayout->addRow(tr("Condition value:"),   m_conditionSpin);
    formLayout->addRow(tr("Timeout:"),           m_timeoutSpin);

    m_runBtn = new QPushButton(tr("Run verification"), form);
    m_runBtn->setDefault(true);
    formLayout->addRow(QString(), m_runBtn);

    // ---- Result card (green PASS / red FAIL) ----
    m_resultCard = new QFrame(this);
    m_resultCard->setFrameShape(QFrame::StyledPanel);
    m_resultCard->setMinimumHeight(72);
    m_resultTitle = new QLabel(tr("Idle"), m_resultCard);
    {
        QFont f = m_resultTitle->font();
        f.setPointSizeF(f.pointSizeF() * 1.6);
        f.setBold(true);
        m_resultTitle->setFont(f);
    }
    m_resultSubtitle = new QLabel(tr("No verification has run yet."), m_resultCard);
    m_resultSubtitle->setWordWrap(true);
    auto* cardLayout = new QVBoxLayout(m_resultCard);
    cardLayout->setContentsMargins(14, 10, 14, 10);
    cardLayout->addWidget(m_resultTitle);
    cardLayout->addWidget(m_resultSubtitle);
    showResultCard(false, tr("Idle"), tr("No verification has run yet."));
    // Override the failure colours for the neutral idle state.
    m_resultCard->setStyleSheet(QStringLiteral(
        "QFrame { background:#2c2c34; border-radius:6px; }"));

    // ---- Live readout ----
    m_liveValueLabel       = new QLabel(tr("Live: --"), this);
    m_conditionStatusLabel = new QLabel(tr("Condition: n/a"), this);
    {
        QFont f = m_liveValueLabel->font();
        f.setPointSizeF(f.pointSizeF() * 1.3);
        m_liveValueLabel->setFont(f);
    }
    auto* liveRow = new QHBoxLayout;
    liveRow->addWidget(m_liveValueLabel);
    liveRow->addSpacing(20);
    liveRow->addWidget(m_conditionStatusLabel);
    liveRow->addStretch();

    // ---- History table ----
    m_clearBtn = new QPushButton(tr("Clear history"), this);
    auto* histToolbar = new QHBoxLayout;
    histToolbar->addWidget(new QLabel(tr("History"), this));
    histToolbar->addStretch();
    histToolbar->addWidget(m_clearBtn);

    m_history = new QTableWidget(0, 7, this);
    m_history->setHorizontalHeaderLabels({
        tr("Time"), tr("Signal"), tr("Target"),
        tr("Observed"), tr("Condition"), tr("Result"), tr("Detail")
    });
    m_history->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_history->horizontalHeader()->setStretchLastSection(true);
    m_history->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_history->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_history->verticalHeader()->hide();
    m_history->verticalHeader()->setDefaultSectionSize(24);
    m_history->setAlternatingRowColors(true);

    // ---- Assemble ----
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    layout->addWidget(m_dbcLabel);
    layout->addWidget(form);
    layout->addWidget(m_resultCard);
    layout->addLayout(liveRow);
    layout->addLayout(histToolbar);
    layout->addWidget(m_history, 1);

    // ---- Timeout timer ----
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);

    connect(m_runBtn,       &QPushButton::clicked, this, &VerifySignalPanel::onRunClicked);
    connect(m_clearBtn,     &QPushButton::clicked, this, &VerifySignalPanel::onClearHistory);
    connect(m_timeoutTimer, &QTimer::timeout,      this, &VerifySignalPanel::onTimeout);
}

void VerifySignalPanel::onDbcLoaded(socketspy::dbc::DbcDatabase db) {
    m_dbc = std::move(db);
    m_signalIndex.clear();

    // Build a name -> location index. The Message member is literally named
    // `signals`, which clashes with the active Qt `signals` keyword macro;
    // disable it around the member-access loop, mirroring main_window.h.
    for (const auto& msg : m_dbc.messages) {
        const uint32_t mid  = maskId(msg.id);
        const QString  mname = QString::fromStdString(msg.name);
#pragma push_macro("signals")
#undef signals
        const auto& msgSignals = msg.signals;
#pragma pop_macro("signals")
        for (const auto& sig : msgSignals) {
            VerifySignalRef ref;
            ref.msg_id  = mid;
            ref.message = mname;
            ref.unit    = QString::fromStdString(sig.unit);
            ref.valid   = true;
            // First definition wins on duplicate names across messages.
            const QString sname = QString::fromStdString(sig.name);
            if (!m_signalIndex.contains(sname))
                m_signalIndex.insert(sname, ref);
        }
    }

    m_dbcLoaded = !m_signalIndex.isEmpty();
    m_dbcLabel->setText(m_dbcLoaded
        ? tr("DBC loaded: %1 signals available").arg(m_signalIndex.size())
        : tr("No DBC loaded"));

    rebuildSignalList();
}

void VerifySignalPanel::rebuildSignalList() {
    const QString prevTarget    = m_signalCombo->currentText();
    const QString prevCondition = m_conditionCombo->currentText();

    QStringList names = m_signalIndex.keys();
    names.sort(Qt::CaseInsensitive);

    m_signalCombo->clear();
    m_signalCombo->addItems(names);

    m_conditionCombo->clear();
    m_conditionCombo->addItem(tr("(none)"));
    m_conditionCombo->addItems(names);

    // Best-effort restore of any prior selection.
    if (!prevTarget.isEmpty()) {
        int i = m_signalCombo->findText(prevTarget);
        if (i >= 0) m_signalCombo->setCurrentIndex(i);
        else        m_signalCombo->setEditText(prevTarget);
    }
    if (!prevCondition.isEmpty()) {
        int i = m_conditionCombo->findText(prevCondition);
        if (i >= 0) m_conditionCombo->setCurrentIndex(i);
    }
}

VerifySignalRef VerifySignalPanel::resolveSignal(const QString& signalName) const {
    auto it = m_signalIndex.constFind(signalName);
    if (it != m_signalIndex.constEnd()) return it.value();
    return VerifySignalRef{};   // .valid == false
}

void VerifySignalPanel::onRunClicked() {
    const QString signalName = m_signalCombo->currentText().trimmed();
    QString conditionSignal  = m_conditionCombo->currentText().trimmed();
    // The "(none)" sentinel means: sample immediately, no operating-point gate.
    if (conditionSignal == tr("(none)")) conditionSignal.clear();

    startVerification(signalName,
                      m_targetSpin->value(),
                      m_toleranceSpin->value(),
                      conditionSignal,
                      m_conditionSpin->value(),
                      m_timeoutSpin->value());
}

void VerifySignalPanel::startVerification(const QString& signalName,
                                          double targetValue,
                                          double tolerance,
                                          const QString& conditionSignal,
                                          double conditionValue,
                                          int timeoutMs) {
    // A new request supersedes any in-flight one; report the old as aborted.
    if (m_active) {
        m_timeoutTimer->stop();
        m_active = false;
        emit verificationFinished(false, m_haveTargetValue ? m_lastTargetValue : kNaN,
                                  tr("Superseded by a new verification request."));
    }

    if (!m_dbcLoaded) {
        finish(false, kNaN, tr("No DBC loaded — cannot resolve '%1'.").arg(signalName));
        return;
    }
    if (signalName.isEmpty()) {
        finish(false, kNaN, tr("No target signal specified."));
        return;
    }

    m_targetRef = resolveSignal(signalName);
    if (!m_targetRef.valid) {
        finish(false, kNaN, tr("Signal '%1' is not present in the loaded DBC.")
                                .arg(signalName));
        return;
    }

    m_hasCondition = !conditionSignal.isEmpty();
    if (m_hasCondition) {
        m_conditionRef = resolveSignal(conditionSignal);
        if (!m_conditionRef.valid) {
            finish(false, kNaN, tr("Condition signal '%1' is not present in the DBC.")
                                    .arg(conditionSignal));
            return;
        }
    } else {
        m_conditionRef = VerifySignalRef{};
    }

    // Arm the run.
    m_signalName       = signalName;
    m_target           = targetValue;
    m_tolerance        = (tolerance < 0.0) ? 0.0 : tolerance;
    m_conditionSignal  = conditionSignal;
    m_conditionValue   = conditionValue;
    m_conditionMet     = !m_hasCondition;   // no gate -> already "met"
    m_lastTargetValue  = 0.0;
    m_haveTargetValue  = false;
    m_active           = true;

    const int to = (timeoutMs < 100) ? 100 : timeoutMs;
    m_timeoutTimer->start(to);

    showResultCard(false, tr("RUNNING"),
        m_hasCondition
            ? tr("Waiting for %1 to reach %2 \xc2\xb1 %3, then checking %4 == %5 \xc2\xb1 %3 \xe2\x80\xa6")
                  .arg(conditionSignal)
                  .arg(conditionValue, 0, 'g', 6)
                  .arg(m_tolerance, 0, 'g', 6)
                  .arg(signalName)
                  .arg(targetValue, 0, 'g', 6)
            : tr("Checking %1 == %2 \xc2\xb1 %3 \xe2\x80\xa6")
                  .arg(signalName)
                  .arg(targetValue, 0, 'g', 6)
                  .arg(m_tolerance, 0, 'g', 6));
    m_resultCard->setStyleSheet(QStringLiteral(
        "QFrame { background:#3a3a16; border-radius:6px; }"));   // amber = busy

    setLiveValue(kNaN, m_conditionMet);
}

void VerifySignalPanel::cancelVerification() {
    if (!m_active) return;
    m_timeoutTimer->stop();
    finish(false, m_haveTargetValue ? m_lastTargetValue : kNaN,
           tr("Verification cancelled by user."));
}

void VerifySignalPanel::onFrameReceived(const socketspy::core::CanFrame& frame) {
    if (!m_active || !m_dbcLoaded) return;
    if (frame.flags & static_cast<uint8_t>(FrameFlags::Error)) return;

    const uint32_t id = maskId(frame.id);
    std::span<const uint8_t> data(frame.data, frame.dlc);

    // --- Operating-point gate: wait until the condition signal is in tolerance. ---
    if (m_hasCondition && !m_conditionMet && id == m_conditionRef.msg_id) {
        auto cv = socketspy::gui::dbc_helper::decode_signal(
            m_dbc, m_conditionRef.msg_id, m_conditionSignal.toStdString(), data);
        if (cv.has_value()) {
            const bool met = std::fabs(*cv - m_conditionValue) <= m_tolerance;
            if (met) {
                m_conditionMet = true;
                m_conditionStatusLabel->setText(
                    tr("Condition: %1 reached (%2)")
                        .arg(m_conditionSignal)
                        .arg(fmtValue(*cv, m_conditionRef.unit)));
            } else {
                m_conditionStatusLabel->setText(
                    tr("Condition: %1 = %2 (target %3)")
                        .arg(m_conditionSignal)
                        .arg(fmtValue(*cv, m_conditionRef.unit))
                        .arg(fmtValue(m_conditionValue, m_conditionRef.unit)));
            }
        }
    }

    // --- Target sampling: only counts once the operating point is reached. ---
    if (id == m_targetRef.msg_id) {
        auto tv = socketspy::gui::dbc_helper::decode_signal(
            m_dbc, m_targetRef.msg_id, m_signalName.toStdString(), data);
        if (tv.has_value()) {
            m_lastTargetValue = *tv;
            m_haveTargetValue = true;
            setLiveValue(*tv, m_conditionMet);

            if (m_conditionMet) {
                const double err = std::fabs(*tv - m_target);
                if (err <= m_tolerance) {
                    m_timeoutTimer->stop();
                    finish(true, *tv,
                        tr("%1 = %2 within \xc2\xb1%3 of target %4 (error %5).")
                            .arg(m_signalName)
                            .arg(fmtValue(*tv, m_targetRef.unit))
                            .arg(m_tolerance, 0, 'g', 6)
                            .arg(fmtValue(m_target, m_targetRef.unit))
                            .arg(err, 0, 'g', 6));
                }
            }
        }
    }
}

void VerifySignalPanel::onTimeout() {
    if (!m_active) return;

    QString detail;
    if (m_hasCondition && !m_conditionMet) {
        detail = tr("Timed out waiting for operating point '%1' == %2 \xc2\xb1 %3.")
                     .arg(m_conditionSignal)
                     .arg(m_conditionValue, 0, 'g', 6)
                     .arg(m_tolerance, 0, 'g', 6);
    } else if (!m_haveTargetValue) {
        detail = tr("Timed out: signal '%1' was never seen on the bus.")
                     .arg(m_signalName);
    } else {
        const double err = std::fabs(m_lastTargetValue - m_target);
        detail = tr("Timed out: %1 last = %2, target %3 \xc2\xb1 %4 (error %5).")
                     .arg(m_signalName)
                     .arg(fmtValue(m_lastTargetValue, m_targetRef.unit))
                     .arg(fmtValue(m_target, m_targetRef.unit))
                     .arg(m_tolerance, 0, 'g', 6)
                     .arg(err, 0, 'g', 6);
    }
    finish(false, m_haveTargetValue ? m_lastTargetValue : kNaN, detail);
}

void VerifySignalPanel::finish(bool verified, double finalValue, const QString& detail) {
    m_active = false;
    m_timeoutTimer->stop();

    showResultCard(verified,
                   verified ? tr("PASS") : tr("FAIL"),
                   detail);

    VerificationRecord rec;
    rec.signalName      = m_signalName;
    rec.target          = m_target;
    rec.tolerance       = m_tolerance;
    rec.conditionSignal = m_conditionSignal;
    rec.conditionValue  = m_conditionValue;
    rec.finalValue      = finalValue;
    rec.verified        = verified;
    rec.detail          = detail;
    rec.when            = QDateTime::currentMSecsSinceEpoch();
    appendHistory(rec);

    emit verificationFinished(verified, finalValue, detail);
}

void VerifySignalPanel::setLiveValue(double value, bool conditionMet) {
    m_liveValueLabel->setText(tr("Live %1: %2")
        .arg(m_signalName.isEmpty() ? tr("value") : m_signalName)
        .arg(fmtValue(value, m_targetRef.unit)));

    if (!m_hasCondition) {
        m_conditionStatusLabel->setText(tr("Condition: n/a"));
    } else if (conditionMet) {
        m_conditionStatusLabel->setText(tr("Condition: reached \xe2\x9c\x93"));
    } // else left as set by the gate decode above
}

void VerifySignalPanel::showResultCard(bool verified, const QString& title,
                                       const QString& subtitle) {
    m_resultTitle->setText(title);
    m_resultSubtitle->setText(subtitle);

    // Only PASS (green) and FAIL (red) recolour the card; RUNNING/Idle styles
    // are applied by their callers so we don't clobber the amber/neutral look.
    if (title == tr("PASS")) {
        m_resultCard->setStyleSheet(QStringLiteral(
            "QFrame { background:#1f5130; border-radius:6px; }"));
        m_resultTitle->setStyleSheet(QStringLiteral("color:#7CFF9E;"));
    } else if (title == tr("FAIL")) {
        m_resultCard->setStyleSheet(QStringLiteral(
            "QFrame { background:#5a1f1f; border-radius:6px; }"));
        m_resultTitle->setStyleSheet(QStringLiteral("color:#FF8585;"));
    } else {
        m_resultTitle->setStyleSheet(QString());
    }
    (void)verified;
}

void VerifySignalPanel::appendHistory(const VerificationRecord& rec) {
    const int row = m_history->rowCount();
    m_history->insertRow(row);

    const QString cond = rec.conditionSignal.isEmpty()
        ? tr("—")
        : QString("%1=%2").arg(rec.conditionSignal)
                          .arg(rec.conditionValue, 0, 'g', 6);

    auto* timeItem = new QTableWidgetItem(
        QDateTime::fromMSecsSinceEpoch(rec.when).toString("hh:mm:ss"));
    auto* sigItem  = new QTableWidgetItem(rec.signalName);
    auto* tgtItem  = new QTableWidgetItem(
        QString("%1 \xc2\xb1 %2").arg(rec.target, 0, 'g', 6)
                                 .arg(rec.tolerance, 0, 'g', 6));
    auto* obsItem  = new QTableWidgetItem(
        std::isnan(rec.finalValue) ? tr("--")
                                   : QString::number(rec.finalValue, 'g', 6));
    auto* condItem = new QTableWidgetItem(cond);
    auto* resItem  = new QTableWidgetItem(rec.verified ? tr("PASS") : tr("FAIL"));
    auto* detItem  = new QTableWidgetItem(rec.detail);

    const QColor bg = rec.verified ? QColor(31, 81, 48) : QColor(90, 31, 31);
    for (auto* it : {timeItem, sigItem, tgtItem, obsItem, condItem, resItem, detItem})
        it->setBackground(QBrush(bg));
    resItem->setForeground(QBrush(rec.verified ? QColor(124, 255, 158)
                                               : QColor(255, 133, 133)));

    m_history->setItem(row, 0, timeItem);
    m_history->setItem(row, 1, sigItem);
    m_history->setItem(row, 2, tgtItem);
    m_history->setItem(row, 3, obsItem);
    m_history->setItem(row, 4, condItem);
    m_history->setItem(row, 5, resItem);
    m_history->setItem(row, 6, detItem);

    m_history->scrollToBottom();
}

void VerifySignalPanel::onClearHistory() {
    m_history->setRowCount(0);
}

} // namespace socketspy::gui
