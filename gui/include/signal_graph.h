#pragma once
#include <QWidget>
#include <QPushButton>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QTimer>
#include <vector>
#include <string>
#include <memory>
#include "cancore.h"

// Temporarily suppress the Qt `signals` keyword macro so that
// socketspy::dbc::Message::signals (a data member) parses correctly.
#pragma push_macro("signals")
#undef signals
#include "dbc_types.h"
#pragma pop_macro("signals")

namespace socketspy::gui {

struct TrackedSignal {
    std::string  signalName;
    uint32_t     msgId{0};
    QLineSeries* series{nullptr};
    double       lastValue{0.0};
    double       minVal{0.0};
    double       maxVal{1.0};
};

class SignalGraphPanel : public QWidget {
    Q_OBJECT

public:
    static constexpr int    kMaxTraces = 4;
    static constexpr double kWindowSec = 10.0;

    explicit SignalGraphPanel(QWidget* parent = nullptr);
    ~SignalGraphPanel() override;

public slots:
    void onDbcLoaded(socketspy::dbc::DbcDatabase db);
    void onFrameReceived(socketspy::core::CanFrame frame);
    void addSignal(QString signalName, uint32_t msgId);

private slots:
    void onClearAll();
    void onScrollAxis();

private:
    void setupUi();
    void rescaleY();

    QChart*      m_chart{nullptr};
    QChartView*  m_view{nullptr};
    QValueAxis*  m_axisX{nullptr};
    QValueAxis*  m_axisY{nullptr};
    QPushButton* m_clearBtn{nullptr};
    QTimer*      m_scrollTimer{nullptr};

    std::vector<TrackedSignal>                   m_traces;
    std::unique_ptr<socketspy::dbc::DbcDatabase> m_dbc;
    bool   m_dbcLoaded{false};
    double m_startTimeSec{0.0};
    bool   m_firstFrame{true};
};

} // namespace socketspy::gui
