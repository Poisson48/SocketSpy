#include "signal_detective_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QDateTime>
#include <cmath>
#include <algorithm>

namespace socketspy::gui {

// ─── FrameStats ───────────────────────────────────────────────────────────────

double FrameStats::hz() const {
    if (frameCount < 2 || lastMs <= firstMs) return 0.0;
    return (frameCount - 1) * 1000.0 / (lastMs - firstMs);
}

SignalKind FrameStats::classify() const {
    if (dlc <= 0) return SignalKind::Unknown;

    int binaryCount  = 0;
    int analogCount  = 0;
    int counterCount = 0;
    int activeBytes  = 0;

    for (int b = 0; b < dlc; ++b) {
        if (b >= distinctPerByte.size()) break;
        const auto& dv = distinctPerByte[b];
        int range = static_cast<int>(byteMax[b]) - static_cast<int>(byteMin[b]);

        if (dv.size() <= 1) continue; // constant byte, skip
        ++activeBytes;

        // Binary / TOR : seulement 2 valeurs distinctes dans {0x00,0x01,0xFF,0xFE}
        if (dv.size() <= 2) {
            bool allKnown = true;
            for (uint8_t v : dv) {
                if (v != 0x00 && v != 0x01 && v != 0xFF && v != 0xFE) {
                    allKnown = false;
                    break;
                }
            }
            if (allKnown) { ++binaryCount; continue; }
        }

        // Counter : majorité des deltas vaut +1
        if (b < deltaHistory.size()) {
            const auto& dh = deltaHistory[b];
            if (dh.size() >= 8) {
                int plus1 = static_cast<int>(std::count(dh.begin(), dh.end(), int8_t(1)));
                if (plus1 >= dh.size() * 8 / 10) { ++counterCount; continue; }
            }
        }

        // Analog : plage large
        if (range >= 8) { ++analogCount; continue; }
    }

    if (activeBytes == 0) return SignalKind::Constant;
    if (analogCount > 0 && analogCount >= binaryCount) return SignalKind::Analog;
    if (binaryCount > 0) return SignalKind::Binary;
    if (counterCount > 0) return SignalKind::Counter;
    return SignalKind::Unknown;
}

QString FrameStats::kindLabel() const {
    switch (classify()) {
        case SignalKind::Constant: return "CONST";
        case SignalKind::Binary:   return "TOR";
        case SignalKind::Analog:   return "ANA";
        case SignalKind::Counter:  return "CNT";
        default:                   return "???";
    }
}

QString FrameStats::rateLabel() const {
    double h = hz();
    if (h < 2.0)  return "Lent";
    if (h < 20.0) return "Moyen";
    return "Rapide";
}

// ─── SignalDetectivePanel ─────────────────────────────────────────────────────

SignalDetectivePanel::SignalDetectivePanel(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void SignalDetectivePanel::setupUi() {
    auto* tabs = new QTabWidget(this);
    tabs->addTab(buildClassifyTab(), tr("Analyse auto"));
    tabs->addTab(buildWiggleTab(),   tr("Test variation"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(tabs);

    m_autoRefresh = new QTimer(this);
    m_autoRefresh->setInterval(1000);
    connect(m_autoRefresh, &QTimer::timeout, this, &SignalDetectivePanel::onAutoRefreshTick);
    m_autoRefresh->start();

    m_baselineTimer = new QTimer(this);
    m_baselineTimer->setInterval(50);
    connect(m_baselineTimer, &QTimer::timeout, this, &SignalDetectivePanel::onBaselineTick);

    m_afterTimer = new QTimer(this);
    m_afterTimer->setInterval(50);
    connect(m_afterTimer, &QTimer::timeout, this, &SignalDetectivePanel::onAfterTick);
}

QWidget* SignalDetectivePanel::buildClassifyTab() {
    auto* w = new QWidget(this);

    m_classTable = new QTableWidget(0, 6, w);
    m_classTable->setHorizontalHeaderLabels(
        {tr("ID"), tr("Hz"), tr("Rythme"), tr("Type"), tr("DLC"), tr("Trames")});
    m_classTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_classTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_classTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_classTable->verticalHeader()->hide();
    m_classTable->verticalHeader()->setDefaultSectionSize(22);
    m_classTable->setAlternatingRowColors(true);
    m_classTable->setSortingEnabled(true);

    m_refreshBtn  = new QPushButton(tr("Rafraichir"), w);
    m_classStatus = new QLabel(tr("En attente de trames..."), w);

    auto* topRow = new QHBoxLayout;
    topRow->addWidget(m_classStatus, 1);
    topRow->addWidget(m_refreshBtn);

    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addLayout(topRow);
    layout->addWidget(m_classTable, 1);

    connect(m_refreshBtn, &QPushButton::clicked, this, &SignalDetectivePanel::onRefresh);
    return w;
}

QWidget* SignalDetectivePanel::buildWiggleTab() {
    auto* w = new QWidget(this);

    m_wiggleInstr = new QLabel(
        tr("<b>Test de variation (wiggle test)</b><br>"
           "1. Cliquez Demarrer pour capturer la baseline (3s)<br>"
           "2. Effectuez l'action physique (pedaler, bouton...)<br>"
           "3. SocketSpy trouve les signaux qui ont bouge"), w);
    m_wiggleInstr->setWordWrap(true);
    m_wiggleInstr->setAlignment(Qt::AlignCenter);

    m_wiggleStartBtn = new QPushButton(tr("Demarrer le test"), w);
    m_wiggleStartBtn->setMinimumHeight(36);

    m_actionDoneBtn = new QPushButton(tr("J'ai effectue l'action"), w);
    m_actionDoneBtn->setMinimumHeight(44);
    m_actionDoneBtn->setEnabled(false);
    m_actionDoneBtn->setVisible(false);
    m_actionDoneBtn->setStyleSheet(
        "background-color: #2a7a2a; color: white; font-weight: bold;");

    m_wiggleProgress = new QProgressBar(w);
    m_wiggleProgress->setRange(0, kWiggleDurationMs);
    m_wiggleProgress->setValue(0);
    m_wiggleProgress->setTextVisible(false);
    m_wiggleProgress->setVisible(false);

    m_wiggleStatus = new QLabel(tr("En attente..."), w);
    m_wiggleStatus->setAlignment(Qt::AlignCenter);

    m_wiggleTable = new QTableWidget(0, 5, w);
    m_wiggleTable->setHorizontalHeaderLabels(
        {tr("ID"), tr("Octet"), tr("Avant"), tr("Apres"), tr("Score")});
    m_wiggleTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_wiggleTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_wiggleTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_wiggleTable->verticalHeader()->hide();
    m_wiggleTable->verticalHeader()->setDefaultSectionSize(22);
    m_wiggleTable->setAlternatingRowColors(true);

    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);
    layout->addWidget(m_wiggleInstr);
    layout->addWidget(m_wiggleStartBtn);
    layout->addWidget(m_actionDoneBtn);
    layout->addWidget(m_wiggleProgress);
    layout->addWidget(m_wiggleStatus);
    layout->addWidget(m_wiggleTable, 1);

    connect(m_wiggleStartBtn, &QPushButton::clicked,
            this, &SignalDetectivePanel::onStartWiggle);
    connect(m_actionDoneBtn, &QPushButton::clicked,
            this, &SignalDetectivePanel::onActionDone);
    return w;
}

// ─── Réception des trames ─────────────────────────────────────────────────────

void SignalDetectivePanel::onFrameReceived(const socketspy::core::CanFrame& frame) {
    const int dlc = static_cast<int>(frame.dlc);
    if (dlc <= 0) return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto& st = m_stats[frame.id];

    if (st.frameCount == 0) {
        st.id      = frame.id;
        st.dlc     = dlc;
        st.firstMs = now;
        st.distinctPerByte.resize(dlc);
        st.byteMin.fill(0xFF, dlc);
        st.byteMax.fill(0x00, dlc);
        st.deltaHistory.resize(dlc);
    }
    st.lastMs = now;
    st.frameCount++;

    QByteArray payload(reinterpret_cast<const char*>(frame.data), dlc);

    for (int b = 0; b < dlc && b < st.dlc; ++b) {
        uint8_t v = static_cast<uint8_t>(frame.data[b]);
        if (st.distinctPerByte[b].size() < 64)
            st.distinctPerByte[b].insert(v);
        st.byteMin[b] = qMin(st.byteMin[b], v);
        st.byteMax[b] = qMax(st.byteMax[b], v);

        if (!st.lastPayload.isEmpty() && b < st.lastPayload.size()) {
            int8_t delta = static_cast<int8_t>(
                v - static_cast<uint8_t>(st.lastPayload[b]));
            auto& dh = st.deltaHistory[b];
            dh.append(delta);
            if (dh.size() > kMaxDeltaHistory) dh.removeFirst();
        }
    }
    st.lastPayload = payload;

    if (m_collectingBefore) {
        auto& buf = m_beforeSnap[frame.id];
        buf.append(payload);
        if (buf.size() > 200) buf.removeFirst();
    }
    if (m_collectingAfter) {
        auto& buf = m_afterSnap[frame.id];
        buf.append(payload);
        if (buf.size() > 200) buf.removeFirst();
    }
}

// ─── Tab 1 : Auto-classification ──────────────────────────────────────────────

void SignalDetectivePanel::onAutoRefreshTick() {
    if (!m_stats.isEmpty())
        refreshClassifyTable();
}

void SignalDetectivePanel::onRefresh() {
    refreshClassifyTable();
}

void SignalDetectivePanel::refreshClassifyTable() {
    m_classTable->setSortingEnabled(false);
    m_classTable->setRowCount(0);
    m_classTable->setRowCount(m_stats.size());

    static const QColor kColorLent  (0x2e, 0x7d, 0x32, 60);
    static const QColor kColorMoyen (0xf9, 0xa8, 0x25, 60);
    static const QColor kColorRapide(0xc6, 0x28, 0x28, 60);
    static const QColor kColorTOR   (0x15, 0x65, 0xc0, 60);
    static const QColor kColorANA   (0x4a, 0x14, 0x8c, 60);
    static const QColor kColorCONST (0x78, 0x90, 0x9c, 60);
    static const QColor kColorCNT   (0xe6, 0x5c, 0x00, 60);

    auto mkItem = [](const QString& t, Qt::Alignment a = Qt::AlignCenter) {
        auto* it = new QTableWidgetItem(t);
        it->setTextAlignment(a);
        return it;
    };

    int row = 0;
    for (const auto& st : std::as_const(m_stats)) {
        QString idStr  = QString("0x%1").arg(st.id, 3, 16, QChar('0')).toUpper();
        double  h      = st.hz();
        QString hzStr  = h > 0 ? QString::number(h, 'f', 1) : "-";
        QString rate   = st.frameCount >= 5 ? st.rateLabel() : "-";
        QString kind   = st.frameCount >= 5 ? st.kindLabel() : "-";

        auto* rateItem = mkItem(rate);
        auto* kindItem = mkItem(kind);

        QColor rateBg = (rate == "Lent")   ? kColorLent
                      : (rate == "Rapide") ? kColorRapide
                      : kColorMoyen;
        if (rate != "-") rateItem->setBackground(rateBg);

        QColor kindBg = (kind == "TOR")   ? kColorTOR
                      : (kind == "ANA")   ? kColorANA
                      : (kind == "CONST") ? kColorCONST
                      : (kind == "CNT")   ? kColorCNT
                      : QColor(Qt::transparent);
        if (kind != "-" && kind != "???") kindItem->setBackground(kindBg);

        m_classTable->setItem(row, 0, mkItem(idStr));
        m_classTable->setItem(row, 1, mkItem(hzStr));
        m_classTable->setItem(row, 2, rateItem);
        m_classTable->setItem(row, 3, kindItem);
        m_classTable->setItem(row, 4, mkItem(QString::number(st.dlc)));
        m_classTable->setItem(row, 5, mkItem(QString::number(st.frameCount)));
        ++row;
    }

    m_classTable->setSortingEnabled(true);
    m_classStatus->setText(tr("%1 ID(s) observe(s)").arg(m_stats.size()));
}

// ─── Tab 2 : Wiggle test ──────────────────────────────────────────────────────

void SignalDetectivePanel::onStartWiggle() {
    m_beforeSnap.clear();
    m_afterSnap.clear();
    m_wiggleState      = WiggleState::Baseline;
    m_collectingBefore = true;
    m_collectingAfter  = false;

    m_wiggleStartBtn->setEnabled(false);
    m_actionDoneBtn->setVisible(false);
    m_wiggleTable->setRowCount(0);
    m_wiggleProgress->setVisible(true);
    m_wiggleProgress->setValue(0);
    m_wiggleStatus->setText(tr("Capture baseline... (3 s)"));
    m_countdownMs = 0;
    m_baselineTimer->start();
}

void SignalDetectivePanel::onBaselineTick() {
    m_countdownMs += 50;
    m_wiggleProgress->setValue(m_countdownMs);
    if (m_countdownMs >= kWiggleDurationMs) {
        m_baselineTimer->stop();
        m_collectingBefore = false;
        m_wiggleState      = WiggleState::WaitAction;

        m_wiggleProgress->setVisible(false);
        m_wiggleStatus->setText(
            tr("Effectuez maintenant l'action physique sur le vehicule..."));
        m_actionDoneBtn->setVisible(true);
        m_actionDoneBtn->setEnabled(true);
    }
}

void SignalDetectivePanel::onActionDone() {
    m_actionDoneBtn->setEnabled(false);
    m_wiggleState     = WiggleState::After;
    m_collectingAfter = true;

    m_wiggleProgress->setRange(0, kWiggleDurationMs);
    m_wiggleProgress->setValue(0);
    m_wiggleProgress->setVisible(true);
    m_wiggleStatus->setText(tr("Capture apres action... (3 s)"));
    m_countdownMs = 0;
    m_afterTimer->start();
}

void SignalDetectivePanel::onAfterTick() {
    m_countdownMs += 50;
    m_wiggleProgress->setValue(m_countdownMs);
    if (m_countdownMs >= kWiggleDurationMs) {
        m_afterTimer->stop();
        m_collectingAfter = false;
        m_wiggleState     = WiggleState::Results;

        m_wiggleProgress->setVisible(false);
        m_actionDoneBtn->setVisible(false);
        m_wiggleStartBtn->setEnabled(true);

        auto candidates = computeWiggleDiff();
        populateWiggleResults(candidates);
    }
}

QVector<WiggleCandidate> SignalDetectivePanel::computeWiggleDiff() {
    QVector<WiggleCandidate> results;

    QSet<uint32_t> ids;
    for (auto it = m_beforeSnap.keyBegin(); it != m_beforeSnap.keyEnd(); ++it)
        ids.insert(*it);
    for (auto it = m_afterSnap.keyBegin(); it != m_afterSnap.keyEnd(); ++it)
        ids.insert(*it);

    for (uint32_t id : ids) {
        const auto& before = m_beforeSnap.value(id);
        const auto& after  = m_afterSnap.value(id);
        if (before.isEmpty() || after.isEmpty()) continue;

        int dlc = before.first().size();
        for (int b = 0; b < dlc; ++b) {
            double sumBefore = 0, sumAfter = 0;
            QSet<uint8_t> valuesBefore, valuesAfter;

            for (const auto& fr : before) {
                if (b < fr.size()) {
                    uint8_t v = static_cast<uint8_t>(fr[b]);
                    sumBefore += v;
                    valuesBefore.insert(v);
                }
            }
            for (const auto& fr : after) {
                if (b < fr.size()) {
                    uint8_t v = static_cast<uint8_t>(fr[b]);
                    sumAfter += v;
                    valuesAfter.insert(v);
                }
            }

            double meanBefore = sumBefore / before.size();
            double meanAfter  = sumAfter  / after.size();
            double delta      = std::abs(meanAfter - meanBefore);

            SignalKind kind = SignalKind::Unknown;
            if (m_stats.contains(id)) kind = m_stats[id].classify();

            double score = 0.0;
            if (kind == SignalKind::Binary) {
                score = (valuesBefore != valuesAfter) ? 1.0 : 0.0;
            } else {
                double globalRange = 1.0;
                if (m_stats.contains(id) && b < m_stats[id].dlc) {
                    globalRange = qMax(1.0,
                        static_cast<double>(m_stats[id].byteMax[b]
                                          - m_stats[id].byteMin[b]));
                }
                score = qMin(1.0, delta / globalRange);
            }

            if (score > 0.05) {
                WiggleCandidate c;
                c.id         = id;
                c.byteIdx    = b;
                c.kind       = kind;
                c.beforeMean = meanBefore;
                c.afterMean  = meanAfter;
                c.score      = score;
                results.append(c);
            }
        }
    }

    std::sort(results.begin(), results.end(),
        [](const WiggleCandidate& a, const WiggleCandidate& b) {
            return a.score > b.score;
        });

    if (results.size() > 30) results.resize(30);
    return results;
}

void SignalDetectivePanel::populateWiggleResults(
    const QVector<WiggleCandidate>& candidates)
{
    m_wiggleTable->setRowCount(0);
    m_wiggleTable->setRowCount(candidates.size());

    if (candidates.isEmpty()) {
        m_wiggleStatus->setText(tr("Aucun signal ne semble avoir change."));
        return;
    }
    m_wiggleStatus->setText(
        tr("%1 signal(s) candidat(s) trouve(s).").arg(candidates.size()));

    auto mkItem = [](const QString& t, Qt::Alignment a = Qt::AlignCenter) {
        auto* it = new QTableWidgetItem(t);
        it->setTextAlignment(a);
        return it;
    };

    for (int row = 0; row < candidates.size(); ++row) {
        const auto& c = candidates[row];
        m_wiggleTable->setItem(row, 0,
            mkItem(QString("0x%1").arg(c.id, 3, 16, QChar('0')).toUpper()));
        m_wiggleTable->setItem(row, 1,
            mkItem(QString("B%1").arg(c.byteIdx)));
        m_wiggleTable->setItem(row, 2,
            mkItem(QString::number(c.beforeMean, 'f', 1)));
        m_wiggleTable->setItem(row, 3,
            mkItem(QString::number(c.afterMean,  'f', 1)));
        m_wiggleTable->setItem(row, 4,
            mkItem(QString::number(c.score * 100, 'f', 0) + "%"));

        QColor bg;
        if      (c.score >= 0.8) bg = QColor(0x2e, 0x7d, 0x32, 80);
        else if (c.score >= 0.3) bg = QColor(0xf9, 0xa8, 0x25, 80);
        else                     bg = QColor(Qt::transparent);

        for (int col = 0; col < 5; ++col)
            if (auto* it = m_wiggleTable->item(row, col))
                it->setBackground(bg);
    }
}

} // namespace socketspy::gui
