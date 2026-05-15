#pragma once
#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QTimer>

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
    void onTogglePeriodic(bool checked);
    void onPeriodicTick();

private:
    void setupUi();
    bool validate(QString& errorMsg) const;
    bool sendFrame();   // shared by onSend and onPeriodicTick

    QComboBox*   m_iface{nullptr};
    QLineEdit*   m_id{nullptr};
    QSpinBox*    m_dlc{nullptr};
    QLineEdit*   m_data{nullptr};
    QCheckBox*   m_extended{nullptr};
    QCheckBox*   m_fd{nullptr};
    QPushButton* m_send{nullptr};
    QLabel*      m_status{nullptr};

    // Periodic send controls
    QCheckBox*   m_periodicChk{nullptr};    // enable/disable periodic mode
    QSpinBox*    m_intervalMs{nullptr};      // period in milliseconds
    QLabel*      m_periodicStatus{nullptr};  // sent counter
    QTimer*      m_periodicTimer{nullptr};
    int          m_periodicCount{0};
};

} // namespace socketspy::gui
