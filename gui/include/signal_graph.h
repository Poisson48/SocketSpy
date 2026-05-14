#pragma once
#include <QWidget>
#include <QPushButton>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QHash>
#include <QTimer>
#include <QList>
#include <vector>
#include <string>
#include <memory>
#include "cancore.h"
#include "project.h"

#pragma push_macro("signals")
#undef signals
#include "dbc_types.h"
#pragma pop_macro("signals")

namespace socketspy::gui {

struct TrackedSignal {
    std::string  signalName;
    QString      label;        // user-editable display name (empty = auto)
    uint32_t     msgId{0};
    QLineSeries* series{nullptr};
    double       lastValue{0.0};
    double       minVal{0.0};
    double       maxVal{255.0};
    bool         isRaw{false};
    int          rawByteIdx{0};

    QString canonicalName() const {
        if (isRaw)
            return QString("0x%1[B%2]")
                .arg(QString::number(msgId, 16).toUpper())
                .arg(rawByteIdx);
        return QString::fromStdString(signalName);
    }
    QString displayName() const {
        return label.isEmpty() ? canonicalName() : label;
    }
};

struct TimeMarker {
    double    timeSec{0.0};
    QString   label;
    QColor    color;
    QGraphicsLineItem* line{nullptr};
    QGraphicsTextItem* text{nullptr};
};

// QChartView subclass — captures right-click and resize for custom behaviour.
class GraphChartView : public QChartView {
    Q_OBJECT
public:
    explicit GraphChartView(QChart* chart, QWidget* parent = nullptr);
Q_SIGNALS:
    void rightClickedAt(QPointF viewPos);
    void resized();
protected:
    void mousePressEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
};

class SignalGraphPanel : public QWidget {
    Q_OBJECT

public:
    static constexpr int    kMaxTraces    = 8;
    static constexpr double kWindowSec    = 10.0;
    static constexpr int    kMaxSeriesPts = 1200;

    explicit SignalGraphPanel(QWidget* parent = nullptr);
    ~SignalGraphPanel() override;

    QList<GraphSignalConfig> trackedSignals() const;
    void restoreSignals(const QList<GraphSignalConfig>& list,
                        const QHash<QString,QString>& aliases = {});

Q_SIGNALS:
    void signalAliased(QString canonical, QString alias);

public slots:
    void onDbcLoaded(socketspy::dbc::DbcDatabase db);
    void onFrameReceived(socketspy::core::CanFrame frame);
    void addSignal(QString signalName, uint32_t msgId);
    void addRawSignal(uint32_t msgId, int byteIdx);
    void addFrameSignals(uint32_t id);

private slots:
    void onClearAll();
    void onScrollAxis();
    void onChartRightClick(QPointF viewPos);
    void onAddMarkerNow();

private:
    void setupUi();
    void applyChartTheme();
    void rescaleY();
    void addTrace(TrackedSignal t);
    void renameTrace(int idx);
    void addMarker(double timeSec, const QString& label);
    void clearMarkers();
    void updateMarkerPositions();

    QChart*         m_chart{nullptr};
    GraphChartView* m_view{nullptr};
    QValueAxis*     m_axisX{nullptr};
    QValueAxis*     m_axisY{nullptr};
    QPushButton*    m_clearBtn{nullptr};
    QPushButton*    m_markerBtn{nullptr};
    QTimer*         m_scrollTimer{nullptr};

    std::vector<TrackedSignal>                    m_traces;
    std::vector<TimeMarker>                       m_markers;
    std::unique_ptr<socketspy::dbc::DbcDatabase>  m_dbc;
    bool   m_dbcLoaded{false};
    double m_startTimeSec{0.0};
    bool   m_firstFrame{true};
    double m_currentMaxT{0.0};
};

} // namespace socketspy::gui
