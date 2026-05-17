#pragma once
#include <QWidget>
#include <QHash>
#include <QSet>
#include <QVector>
#include <QTimer>
#include <QTabWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QEvent>
#include <cstdint>
#include "cancore.h"

namespace socketspy::gui {

// ─── Classification heuristique ──────────────────────────────────────────────
enum class SignalKind { Unknown, Constant, Binary, Analog, Counter };

struct FrameStats {
    uint32_t id{0};
    int      dlc{0};
    int      frameCount{0};
    qint64   firstMs{0};
    qint64   lastMs{0};
    QVector<QSet<uint8_t>> distinctPerByte;
    QVector<uint8_t>       byteMin;
    QVector<uint8_t>       byteMax;
    QByteArray             lastPayload;
    QVector<QVector<int8_t>> deltaHistory; // [byte][sample]

    double     hz()        const;
    SignalKind classify()  const;
    QString    kindLabel() const;
    QString    rateLabel() const; // "Lent" / "Moyen" / "Rapide"
};

// ─── Wiggle test ─────────────────────────────────────────────────────────────
struct WiggleCandidate {
    uint32_t   id{0};
    int        byteIdx{0};
    SignalKind kind{SignalKind::Unknown};
    double     beforeMean{0.0};
    double     afterMean{0.0};
    double     score{0.0}; // 0.0–1.0
};

// ─── SignalDetectivePanel ─────────────────────────────────────────────────────
class SignalDetectivePanel : public QWidget {
    Q_OBJECT
public:
    explicit SignalDetectivePanel(QWidget* parent = nullptr);

public slots:
    void onFrameReceived(const socketspy::core::CanFrame& frame);

private slots:
    void onRefresh();
    void onAutoRefreshTick();
    void onStartWiggle();
    void onBaselineTick();
    void onActionDone();
    void onAfterTick();

private:
    void     setupUi();
    QWidget* buildClassifyTab();
    QWidget* buildWiggleTab();

    void refreshClassifyTable();
    void populateWiggleResults(const QVector<WiggleCandidate>& candidates);
    QVector<WiggleCandidate> computeWiggleDiff();

    // Tab 1
    QTableWidget* m_classTable{nullptr};
    QPushButton*  m_refreshBtn{nullptr};
    QLabel*       m_classStatus{nullptr};

    // Tab 2
    enum class WiggleState { Idle, Baseline, WaitAction, After, Results };

    QLabel*       m_wiggleInstr{nullptr};
    QPushButton*  m_wiggleStartBtn{nullptr};
    QPushButton*  m_actionDoneBtn{nullptr};
    QProgressBar* m_wiggleProgress{nullptr};
    QTableWidget* m_wiggleTable{nullptr};
    QLabel*       m_wiggleStatus{nullptr};

    WiggleState m_wiggleState{WiggleState::Idle};
    QTimer*     m_baselineTimer{nullptr};
    QTimer*     m_afterTimer{nullptr};
    int         m_countdownMs{0};

    QHash<uint32_t, QVector<QByteArray>> m_beforeSnap;
    QHash<uint32_t, QVector<QByteArray>> m_afterSnap;
    bool m_collectingBefore{false};
    bool m_collectingAfter{false};

    // Données live
    QHash<uint32_t, FrameStats> m_stats;
    QTimer* m_autoRefresh{nullptr};

    bool eventFilter(QObject* obj, QEvent* event) override;
    void setupCopyable(QTableWidget* table);
    void copySelectedCells(QTableWidget* table);

    static constexpr int kMaxDeltaHistory  = 64;
    static constexpr int kWiggleDurationMs = 3000;
};

} // namespace socketspy::gui
