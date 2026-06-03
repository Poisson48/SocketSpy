#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QHash>
#include <QString>
#include <QVector>
#include <cstdint>
#include "cancore.h"

// dbc_types.h defines a member called `signals`, which collides with the Qt
// `signals` keyword macro. Wrap the include with the same push/undef/pop guard
// used in main_window.h so both can coexist in this translation unit.
#pragma push_macro("signals")
#undef signals
#include "dbc_types.h"
#pragma pop_macro("signals")

namespace socketspy::gui {

// One cached signal definition extracted from the loaded DBC.
struct ExpectedSignal {
    QString  name;
    double   factor{1.0};
    double   offset{0.0};
    double   min_val{0.0};
    double   max_val{0.0};
    uint8_t  start_bit{0};
    uint8_t  bit_length{0};
    bool     little_endian{true};
    QString  unit;
    bool     has_range{false};   // true when max_val > min_val (a checkable window)
};

// One cached message definition extracted from the loaded DBC.
struct ExpectedMessage {
    uint32_t id{0};              // masked ID (EFF/RTR flags stripped)
    QString  name;
    uint8_t  dlc{0};
    QVector<ExpectedSignal> signals_;
};

// Severity of a validation finding, also drives the row colour.
enum class FindingSeverity {
    Info,        // benign / informational
    Warning,     // DLC mismatch, suspicious but not necessarily wrong
    Error,       // value out of range, undocumented ID
};

// Category of a validation finding.
enum class FindingKind {
    OutOfRange,
    UndocumentedId,
    DlcMismatch,
};

// A single, de-duplicated finding shown as one table row.
struct Finding {
    FindingKind     kind{FindingKind::OutOfRange};
    FindingSeverity severity{FindingSeverity::Error};
    uint32_t        id{0};       // masked CAN ID involved
    QString         subject;     // "MsgName::SigName" or message name / raw id
    QString         observed;    // observed value / DLC, formatted
    QString         expected;    // expected range / DLC, formatted
    QString         issue;       // human readable category
    uint64_t        count{0};    // how many frames triggered this finding
    double          lastValue{0.0};
};

class SignalValidatorPanel : public QWidget {
    Q_OBJECT

public:
    explicit SignalValidatorPanel(QWidget* parent = nullptr);

public slots:
    // Cache the expected message/signal definitions from the loaded DBC.
    void onDbcLoaded(socketspy::dbc::DbcDatabase db);
    // Decode and validate one live frame against the cached DBC.
    void onFrameReceived(const socketspy::core::CanFrame& frame);

private slots:
    void refreshTable();
    void onClear();
    void onTogglePause();

private:
    void setupUi();
    void addFinding(FindingKind kind, FindingSeverity sev,
                    uint32_t id, const QString& subject,
                    const QString& observed, const QString& expected,
                    const QString& issue, double value);
    void updateCounters();

    QTableWidget* m_table{nullptr};
    QPushButton*  m_clearBtn{nullptr};
    QPushButton*  m_pauseBtn{nullptr};
    QLabel*       m_dbcLabel{nullptr};
    QLabel*       m_errorCountLabel{nullptr};
    QLabel*       m_warnCountLabel{nullptr};
    QLabel*       m_okCountLabel{nullptr};
    QTimer*       m_timer{nullptr};

    // Cached DBC, keyed by masked CAN ID, for fast expected-definition lookup.
    QHash<uint32_t, ExpectedMessage> m_expected;
    // Raw DBC kept alongside the cache so the shared dbc_helper decode routines
    // (which take a DbcDatabase) can be reused without re-implementing decoding.
    socketspy::dbc::DbcDatabase m_dbc;
    bool m_dbcLoaded{false};

    bool m_paused{false};

    // De-duplicated findings, keyed by a stable category+id+subject string so
    // repeated bad frames bump a counter instead of growing the table forever.
    QHash<QString, Finding> m_findings;
    bool m_dirty{false};

    // Running totals over all observed frames.
    uint64_t m_framesSeen{0};
    uint64_t m_framesOk{0};
};

} // namespace socketspy::gui
