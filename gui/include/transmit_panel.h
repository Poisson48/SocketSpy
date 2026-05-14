#pragma once
#include <QWidget>
#include <QComboBox>
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

public slots:
    void setCurrentIface(const QString& iface);
    void refreshIfaces();

private slots:
    void onSend();

private:
    void setupUi();
    bool validate(QString& errorMsg) const;

    QComboBox*   m_iface{nullptr};
    QLineEdit*   m_id{nullptr};
    QSpinBox*    m_dlc{nullptr};
    QLineEdit*   m_data{nullptr};
    QCheckBox*   m_extended{nullptr};
    QCheckBox*   m_fd{nullptr};
    QPushButton* m_send{nullptr};
    QLabel*      m_status{nullptr};
};

} // namespace socketspy::gui
