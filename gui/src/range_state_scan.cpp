#include "range_state_scan.h"
#include <cmath>
#include <algorithm>

namespace socketspy::gui {

RangeStateScanWorker::RangeStateScanWorker(QVector<QByteArray> frames, QObject* parent)
    : QObject(parent), m_frames(std::move(frames))
{}

// Extract bit_len bits starting at byte_offset*8 in little-endian or
// big-endian (Motorola) fashion and return as an unsigned integer cast to double.
double RangeStateScanWorker::extractSignal(const QByteArray& data,
                                            int byteOffset, int bitLen,
                                            bool bigEndian)
{
    if (byteOffset < 0 || byteOffset >= data.size()) return 0.0;

    uint64_t raw = 0;

    if (!bigEndian) {
        // Little-endian: read bytes starting at byteOffset, LSB first
        int bitsLeft = bitLen;
        int byteIdx  = byteOffset;
        int shift    = 0;
        while (bitsLeft > 0 && byteIdx < data.size()) {
            int bitsFromThisByte = std::min(bitsLeft, 8);
            uint64_t mask = (1ULL << bitsFromThisByte) - 1;
            raw |= (static_cast<uint64_t>(
                        static_cast<uint8_t>(data[byteIdx])) & mask) << shift;
            shift    += bitsFromThisByte;
            bitsLeft -= bitsFromThisByte;
            ++byteIdx;
        }
    } else {
        // Big-endian (Motorola): MSB resides at byteOffset, read MSB→LSB
        int bitsLeft = bitLen;
        int byteIdx  = byteOffset;
        while (bitsLeft > 0 && byteIdx < data.size()) {
            int bitsFromThisByte = std::min(bitsLeft, 8);
            int rightShift = 8 - bitsFromThisByte;
            uint8_t mask = static_cast<uint8_t>((0xFF >> rightShift));
            raw = (raw << bitsFromThisByte) |
                  (static_cast<uint64_t>(
                       static_cast<uint8_t>(data[byteIdx])) & mask);
            bitsLeft -= bitsFromThisByte;
            ++byteIdx;
        }
    }

    return static_cast<double>(raw);
}

void RangeStateScanWorker::run()
{
    if (m_frames.isEmpty()) {
        emit finished({});
        return;
    }

    // Determine max DLC from collected frames
    int maxDlc = 0;
    for (const auto& f : m_frames)
        maxDlc = std::max(maxDlc, static_cast<int>(f.size()));
    if (maxDlc == 0) { emit finished({}); return; }

    QVector<ScanResult> results;

    // Total iterations for progress: byteOffset × bitLength × 2 endians
    // byteOffset: 0..maxDlc-1, bitLength: 1..16
    int total = maxDlc * 16 * 2;
    int done  = 0;

    for (int byteOffset = 0; byteOffset < maxDlc; ++byteOffset) {
        for (int bitLen = 1; bitLen <= 16; ++bitLen) {
            for (int endianIdx = 0; endianIdx < 2; ++endianIdx) {
                bool bigEndian = (endianIdx == 1);

                // Collect values
                QVector<double> vals;
                vals.reserve(m_frames.size());
                for (const auto& frame : m_frames) {
                    if (frame.size() > byteOffset)
                        vals.push_back(extractSignal(frame, byteOffset, bitLen, bigEndian));
                }

                ++done;
                emit progress(done * 100 / total);

                if (vals.size() < 2) continue;

                double minV = *std::min_element(vals.begin(), vals.end());
                double maxV = *std::max_element(vals.begin(), vals.end());
                double range = maxV - minV;
                if (range <= 0.0) continue; // skip constant signals

                double sum = 0.0;
                for (double v : vals) sum += v;
                double mean = sum / vals.size();

                double var = 0.0;
                for (double v : vals) var += (v - mean) * (v - mean);
                double stddev = std::sqrt(var / vals.size());

                double coherence = 1.0 - (stddev / range);

                results.push_back(ScanResult{byteOffset, bitLen, bigEndian,
                                             minV, maxV, mean, coherence});
            }
        }
    }

    // Sort descending by coherence
    std::sort(results.begin(), results.end(),
              [](const ScanResult& a, const ScanResult& b) {
                  return a.coherence > b.coherence;
              });

    emit finished(results);
}

} // namespace socketspy::gui
