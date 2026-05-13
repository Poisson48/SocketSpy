#pragma once
#include <QWidget>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include "trigger_config.h"

namespace socketspy::gui {

class TriggerPanel : public QWidget {
    Q_OBJECT

public:
    explicit TriggerPanel(QWidget* parent = nullptr);
    TriggerConfig currentConfig() const;
    void applyConfig(const TriggerConfig& cfg);

signals:
    void triggerConfigChanged(socketspy::gui::TriggerConfig cfg);

private slots:
    void onArm();
    void onDisarm();

private:
    void setupUi();
    static bool parseHexByte(const QString& tok, uint8_t& out);
    TriggerConfig buildConfig() const;

    QCheckBox*   m_enable{nullptr};
    QLineEdit*   m_id{nullptr};
    QLineEdit*   m_idMask{nullptr};
    QLineEdit*   m_dataPattern{nullptr};
    QLineEdit*   m_dataMask{nullptr};
    QComboBox*   m_action{nullptr};
    QPushButton* m_armBtn{nullptr};
    QPushButton* m_disarmBtn{nullptr};
};

} // namespace socketspy::gui
