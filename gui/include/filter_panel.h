#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include "filter_model.h"

namespace socketspy::gui {

class FilterPanel : public QWidget {
    Q_OBJECT

public:
    explicit FilterPanel(QWidget* parent = nullptr);
    FrameFilter currentFilter() const;
    void applyFilter(const FrameFilter& f);

signals:
    void filterChanged(socketspy::gui::FrameFilter filter);

private slots:
    void onAnyFieldChanged();
    void onClear();

private:
    void setupUi();
    static bool parseHex(const QString& s, uint32_t& out);

    QCheckBox*   m_enable{nullptr};
    QPushButton* m_clear{nullptr};
    QLineEdit*   m_idMin{nullptr};
    QLineEdit*   m_idMax{nullptr};
    QLineEdit*   m_idMask{nullptr};
    QLineEdit*   m_dlcMin{nullptr};
    QLineEdit*   m_dlcMax{nullptr};
    QCheckBox*   m_showStd{nullptr};
    QCheckBox*   m_showExt{nullptr};
    QCheckBox*   m_showFd{nullptr};
    QCheckBox*   m_showErr{nullptr};
};

} // namespace socketspy::gui
