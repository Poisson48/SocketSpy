#include "asc_io.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStringList>
#include <algorithm>
#include <cstdint>

namespace socketspy::gui {

// ---------------------------------------------------------------------------
// AscWriter
// ---------------------------------------------------------------------------

bool AscWriter::write(const QList<socketspy::core::CanFrame>& frames,
                      const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;

    QTextStream out(&f);

    // Header
    const QString dateStr =
        QDateTime::currentDateTime().toString("ddd MMM dd HH:mm:ss.zzz yyyy");
    out << "date " << dateStr << "\n";
    out << "base hex  timestamps relative\n";
    out << "no_internal_events\n";
    out << "// SocketSpy\n";
    out << "begin measurement\n";

    if (!frames.isEmpty()) {
        const uint64_t firstUs = frames.first().timestamp_us;
        for (const auto& fr : frames) {
            double ts = static_cast<double>(fr.timestamp_us - firstUs) / 1'000'000.0;
            const uint8_t dlc = std::min(fr.dlc, static_cast<uint8_t>(64));

            // timestamp channel ID Rx d DLC B0 B1 ...
            out << QString("   %1 1  %2 Rx d %3 ")
                       .arg(ts, 0, 'f', 6)
                       .arg(QString::number(fr.id, 16).toUpper())
                       .arg(dlc);

            for (uint8_t i = 0; i < dlc; ++i) {
                if (i > 0) out << ' ';
                out << QString("%1").arg(fr.data[i], 2, 16, QChar('0')).toUpper();
            }
            out << "\n";
        }
    }

    out << "end measurement\n";
    f.close();
    return true;
}

// ---------------------------------------------------------------------------
// AscReader
// ---------------------------------------------------------------------------

QList<socketspy::core::CanFrame> AscReader::read(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QList<socketspy::core::CanFrame> frames;
    QTextStream in(&f);

    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        // Skip header/comment lines
        const QString low = line.toLower();
        if (low.startsWith("date")  || low.startsWith("//")    ||
            low.startsWith("base")  || low.startsWith("no_")   ||
            low.startsWith("begin") || low.startsWith("end"))
            continue;

        // Data line starts with a float timestamp
        const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        // Minimum: TIMESTAMP CHANNEL ID Rx/Tx d DLC
        if (parts.size() < 6) continue;

        bool tsOk = false;
        const double ts = parts[0].toDouble(&tsOk);
        if (!tsOk) continue;

        // parts[2] = ID (hex), parts[3] = Rx/Tx, parts[4] = 'd', parts[5] = DLC
        bool idOk = false;
        const uint32_t id = parts[2].toUInt(&idOk, 16);
        if (!idOk) continue;

        bool dlcOk = false;
        const int dlc = parts[5].toInt(&dlcOk);
        if (!dlcOk || dlc < 0 || dlc > 64) continue;

        socketspy::core::CanFrame fr{};
        fr.id           = id;
        fr.timestamp_us = static_cast<uint64_t>(ts * 1'000'000.0);
        fr.dlc          = static_cast<uint8_t>(dlc);

        // Bytes start at parts[6]
        const int available = static_cast<int>(parts.size()) - 6;
        const int nbytes    = std::min(dlc, std::min(available, 64));
        for (int i = 0; i < nbytes; ++i) {
            bool byteOk = false;
            fr.data[i] = static_cast<uint8_t>(parts[6 + i].toUInt(&byteOk, 16));
        }

        frames.append(fr);
    }

    return frames;
}

} // namespace socketspy::gui
