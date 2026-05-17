#include "diff_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>

namespace socketspy::gui {

static QString bytesToHexStr(const uint8_t* data, int len) {
    QString hex;
    hex.reserve(len * 3);
    for (int i = 0; i < len; ++i) {
        if (i) hex += ' ';
        hex += QString("%1").arg(data[i], 2, 16, QChar('0')).toUpper();
    }
    return hex;
}

DiffPanel::DiffPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void DiffPanel::setupUi() {
    m_pathA  = new QLineEdit(this);
    m_pathA->setPlaceholderText("Path to capture A (.log or .csv)…");
    m_pathA->setReadOnly(true);
    m_openA  = new QPushButton("Open…", this);

    m_pathB  = new QLineEdit(this);
    m_pathB->setPlaceholderText("Path to capture B (.log or .csv)…");
    m_pathB->setReadOnly(true);
    m_openB  = new QPushButton("Open…", this);

    auto makeFileRow = [](QLineEdit* edit, QPushButton* btn) -> QHBoxLayout* {
        auto* row = new QHBoxLayout;
        row->addWidget(edit, 1);
        row->addWidget(btn);
        return row;
    };

    auto* form = new QFormLayout;
    form->addRow("Capture A:", makeFileRow(m_pathA, m_openA));
    form->addRow("Capture B:", makeFileRow(m_pathB, m_openB));

    m_compare = new QPushButton("Compare", this);
    m_compare->setEnabled(false);

    m_diffOnly   = new QCheckBox("Show only differences", this);
    m_commonOnly = new QCheckBox("Show only common IDs", this);

    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(m_diffOnly);
    filterRow->addWidget(m_commonOnly);
    filterRow->addStretch();

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #6b7280;");

    auto* compareRow = new QHBoxLayout;
    compareRow->addStretch();
    compareRow->addWidget(m_compare);
    compareRow->addStretch();

    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels({"CAN ID", "Status", "Data A", "Data B", "Delta"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(24);
    m_table->setAlternatingRowColors(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addLayout(form);
    layout->addLayout(compareRow);
    layout->addLayout(filterRow);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_table, 1);

    connect(m_openA,    &QPushButton::clicked, this, &DiffPanel::onOpenA);
    connect(m_openB,    &QPushButton::clicked, this, &DiffPanel::onOpenB);
    connect(m_compare,  &QPushButton::clicked, this, &DiffPanel::onCompare);
    connect(m_diffOnly,   &QCheckBox::toggled, this, &DiffPanel::onFilterChanged);
    connect(m_commonOnly, &QCheckBox::toggled, this, &DiffPanel::onFilterChanged);
}

void DiffPanel::onOpenA() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Open Capture A", {},
        "CAN Captures (*.log *.csv);;All Files (*)");
    if (path.isEmpty()) return;
    m_pathA->setText(path);
    m_captureA.clear();
    m_compare->setEnabled(!m_pathA->text().isEmpty() && !m_pathB->text().isEmpty());
}

void DiffPanel::onOpenB() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Open Capture B", {},
        "CAN Captures (*.log *.csv);;All Files (*)");
    if (path.isEmpty()) return;
    m_pathB->setText(path);
    m_captureB.clear();
    m_compare->setEnabled(!m_pathA->text().isEmpty() && !m_pathB->text().isEmpty());
}

bool DiffPanel::parseLogFile(const QString& path,
                              std::unordered_map<uint32_t, DiffFrame>& out)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QTextStream ts(&f);
    while (!ts.atEnd()) {
        QString line = ts.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        // Format: (ts) IFACE ID#DATA
        // e.g.  (1.234567) vcan0 00000123#DEADBEEF
        int hashIdx = line.lastIndexOf('#');
        if (hashIdx < 0) continue;
        QString dataHex = line.mid(hashIdx + 1).trimmed();
        QString left    = line.left(hashIdx).trimmed();
        // last token before # is the ID
        int spaceIdx = left.lastIndexOf(' ');
        if (spaceIdx < 0) continue;
        QString idStr = left.mid(spaceIdx + 1).trimmed();
        bool ok = false;
        uint32_t id = idStr.toUInt(&ok, 16);
        if (!ok) continue;

        DiffFrame fr;
        fr.id  = id;
        fr.dlc = static_cast<uint8_t>(dataHex.length() / 2);
        if (fr.dlc > 64) fr.dlc = 64;
        for (int i = 0; i < fr.dlc; ++i)
            fr.data[i] = static_cast<uint8_t>(dataHex.mid(i * 2, 2).toUInt(nullptr, 16));
        out[id] = fr;  // last frame per ID wins
    }
    return true;
}

