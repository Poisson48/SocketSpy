#pragma once
#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <cstdint>
#include "cancore.h"

namespace socketspy::gui {

class FuzzerPanel : public QWidget {
    Q_OBJECT

public:
    explicit FuzzerPanel(QWidget* parent = nullptr);

private slots:
    void onToggleFuzz(bool checked);
    void onFuzzTick();
    void refreshIfaces();

private:
    void setupUi();
    bool buildFrame(socketspy::core::CanFrame& frame, QString& err);

    QComboBox*   m_iface{nullptr};
    QLineEdit*   m_id{nullptr};
    QSpinBox*    m_dlc{nullptr};
    QComboBox*   m_mode{nullptr};
    QCheckBox*   m_fd{nullptr};
    QSpinBox*    m_interval{nullptr};
    QPushButton* m_startStop{nullptr};
    QLabel*      m_counter{nullptr};
    QLabel*      m_status{nullptr};
    QTimer*      m_timer{nullptr};

    uint64_t     m_count{0};
    bool         m_running{false};
    uint8_t      m_incrementState[64]{};
    int          m_bitFlipPos{0};
};

} // namespace socketspy::gui
