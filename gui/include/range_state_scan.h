#pragma once
#include <QByteArray>
#include <QVector>
#include <QObject>

namespace socketspy::gui {

struct ScanResult {
    int    byteOffset;
    int    bitLength;
    bool   bigEndian;
    double minVal;
    double maxVal;
    double mean;
    double coherence; // 1.0 - stddev/range if range>0, else 0.0
};

class RangeStateScanWorker : public QObject {
    Q_OBJECT
public:
    explicit RangeStateScanWorker(QVector<QByteArray> frames, QObject* parent = nullptr);

    static double extractSignal(const QByteArray& data, int byteOffset, int bitLen,
                                 bool bigEndian);

public slots:
    void run();

signals:
    void progress(int value);
    void finished(QVector<socketspy::gui::ScanResult> results);

private:
    QVector<QByteArray> m_frames;
};

} // namespace socketspy::gui