bool DiffPanel::parseCsvFile(const QString& path,
                               std::unordered_map<uint32_t, DiffFrame>& out)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QTextStream ts(&f);
    bool header = true;
    while (!ts.atEnd()) {
        QString line = ts.readLine().trimmed();
        if (line.isEmpty()) continue;
        if (header) { header = false; continue; }
        QStringList cols = line.split(',');
        if (cols.size() < 4) continue;
        // Expected: Timestamp,ID,DLC,Data,...
        bool ok = false;
        uint32_t id = cols[1].trimmed().toUInt(&ok, 16);
        if (!ok) continue;
        QString dataStr = cols[3].trimmed().replace(' ', "");
        DiffFrame fr;
        fr.id  = id;
        fr.dlc = static_cast<uint8_t>(dataStr.length() / 2);
        if (fr.dlc > 64) fr.dlc = 64;
        for (int i = 0; i < fr.dlc; ++i)
            fr.data[i] = static_cast<uint8_t>(dataStr.mid(i * 2, 2).toUInt(nullptr, 16));
        out[id] = fr;
    }
    return true;
}

bool DiffPanel::parseFile(const QString& path,
                           std::unordered_map<uint32_t, DiffFrame>& out,
                           QString& err)
{
    bool ok = path.endsWith(".csv", Qt::CaseInsensitive)
        ? parseCsvFile(path, out)
        : parseLogFile(path, out);
    if (!ok) err = "Cannot read: " + path;
    return ok;
}

void DiffPanel::onCompare() {
    m_captureA.clear();
    m_captureB.clear();
    m_table->setRowCount(0);

    QString err;
    if (!parseFile(m_pathA->text(), m_captureA, err)) {
        QMessageBox::critical(this, "Diff", err); return;
    }
    if (!parseFile(m_pathB->text(), m_captureB, err)) {
        QMessageBox::critical(this, "Diff", err); return;
    }
    populateTable();
}

void DiffPanel::populateTable() {
    m_table->setRowCount(0);

    bool diffOnly   = m_diffOnly->isChecked();
    bool commonOnly = m_commonOnly->isChecked();

    auto addRow = [&](uint32_t id, const QString& status,
                      const QString& dataA, const QString& dataB,
                      const QString& delta, const QColor& color)
    {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        auto item = [&](const QString& text) -> QTableWidgetItem* {
            auto* it = new QTableWidgetItem(text);
            if (color.isValid()) it->setBackground(QBrush(color));
            return it;
        };
        m_table->setItem(row, 0, item(QString("%1").arg(id, 8, 16, QChar('0')).toUpper()));
        m_table->setItem(row, 1, item(status));
        m_table->setItem(row, 2, item(dataA));
        m_table->setItem(row, 3, item(dataB));
        m_table->setItem(row, 4, item(delta));
    };

    // Collect all IDs
    std::unordered_map<uint32_t, int> allIds;
    for (auto& [id, _] : m_captureA) allIds[id]++;
    for (auto& [id, _] : m_captureB) allIds[id]++;

    int aOnly = 0, bOnly = 0, changed = 0, same = 0;

    for (auto& [id, _] : allIds) {
        bool inA = m_captureA.count(id) > 0;
        bool inB = m_captureB.count(id) > 0;

        if (inA && !inB) {
            ++aOnly;
            if (commonOnly) continue;
            const auto& fa = m_captureA[id];
            addRow(id, "A only", bytesToHexStr(fa.data, fa.dlc), "–", "–",
                   QColor(255, 230, 200));
        } else if (!inA && inB) {
            ++bOnly;
            if (commonOnly) continue;
            const auto& fb = m_captureB[id];
            addRow(id, "B only", "–", bytesToHexStr(fb.data, fb.dlc), "–",
                   QColor(200, 230, 255));
        } else {
            const auto& fa = m_captureA[id];
            const auto& fb = m_captureB[id];
            QString hexA = bytesToHexStr(fa.data, fa.dlc);
            QString hexB = bytesToHexStr(fb.data, fb.dlc);

            // Compute byte-level delta
            QString delta;
            int maxDlc = std::max(fa.dlc, fb.dlc);
            bool differs = (fa.dlc != fb.dlc);
            for (int b = 0; b < maxDlc; ++b) {
                uint8_t byteA = b < fa.dlc ? fa.data[b] : 0;
                uint8_t byteB = b < fb.dlc ? fb.data[b] : 0;
                if (byteA != byteB) {
                    differs = true;
                    if (!delta.isEmpty()) delta += ' ';
                    delta += QString("B%1:%2→%3")
                        .arg(b)
                        .arg(byteA, 2, 16, QChar('0')).toUpper()
                        .arg(byteB, 2, 16, QChar('0')).toUpper();
                }
            }

            if (differs) {
                ++changed;
                if (!delta.isEmpty()) delta.prepend("");
                addRow(id, "Changed", hexA, hexB, delta, QColor(255, 255, 180));
            } else {
                ++same;
                if (diffOnly) continue;
                addRow(id, "Same", hexA, hexB, "–", {});
            }
        }
    }

    m_statusLabel->setText(
        QString("IDs: A-only=%1  B-only=%2  Changed=%3  Same=%4  |  Total rows: %5")
        .arg(aOnly).arg(bOnly).arg(changed).arg(same).arg(m_table->rowCount()));
}

void DiffPanel::onFilterChanged() {
    if (!m_captureA.empty() || !m_captureB.empty())
        populateTable();
}

} // namespace socketspy::gui
