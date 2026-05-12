#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>

namespace socketspy::gui {

class TransmitPanel : public QWidget {
    Q_OBJECT

public:
    explicit TransmitPanel(QWidget* parent = nullptr);

private slots:
    void onSend();

private:
    void setupUi();
    bool validate(QString& errorMsg) const;

    QLineEdit*   m_iface{nullptr};
    QLineEdit*   m_id{nullptr};
    QSpinBox*    m_dlc{nullptr};
    QLineEdit*   m_data{nullptr};
    QCheckBox*   m_extended{nullptr};
    QPushButton* m_send{nullptr};
    QLabel*      m_status{nullptr};
};

} // namespace socketspy::gui
