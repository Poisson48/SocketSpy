#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <QGroupBox>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace socketspy::gui {

struct DiffFrame {
    uint32_t id{0};
    uint8_t  dlc{0};
    uint8_t  data[64]{};
};

class DiffPanel : public QWidget {
    Q_OBJECT

public:
    explicit DiffPanel(QWidget* parent = nullptr);

private slots:
    void onOpenA();
    void onOpenB();
    void onCompare();
    void onFilterChanged();

private:
    void setupUi();
    bool parseFile(const QString& path, std::unordered_map<uint32_t, DiffFrame>& out, QString& err);
    bool parseCsvFile(const QString& path, std::unordered_map<uint32_t, DiffFrame>& out);
    bool parseLogFile(const QString& path, std::unordered_map<uint32_t, DiffFrame>& out);
    void populateTable();

    QLineEdit*    m_pathA{nullptr};
    QLineEdit*    m_pathB{nullptr};
    QPushButton*  m_openA{nullptr};
    QPushButton*  m_openB{nullptr};
    QPushButton*  m_compare{nullptr};
    QTableWidget* m_table{nullptr};
    QLabel*       m_statusLabel{nullptr};
    QCheckBox*    m_diffOnly{nullptr};
    QCheckBox*    m_commonOnly{nullptr};

    std::unordered_map<uint32_t, DiffFrame> m_captureA;
    std::unordered_map<uint32_t, DiffFrame> m_captureB;
};

} // namespace socketspy::gui
