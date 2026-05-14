#pragma once
#include <QDialog>
#include <QTreeWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QStackedWidget>
#include "sim_profile.h"

namespace socketspy::gui {

class SimProfileEditor : public QDialog {
    Q_OBJECT

public:
    explicit SimProfileEditor(QWidget* parent = nullptr);

    SimProfile result() const;
    QString    savedPath() const { return m_savedPath; }

private slots:
    void onAddNode();
    void onAddMessage();
    void onAddSignal();
    void onRemove();
    void onSelectionChanged();
    void onAccept();

private:
    enum ItemType { Node = 1001, Message = 1002, Signal = 1003 };
    enum DataRole {
        IdRole = Qt::UserRole, PeriodRole, DlcRole,
        StartBitRole, LengthRole, FactorRole, OffsetRole,
        MinRole, MaxRole, DefaultRole
    };

    void setupUi();
    QTreeWidgetItem* addNodeItem(const QString& name);
    QTreeWidgetItem* addMessageItem(QTreeWidgetItem* parent, uint32_t id, int period, uint8_t dlc);
    QTreeWidgetItem* addSignalItem(QTreeWidgetItem* parent, const SimSignal& s);
    SimProfile       buildProfile() const;
    bool             saveToFile(const SimProfile& p);

    QLineEdit*      m_profileName{nullptr};
    QTreeWidget*    m_tree{nullptr};
    QPushButton*    m_addNodeBtn{nullptr};
    QPushButton*    m_addMsgBtn{nullptr};
    QPushButton*    m_addSigBtn{nullptr};
    QPushButton*    m_removeBtn{nullptr};
    QStackedWidget* m_stack{nullptr};

    // Node form
    QLineEdit* m_nodeNameEdit{nullptr};

    // Message form
    QLineEdit* m_msgIdEdit{nullptr};
    QSpinBox*  m_msgPeriodSpin{nullptr};
    QSpinBox*  m_msgDlcSpin{nullptr};

    // Signal form
    QLineEdit*      m_sigNameEdit{nullptr};
    QSpinBox*       m_sigStartBitSpin{nullptr};
    QSpinBox*       m_sigLengthSpin{nullptr};
    QDoubleSpinBox* m_sigFactorSpin{nullptr};
    QDoubleSpinBox* m_sigOffsetSpin{nullptr};
    QDoubleSpinBox* m_sigMinSpin{nullptr};
    QDoubleSpinBox* m_sigMaxSpin{nullptr};
    QDoubleSpinBox* m_sigDefaultSpin{nullptr};

    QString          m_savedPath;
    QTreeWidgetItem* m_current{nullptr};
    bool             m_blockSync{false};
};

} // namespace socketspy::gui
