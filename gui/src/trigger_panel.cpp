#include "trigger_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QRegularExpressionValidator>

namespace socketspy::gui {

TriggerPanel::TriggerPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void TriggerPanel::setupUi() {
    auto* hexValidator = new QRegularExpressionValidator(
        QRegularExpression("(0x)?[0-9A-Fa-f]{1,8}"), this);
    auto* byteSeqValidator = new QRegularExpressionValidator(
        QRegularExpression("([0-9A-Fa-f]{1,2}\\s*){0,8}"), this);

    m_enable      = new QCheckBox("Enable trigger", this);
    m_id          = new QLineEdit("0x000", this);
    m_idMask      = new QLineEdit("0xFFFFFFFF", this);
    m_dataPattern = new QLineEdit(this);
    m_dataMask    = new QLineEdit(this);
    m_action      = new QComboBox(this);
    m_armBtn      = new QPushButton("Arm", this);
    m_disarmBtn   = new QPushButton("Disarm", this);

    m_id->setValidator(hexValidator);
    m_idMask->setValidator(hexValidator);
    m_dataPattern->setValidator(byteSeqValidator);
    m_dataMask->setValidator(byteSeqValidator);
    m_dataPattern->setPlaceholderText("e.g. 01 FF 00  (leave empty to ignore)");
    m_dataMask->setPlaceholderText("e.g. FF FF 00  (per-byte mask)");

    m_action->addItem("Start Record", QVariant::fromValue(int(0)));
    m_action->addItem("Stop Record",  QVariant::fromValue(int(1)));
    m_action->addItem("Bookmark",     QVariant::fromValue(int(2)));

    m_disarmBtn->setEnabled(false);

    auto* grid = new QGridLayout;
    grid->addWidget(new QLabel("CAN ID:", this),      0, 0);
    grid->addWidget(m_id,                              0, 1);
    grid->addWidget(new QLabel("ID Mask:", this),      0, 2);
    grid->addWidget(m_idMask,                          0, 3);
    grid->addWidget(new QLabel("Data Pattern:", this), 1, 0);
    grid->addWidget(m_dataPattern,                     1, 1, 1, 3);
    grid->addWidget(new QLabel("Data Mask:", this),    2, 0);
    grid->addWidget(m_dataMask,                        2, 1, 1, 3);
    grid->addWidget(new QLabel("Action:", this),       3, 0);
    grid->addWidget(m_action,                          3, 1);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(m_armBtn);
    btnRow->addWidget(m_disarmBtn);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_enable);
    layout->addLayout(grid);
    layout->addLayout(btnRow);

    connect(m_armBtn,    &QPushButton::clicked, this, &TriggerPanel::onArm);
    connect(m_disarmBtn, &QPushButton::clicked, this, &TriggerPanel::onDisarm);
}

bool TriggerPanel::parseHexByte(const QString& tok, uint8_t& out) {
    bool ok = false;
    uint v = tok.toUInt(&ok, 16);
    if (ok && v <= 0xFF) { out = static_cast<uint8_t>(v); return true; }
    return false;
}

TriggerConfig TriggerPanel::buildConfig() const {
    TriggerConfig cfg;
    cfg.enabled = m_enable->isChecked();

    auto parseHex32 = [](const QString& s, uint32_t& out) {
        QString t = s;
        if (t.startsWith("0x", Qt::CaseInsensitive)) t.remove(0, 2);
        bool ok = false;
        out = t.toUInt(&ok, 16);
        return ok;
    };

    parseHex32(m_id->text(),     cfg.id);
    parseHex32(m_idMask->text(), cfg.id_mask);

    auto parseByteSeq = [](const QString& s, uint8_t* buf, uint8_t& len) {
        const QStringList toks = s.simplified().split(' ', Qt::SkipEmptyParts);
        len = 0;
        for (const QString& tok : toks) {
            if (len >= 8) break;
            uint8_t b = 0;
            bool ok = false;
            uint v = tok.toUInt(&ok, 16);
            if (ok && v <= 0xFF) b = static_cast<uint8_t>(v);
            buf[len++] = b;
        }
    };

    uint8_t dummy_len = 0;
    parseByteSeq(m_dataPattern->text(), cfg.data,      cfg.data_len);
    parseByteSeq(m_dataMask->text(),    cfg.data_mask, dummy_len);
    (void)dummy_len;

    switch (m_action->currentIndex()) {
        case 0: cfg.action = TriggerConfig::Action::StartRecord; break;
        case 1: cfg.action = TriggerConfig::Action::StopRecord;  break;
        case 2: cfg.action = TriggerConfig::Action::Bookmark;    break;
        default: break;
    }

    return cfg;
}

void TriggerPanel::onArm() {
    TriggerConfig cfg = buildConfig();
    cfg.enabled = true;
    m_enable->setChecked(true);
    m_armBtn->setEnabled(false);
    m_disarmBtn->setEnabled(true);
    emit triggerConfigChanged(cfg);
}

void TriggerPanel::onDisarm() {
    TriggerConfig cfg = buildConfig();
    cfg.enabled = false;
    m_enable->setChecked(false);
    m_armBtn->setEnabled(true);
    m_disarmBtn->setEnabled(false);
    emit triggerConfigChanged(cfg);
}

} // namespace socketspy::gui
