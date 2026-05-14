#include "protocol_panel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QString>
#include <QDateTime>

#include "canopen_nmt.h"
#include "canopen_emcy.h"
#include "canopen_sdo.h"
#include "isotp.h"
#include "j1939_frame.h"
#include "uds_client.h"
#include "obd2.h"
#include "nmea2000.h"

namespace socketspy::gui {

static constexpr int kMaxRows = 2000;

// Column indices
enum Col { ColTimestamp = 0, ColProtocol, ColType, ColId, ColDetails, ColCount };

ProtocolPanel::ProtocolPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void ProtocolPanel::setupUi()
{
    auto* topBar = new QHBoxLayout;

    auto* label = new QLabel("Protocol:", this);
    m_protoCombo = new QComboBox(this);
    m_protoCombo->addItems({"All", "CANopen", "ISO-TP", "J1939", "UDS/OBD-II", "NMEA 2000"});
    m_clearBtn = new QPushButton("Clear", this);

    topBar->addWidget(label);
    topBar->addWidget(m_protoCombo);
    topBar->addStretch();
    topBar->addWidget(m_clearBtn);

    m_table = new QTableWidget(0, ColCount, this);
    m_table->setHorizontalHeaderLabels({"Timestamp", "Protocol", "Type", "ID", "Details"});
    m_table->horizontalHeader()->setSectionResizeMode(ColDetails, QHeaderView::Stretch);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(topBar);
    layout->addWidget(m_table);
    setLayout(layout);

    connect(m_clearBtn, &QPushButton::clicked, this, [this]() {
        m_table->setRowCount(0);
    });
}

void ProtocolPanel::addRow(const QString& proto, const QString& type,
                            uint32_t id, const QString& details)
{
    // Trim table to kMaxRows
    if (m_table->rowCount() >= kMaxRows)
        m_table->removeRow(0);

    const int row = m_table->rowCount();
    m_table->insertRow(row);

    // Timestamp: current time in milliseconds
    const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
    m_table->setItem(row, ColTimestamp, new QTableWidgetItem(QString::number(now_ms)));
    m_table->setItem(row, ColProtocol,  new QTableWidgetItem(proto));
    m_table->setItem(row, ColType,      new QTableWidgetItem(type));
    m_table->setItem(row, ColId,        new QTableWidgetItem(
        QString("0x%1").arg(id, 3, 16, QChar('0')).toUpper()));
    m_table->setItem(row, ColDetails,   new QTableWidgetItem(details));

    // Auto-scroll to bottom
    m_table->scrollToBottom();
}

// ---------------------------------------------------------------------------
// CANopen decoder
// ---------------------------------------------------------------------------
void ProtocolPanel::decodeCanopen(const socketspy::core::CanFrame& f)
{
    using namespace socketspy::protocols::canopen;

    const uint32_t id  = f.id;
    const uint8_t* d   = f.data;
    const uint8_t  dlc = f.dlc;

    // NMT — ID 0x000
    if (id == 0x000) {
        NmtMessage nmt;
        if (decode_nmt(id, d, dlc, nmt)) {
            const QString details = QString("cmd=%1 node=%2")
                .arg(QString::fromUtf8(nmt_command_name(nmt.command).data()))
                .arg(nmt.node_id);
            addRow("CANopen", "NMT", id, details);
        }
        return;
    }

    // EMCY — ID 0x081..0x0FF
    if (id >= 0x081 && id <= 0x0FF) {
        EmcyFrame emcy;
        if (decode_emcy(id, d, dlc, emcy)) {
            const QString details = QString("node=%1 err=0x%2 class=%3")
                .arg(emcy.node_id)
                .arg(emcy.error_code, 4, 16, QChar('0'))
                .arg(QString::fromUtf8(emcy_error_class(emcy.error_code).data()));
            addRow("CANopen", "EMCY", id, details);
        }
        return;
    }

    // Heartbeat / Boot-up — ID 0x700..0x77F
    if (id >= 0x700 && id <= 0x77F) {
        uint8_t  node_id;
        NmtState state;
        if (decode_heartbeat(id, d, dlc, node_id, state)) {
            const bool bootup = (state == NmtState::Initialising);
            const QString type = bootup ? "Boot-up" : "Heartbeat";
            const QString details = QString("node=%1 state=%2")
                .arg(node_id)
                .arg(QString::fromUtf8(nmt_state_name(state).data()));
            addRow("CANopen", type, id, details);
        }
        return;
    }

    // SDO response — ID 0x580..0x5FF
    if (id >= 0x580 && id <= 0x5FF) {
        const uint8_t node_id = static_cast<uint8_t>(id - 0x580);
        SdoFrame sdo;
        if (decode_sdo_response(id, node_id, d, dlc, sdo)) {
            QString typeStr;
            switch (sdo.type) {
                case SdoResponseType::InitiateUploadExpedited:  typeStr = "SDO UploadExp";   break;
                case SdoResponseType::InitiateUploadSegmented:  typeStr = "SDO UploadSeg";   break;
                case SdoResponseType::UploadSegment:            typeStr = "SDO UploadSeg+";  break;
                case SdoResponseType::InitiateDownloadAck:      typeStr = "SDO DownloadAck"; break;
                case SdoResponseType::DownloadSegmentAck:       typeStr = "SDO DLSegAck";    break;
                case SdoResponseType::AbortTransfer:            typeStr = "SDO Abort";       break;
                default:                                        typeStr = "SDO";              break;
            }
            const QString details = QString("node=%1 idx=0x%2 sub=%3")
                .arg(node_id)
                .arg(sdo.index, 4, 16, QChar('0'))
                .arg(sdo.subindex);
            addRow("CANopen", typeStr, id, details);
        }
        return;
    }

    // PDO — ID 0x180..0x57F
    if (id >= 0x180 && id <= 0x57F) {
        // Determine direction and PDO number
        QString type;
        if      (id >= 0x180 && id <= 0x1FF) type = "TPDO1";
        else if (id >= 0x200 && id <= 0x27F) type = "RPDO1";
        else if (id >= 0x280 && id <= 0x2FF) type = "TPDO2";
        else if (id >= 0x300 && id <= 0x37F) type = "RPDO2";
        else if (id >= 0x380 && id <= 0x3FF) type = "TPDO3";
        else if (id >= 0x400 && id <= 0x47F) type = "RPDO3";
        else if (id >= 0x480 && id <= 0x4FF) type = "TPDO4";
        else                                  type = "RPDO4";

        const uint8_t node_id = static_cast<uint8_t>(id & 0x7F);
        QString hex;
        for (int i = 0; i < dlc; ++i) {
            if (i) hex += ' ';
            hex += QString("%1").arg(d[i], 2, 16, QChar('0')).toUpper();
        }
        const QString details = QString("node=%1 dlc=%2 data=%3")
            .arg(node_id).arg(dlc).arg(hex);
        addRow("CANopen", type, id, details);
    }
}

// ---------------------------------------------------------------------------
// ISO-TP decoder
// ---------------------------------------------------------------------------
void ProtocolPanel::decodeIsoTp(const socketspy::core::CanFrame& f)
{
    using namespace socketspy::protocols::isotp;

    if (f.dlc == 0) return;

    const IsoTpFrameType ft = frame_type(f.data);
    QString type;
    QString details;

    switch (ft) {
        case IsoTpFrameType::Single: {
            type = "SF";
            uint8_t payload[64];
            uint8_t len = decode_single_frame(f.data, f.dlc, payload, sizeof(payload));
            details = QString("len=%1").arg(len);
            break;
        }
        case IsoTpFrameType::First: {
            type = "FF";
            uint8_t first[64];
            uint8_t first_len = 0;
            uint16_t total = decode_first_frame(f.data, f.dlc, first, first_len);
            details = QString("total_len=%1 first_bytes=%2").arg(total).arg(first_len);
            break;
        }
        case IsoTpFrameType::Consecutive: {
            type = "CF";
            uint8_t sn = 0;
            uint8_t payload[64];
            decode_consecutive_frame(f.data, f.dlc, sn, payload);
            details = QString("SN=%1").arg(sn);
            break;
        }
        case IsoTpFrameType::FlowControl:
            type = "FC";
            details = QString("flag=%1 BS=%2 STmin=%3")
                .arg(f.data[0] & 0x0F)
                .arg(f.dlc > 1 ? f.data[1] : 0)
                .arg(f.dlc > 2 ? f.data[2] : 0);
            break;
    }

    addRow("ISO-TP", type, f.id, details);
}

// ---------------------------------------------------------------------------
// J1939 decoder
// ---------------------------------------------------------------------------
void ProtocolPanel::decodeJ1939(const socketspy::core::CanFrame& f)
{
    using namespace socketspy::protocols::j1939;

    // J1939 uses 29-bit extended frames; check extended flag
    // flags bit 0 = FD, we need to check if this is extended (29-bit)
    // J1939 IDs are always > 0x7FF (11-bit max), so check ID range
    if (f.id <= 0x7FF) return;  // Standard 11-bit: not J1939

    const J1939Id jid = decode_j1939_id(f.id);
    const QString pgnName = QString::fromUtf8(pgn_name(jid.pgn).data());

    QString details = QString("PGN=0x%1(%2) SA=0x%3 prio=%4")
        .arg(jid.pgn, 5, 16, QChar('0')).toUpper()
        .arg(pgnName.isEmpty() ? "Unknown" : pgnName)
        .arg(jid.sa, 2, 16, QChar('0')).toUpper()
        .arg(jid.priority);

    if (jid.peer_to_peer)
        details += QString(" DA=0x%1").arg(jid.ps, 2, 16, QChar('0')).toUpper();

    addRow("J1939", "Frame", f.id, details);
}

// ---------------------------------------------------------------------------
// UDS / OBD-II decoder
// ---------------------------------------------------------------------------
void ProtocolPanel::decodeUdsObd(const socketspy::core::CanFrame& f)
{
    if (f.dlc < 2) return;

    const uint8_t b0 = f.data[0];

    // OBD-II responses: mode byte = request_mode + 0x40, IDs 0x7E8..0x7EF
    if (f.id >= 0x7E8 && f.id <= 0x7EF) {
        using namespace socketspy::protocols::obd2;
        auto resp = decode_obd2(f.data, f.dlc);
        if (resp) {
            const QString pidName = QString::fromUtf8(
                pid_name(resp->mode, resp->pid).data());
            const QString unit = QString::fromUtf8(
                pid_unit(resp->mode, resp->pid).data());
            auto val = interpret_pid(resp->mode, resp->pid, resp->data, resp->data_len);
            QString details = QString("mode=0x%1 PID=0x%2 %3")
                .arg(resp->mode, 2, 16, QChar('0')).toUpper()
                .arg(resp->pid,  2, 16, QChar('0')).toUpper()
                .arg(pidName);
            if (val)
                details += QString(" val=%1 %2").arg(*val).arg(unit);
            addRow("OBD-II", "Response", f.id, details);
            return;
        }
    }

    // OBD-II requests: IDs 0x7DF or 0x7E0..0x7E7
    if (f.id == 0x7DF || (f.id >= 0x7E0 && f.id <= 0x7E7)) {
        const uint8_t mode = b0;
        const uint8_t pid  = f.dlc > 1 ? f.data[1] : 0;
        using namespace socketspy::protocols::obd2;
        const QString pname = QString::fromUtf8(pid_name(mode, pid).data());
        const QString details = QString("mode=0x%1 PID=0x%2 %3")
            .arg(mode, 2, 16, QChar('0')).toUpper()
            .arg(pid,  2, 16, QChar('0')).toUpper()
            .arg(pname);
        addRow("OBD-II", "Request", f.id, details);
        return;
    }

    // UDS: diagnostic IDs typically 0x700..0x7FF range (other than above)
    if (f.id >= 0x700 && f.id <= 0x7FF) {
        using namespace socketspy::protocols::uds;
        // data[0] might be ISO-TP PCI byte; UDS sits inside ISO-TP
        // For single-frame ISO-TP: data[0] = 0x0N (N=length), data[1] = service ID
        const uint8_t pci = b0 & 0xF0;
        uint8_t svcByte = f.data[1];
        uint8_t subFunc = f.dlc > 2 ? f.data[2] : 0;

        if (pci == 0x00 && f.dlc >= 2) {  // ISO-TP single frame
            const auto svc = static_cast<Service>(svcByte);
            const QString svcName = QString::fromUtf8(service_name(svc).data());
            const QString details = QString("SID=0x%1(%2) sub=0x%3")
                .arg(svcByte, 2, 16, QChar('0')).toUpper()
                .arg(svcName.isEmpty() ? "?" : svcName)
                .arg(subFunc, 2, 16, QChar('0')).toUpper();
            addRow("UDS", "Request", f.id, details);
        } else {
            // Raw fallback
            const QString details = QString("data[0]=0x%1 data[1]=0x%2")
                .arg(b0,        2, 16, QChar('0')).toUpper()
                .arg(svcByte,   2, 16, QChar('0')).toUpper();
            addRow("UDS", "Frame", f.id, details);
        }
    }
}

// ---------------------------------------------------------------------------
// NMEA 2000 decoder
// ---------------------------------------------------------------------------
void ProtocolPanel::decodeNmea2000(const socketspy::core::CanFrame& f)
{
    using namespace socketspy::protocols::nmea2000;

    if (f.id <= 0x7FF) return;  // NMEA 2000 uses 29-bit IDs

    const Nmea2000Frame nmea = decode_nmea2000_id(f.id, f.data, f.dlc);
    const QString pgnName = QString::fromUtf8(pgn_name(nmea.pgn).data());

    QString details = QString("PGN=%1(%2) src=%3 dst=%4")
        .arg(nmea.pgn)
        .arg(pgnName.isEmpty() ? "Unknown" : pgnName)
        .arg(nmea.src)
        .arg(nmea.dst);

    addRow("NMEA2000", "Frame", f.id, details);
}

// ---------------------------------------------------------------------------
// "All" mode: try protocols in priority order
// ---------------------------------------------------------------------------
void ProtocolPanel::decodeAll(const socketspy::core::CanFrame& f)
{
    using namespace socketspy::protocols::canopen;

    const uint32_t id  = f.id;
    const uint8_t* d   = f.data;
    const uint8_t  dlc = f.dlc;

    // CANopen ranges
    if (id == 0x000) { decodeCanopen(f); return; }
    if (id >= 0x081 && id <= 0x0FF) { decodeCanopen(f); return; }
    if (id >= 0x180 && id <= 0x57F) { decodeCanopen(f); return; }
    if (id >= 0x580 && id <= 0x5FF) { decodeCanopen(f); return; }
    if (id >= 0x700 && id <= 0x77F) { decodeCanopen(f); return; }

    // OBD-II / UDS ranges
    if (id == 0x7DF || (id >= 0x7E0 && id <= 0x7EF)) { decodeUdsObd(f); return; }
    if (id >= 0x700 && id <= 0x7FF) { decodeUdsObd(f); return; }

    // 29-bit extended frame: try J1939 / NMEA2000
    if (id > 0x7FF) {
        // Heuristic: NMEA2000 uses specific PGN ranges
        // J1939 is the default for 29-bit
        decodeJ1939(f);
        return;
    }

    // ISO-TP: applicable to any frame with valid PCI byte
    if (dlc >= 1) {
        const uint8_t pci_nibble = (d[0] >> 4) & 0x0F;
        if (pci_nibble <= 3) {
            decodeIsoTp(f);
            return;
        }
    }

    // Raw fallback
    QString hex;
    for (int i = 0; i < dlc; ++i) {
        if (i) hex += ' ';
        hex += QString("%1").arg(d[i], 2, 16, QChar('0')).toUpper();
    }
    addRow("Raw", "Frame", id, hex);
}

// ---------------------------------------------------------------------------
// Main slot — dispatch based on selected protocol
// ---------------------------------------------------------------------------
void ProtocolPanel::onFrameReceived(socketspy::core::CanFrame frame)
{
    const QString proto = m_protoCombo->currentText();

    if (proto == "CANopen")    { decodeCanopen(frame);  return; }
    if (proto == "ISO-TP")     { decodeIsoTp(frame);    return; }
    if (proto == "J1939")      { decodeJ1939(frame);    return; }
    if (proto == "UDS/OBD-II") { decodeUdsObd(frame);   return; }
    if (proto == "NMEA 2000")  { decodeNmea2000(frame); return; }
    // "All"
    decodeAll(frame);
}

} // namespace socketspy::gui
