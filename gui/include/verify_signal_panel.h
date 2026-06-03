#pragma once
#include <QWidget>
#include <QString>
#include <QHash>
#include <QVector>
#include <cstdint>
#include "cancore.h"

// dbc_types.h defines a member called `signals`, which collides with the Qt
// `signals` keyword macro. Wrap the include with the same push/undef/pop guard
// used in main_window.h / signal_validator_panel.h so both can coexist in this
// translation unit.
#pragma push_macro("signals")
#undef signals
#include "dbc_types.h"
#pragma pop_macro("signals")

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QLabel;
class QPushButton;
class QLineEdit;
class QTableWidget;
class QTimer;
class QFrame;

namespace socketspy::gui {

// Resolved coordinates of one DBC signal: which message carries it (masked id)
// plus the engineering unit, cached so live decoding never re-scans the DBC.
struct VerifySignalRef {
    uint32_t msg_id{0};      // masked CAN id (EFF/RTR flags stripped)
    QString  message;        // owning message name
    QString  unit;           // engineering unit (e.g. "rpm", "km/h")
    bool     valid{false};
};

// Outcome of one completed verification, kept for the history table.
struct VerificationRecord {
    QString  signalName;
    double   target{0.0};
    double   tolerance{0.0};
    QString  conditionSignal;   // empty when no operating-point gate
    double   conditionValue{0.0};
    double   finalValue{0.0};
    bool     verified{false};
    QString  detail;
    qint64   when{0};           // epoch ms, when the run finished
};

// VerifySignalPanel is the SocketSpy side of the ECU-Studio -> SocketSpy
// live-verify hook. After ECU Studio flashes a map it asks SocketSpy to confirm
// on the live bus that a signal reaches an expected value at an operating point.
// The same check is exposed manually through this panel's UI and programmatically
// through startVerification(); both share one verify engine.
class VerifySignalPanel : public QWidget {
    Q_OBJECT

public:
    explicit VerifySignalPanel(QWidget* parent = nullptr);

public slots:
    // Cache the DBC so signal names can be resolved and frames decoded.
    void onDbcLoaded(socketspy::dbc::DbcDatabase db);

    // Decode incoming frames while a verification is active.
    void onFrameReceived(const socketspy::core::CanFrame& frame);

    // Programmatic entry point (the ECU-Studio bridge / verify_signal API tool
    // call this). Starts a verification of `signalName` against `targetValue`
    // within `tolerance`. If `conditionSignal` is non-empty the run first waits
    // until that signal is within `tolerance` of `conditionValue` (the operating
    // point) before sampling the target. Passes if the target is within
    // tolerance before `timeoutMs` elapses.
    void startVerification(const QString& signalName,
                           double targetValue,
                           double tolerance,
                           const QString& conditionSignal,
                           double conditionValue,
                           int timeoutMs);

    // Abort the active verification (no-op if idle). Emits a FAIL result.
    void cancelVerification();

signals:
    // Emitted exactly once per verification when it concludes (pass, fail or
    // timeout). `finalValue` is the last observed target value (NaN if never
    // decoded). `detail` is a human-readable explanation.
    void verificationFinished(bool verified, double finalValue, const QString& detail);

private slots:
    void onRunClicked();
    void onTimeout();
    void onClearHistory();

private:
    void setupUi();
    void rebuildSignalList();
    VerifySignalRef resolveSignal(const QString& signalName) const;
    void finish(bool verified, double finalValue, const QString& detail);
    void setLiveValue(double value, bool conditionMet);
    void showResultCard(bool verified, const QString& title, const QString& subtitle);
    void appendHistory(const VerificationRecord& rec);

    // --- DBC state ---
    socketspy::dbc::DbcDatabase m_dbc;
    bool m_dbcLoaded{false};
    // signal name -> resolved location, built once per DBC load.
    QHash<QString, VerifySignalRef> m_signalIndex;

    // --- Active verification state ---
    bool    m_active{false};
    QString m_signalName;
    double  m_target{0.0};
    double  m_tolerance{0.0};
    bool    m_hasCondition{false};
    QString m_conditionSignal;
    double  m_conditionValue{0.0};
    bool    m_conditionMet{false};     // operating point reached this run
    VerifySignalRef m_targetRef;
    VerifySignalRef m_conditionRef;
    double  m_lastTargetValue{0.0};
    bool    m_haveTargetValue{false};
    QTimer* m_timeoutTimer{nullptr};

    // --- Widgets ---
    QComboBox*      m_signalCombo{nullptr};
    QDoubleSpinBox* m_targetSpin{nullptr};
    QDoubleSpinBox* m_toleranceSpin{nullptr};
    QComboBox*      m_conditionCombo{nullptr};
    QDoubleSpinBox* m_conditionSpin{nullptr};
    QSpinBox*       m_timeoutSpin{nullptr};
    QPushButton*    m_runBtn{nullptr};
    QPushButton*    m_clearBtn{nullptr};
    QLabel*         m_dbcLabel{nullptr};

    QFrame*         m_resultCard{nullptr};
    QLabel*         m_resultTitle{nullptr};
    QLabel*         m_resultSubtitle{nullptr};
    QLabel*         m_liveValueLabel{nullptr};
    QLabel*         m_conditionStatusLabel{nullptr};

    QTableWidget*   m_history{nullptr};
};

} // namespace socketspy::gui
