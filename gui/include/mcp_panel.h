#pragma once
#include <QWidget>
#include <QProcess>

QT_BEGIN_NAMESPACE
class QRadioButton;
class QSpinBox;
class QPushButton;
class QLabel;
class QPlainTextEdit;
class QGroupBox;
QT_END_NAMESPACE

namespace socketspy::gui {

class McpPanel : public QWidget {
    Q_OBJECT

public:
    explicit McpPanel(QWidget* parent = nullptr);
    ~McpPanel() override;

private slots:
    void onStartClicked();
    void onStopClicked();
    void onModeChanged();
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void onReadyReadStdout();
    void onReadyReadStderr();

private:
    void buildUi();
    void setRunning(bool running);
    void appendConsole(const QString& text, bool isError = false);

    // Mode
    QRadioButton*   m_tcpRadio{nullptr};
    QRadioButton*   m_stdioRadio{nullptr};

    // Port
    QSpinBox*       m_portSpin{nullptr};
    QWidget*        m_portRow{nullptr};

    // Controls
    QPushButton*    m_startBtn{nullptr};
    QPushButton*    m_stopBtn{nullptr};

    // Status
    QLabel*         m_statusLabel{nullptr};

    // Console
    QPlainTextEdit* m_console{nullptr};

    // Process
    QProcess*       m_process{nullptr};
    int             m_currentPort{7891};
};

} // namespace socketspy::gui
