#pragma once
#include <QWidget>
#include <QTimer>
#include <QLabel>
#include <QProgressBar>

namespace socketspy::gui {

class SplashScreen : public QWidget {
    Q_OBJECT
public:
    explicit SplashScreen(QWidget* parent = nullptr);

    void finish(QWidget* mainWindow);

private slots:
    void onTick();

private:
    void buildUi();

    QLabel*       m_statusLabel{nullptr};
    QProgressBar* m_progress{nullptr};
    QTimer*       m_timer{nullptr};
    int           m_step{0};
    static constexpr int kSteps = 6;
};

} // namespace socketspy::gui
