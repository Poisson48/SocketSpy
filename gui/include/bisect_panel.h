#pragma once
#include <QWidget>
#include <QVector>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include <QComboBox>
#include "cancore.h"

namespace socketspy::gui {

class BisectPanel : public QWidget {
    Q_OBJECT

public:
    explicit BisectPanel(QWidget* parent = nullptr);

private slots:
    void onLoadCapture();
    void onReplayFirstHalf();
    void onReplaySecondHalf();
    void onEventInFirstHalf();
    void onEventInSecondHalf();
    void onReset();

private:
    void setupUi();
    void updateDisplay();
    void replayRange(int from, int to);
    bool parseLogFile(const QString& path);

    // State
    QVector<socketspy::core::CanFrame> m_frames;
    int m_lo{0};
    int m_hi{0};

    // UI widgets
    QPushButton* m_loadBtn{nullptr};
    QLabel*      m_frameCountLabel{nullptr};
    QLabel*      m_windowLabel{nullptr};
    QProgressBar* m_progress{nullptr};
    QPushButton* m_replayFirstBtn{nullptr};
    QPushButton* m_replaySecondBtn{nullptr};
    QPushButton* m_eventFirstBtn{nullptr};
    QPushButton* m_eventSecondBtn{nullptr};
    QPushButton* m_resetBtn{nullptr};
    QLabel*      m_resultLabel{nullptr};
    QTableWidget* m_table{nullptr};
    QComboBox*   m_iface{nullptr};
};

} // namespace socketspy::gui
