#pragma once
#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>
#include "cancore.h"

#pragma push_macro("signals")
#undef signals
#include "dbc_types.h"
#pragma pop_macro("signals")

namespace socketspy::gui {

class ProtocolPanel : public QWidget {
    Q_OBJECT
public:
    explicit ProtocolPanel(QWidget* parent = nullptr);

public slots:
    void onFrameReceived(socketspy::core::CanFrame frame);
    void onDbcLoaded(const socketspy::dbc::DbcDatabase& db);

private:
    void setupUi();
    void addRow(const QString& proto, const QString& type,
                uint32_t id, const QString& details);

    void decodeCanopen(const socketspy::core::CanFrame& f);
    void decodeIsoTp(const socketspy::core::CanFrame& f);
    void decodeJ1939(const socketspy::core::CanFrame& f);
    void decodeUdsObd(const socketspy::core::CanFrame& f);
    void decodeNmea2000(const socketspy::core::CanFrame& f);
    void decodeAll(const socketspy::core::CanFrame& f);

    QComboBox*    m_protoCombo{nullptr};  // "All", "CANopen", "ISO-TP", "J1939", "UDS/OBD-II", "NMEA 2000"
    QPushButton*  m_clearBtn{nullptr};
    QTableWidget* m_table{nullptr};       // Timestamp, Protocol, Type, ID, Details

    socketspy::dbc::DbcDatabase m_dbc;
};

} // namespace socketspy::gui
