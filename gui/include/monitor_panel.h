#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QCheckBox>
#include "cancore.h"

// Temporarily suppress the Qt `signals` keyword macro so that
// socketspy::dbc::Message::signals (a data member) parses correctly.
#pragma push_macro("signals")
#undef signals
#include "dbc_types.h"
#pragma pop_macro("signals")

namespace socketspy::gui {

class MonitorPanel : public QWidget {
    Q_OBJECT

public:
    explicit MonitorPanel(QWidget* parent = nullptr);
    ~MonitorPanel() override;

    static constexpr int kMaxRows = 500;

public slots:
    void onFrameReceived(socketspy::core::CanFrame frame);
    void onDbcLoaded(socketspy::dbc::DbcDatabase db);

signals:
    void signalDoubleClicked(QString signalName, uint32_t msgId);

private slots:
    void onClear();
    void onCellDoubleClicked(int row, int col);

private:
    void setupUi();
    QString decodeFrame(const socketspy::core::CanFrame& f) const;

    QTableWidget*  m_table{nullptr};
    QPushButton*   m_clear{nullptr};
    QCheckBox*     m_pause{nullptr};

    socketspy::dbc::DbcDatabase* m_dbc{nullptr};
    bool m_dbcLoaded{false};
};

} // namespace socketspy::gui
