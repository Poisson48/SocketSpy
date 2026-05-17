#pragma once
#include <QWidget>
#include <QHash>
#include <QVector>
#include <QByteArray>
#include <cstdint>
#include "cancore.h"
#include "range_state_scan.h"

class QComboBox;
class QPushButton;
class QProgressBar;
class QTableWidget;

namespace socketspy::gui {

class RangeStatePanel : public QWidget {
    Q_OBJECT

public:
    explicit RangeStatePanel(QWidget* parent = nullptr);

public slots:
    void onFrameReceived(const socketspy::core::CanFrame& frame);

private slots:
    void onScan();
    void onScanProgress(int value);
    void onScanFinished(QVector<socketspy::gui::ScanResult> results);

private:
    void setupUi();
    void populateResults(const QVector<ScanResult>& results);

    QHash<uint32_t, QVector<QByteArray>> m_frames;

    QComboBox*    m_idCombo{nullptr};
    QPushButton*  m_scanBtn{nullptr};
    QProgressBar* m_progress{nullptr};
    QTableWidget* m_results{nullptr};
};

} // namespace socketspy::gui
