#pragma once
#include <QWidget>
#include <QListWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <memory>
#include <unordered_map>
#include "cancore.h"

#pragma push_macro("signals")
#undef signals
#include "dbc_types.h"
#pragma pop_macro("signals")

namespace socketspy::gui {

// ─── BitGridWidget ────────────────────────────────────────────────────────────
class BitGridWidget : public QWidget {
    Q_OBJECT
public:
    static constexpr int kCellPx  = 22;
    static constexpr int kGap     = 2;
    static constexpr int kLeftPad = 52;
    static constexpr int kTopPad  = 22;

    explicit BitGridWidget(QWidget* parent = nullptr);

    void setData(const uint8_t* data, int dlc);
    void setSignals(const std::vector<socketspy::dbc::Signal>& sigs);
    void clearSelection();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void selectionChanged(int startBit, int length);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    int   bitAt(QPoint pos) const;
    QRect cellRect(int bitIdx) const;
    void  emitSelection();

    uint8_t m_data[8]{};
    int     m_dlc{0};
    std::vector<socketspy::dbc::Signal> m_signals;

    int  m_dragStart{-1};
    int  m_dragEnd{-1};
    bool m_dragging{false};
};

// ─── DbcBuilderPanel ─────────────────────────────────────────────────────────
class DbcBuilderPanel : public QWidget {
    Q_OBJECT
public:
    explicit DbcBuilderPanel(QWidget* parent = nullptr);

    void loadDbc(const socketspy::dbc::DbcDatabase& db);
    const socketspy::dbc::DbcDatabase& database() const { return m_db; }

public slots:
    void onFrameReceived(socketspy::core::CanFrame frame);

signals:
    void dbcUpdated(const socketspy::dbc::DbcDatabase& db);

private slots:
    void onIdSelected(int row);
    void onBitsSelected(int startBit, int length);
    void onAddSignal();
    void onDeleteSignal(int sigIdx);
    void onMsgNameChanged(const QString& text);

private:
    void setupUi();
    socketspy::dbc::Message* findOrCreateMessage(uint32_t id);
    void refreshSignalTable();
    void refreshGrid();

    QListWidget*   m_idList{nullptr};
    BitGridWidget* m_grid{nullptr};
    QLabel*        m_noDataLabel{nullptr};

    QLineEdit*      m_msgName{nullptr};
    QLineEdit*      m_sigName{nullptr};
    QSpinBox*       m_startBit{nullptr};
    QSpinBox*       m_bitLen{nullptr};
    QComboBox*      m_byteOrder{nullptr};
    QComboBox*      m_valueType{nullptr};
    QDoubleSpinBox* m_factor{nullptr};
    QDoubleSpinBox* m_offset{nullptr};
    QDoubleSpinBox* m_minVal{nullptr};
    QDoubleSpinBox* m_maxVal{nullptr};
    QLineEdit*      m_unit{nullptr};
    QPushButton*    m_addBtn{nullptr};
    QTableWidget*   m_sigTable{nullptr};

    socketspy::dbc::DbcDatabase m_db;
    uint32_t m_selectedId{0xFFFFFFFF};

    struct LastFrame { uint8_t data[8]{}; int dlc{0}; };
    std::unordered_map<uint32_t, LastFrame> m_lastFrames;
};

} // namespace socketspy::gui
