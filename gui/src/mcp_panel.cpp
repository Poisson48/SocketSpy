#include "mcp_panel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QRadioButton>
#include <QButtonGroup>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QPlainTextEdit>
#include <QGroupBox>
#include <QProcess>
#include <QTimer>
#include <QCoreApplication>
#include <QDir>
#include <QDateTime>
#include <QScrollBar>
#include <QFrame>

namespace socketspy::gui {

// ---------------------------------------------------------------------------
// Tool list (static)
// ---------------------------------------------------------------------------
static const char* const kMcpTools[] = {
    "can_monitor",
    "can_send",
    "can_stop",
    "can_decode",
    "can_replay",
    "can_script",
    "canopen_sdo_read",
    "canopen_sdo_write",
    "canopen_scan",
    "can_diff",
    "get_stats",
};
static constexpr int kMcpToolCount = static_cast<int>(sizeof(kMcpTools) / sizeof(kMcpTools[0]));

// ---------------------------------------------------------------------------
McpPanel::McpPanel(QWidget* parent) : QWidget(parent) {
    m_process = new QProcess(this);
    connect(m_process, &QProcess::started,
            this, &McpPanel::onProcessStarted);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &McpPanel::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &McpPanel::onProcessError);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &McpPanel::onReadyReadStdout);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &McpPanel::onReadyReadStderr);

    buildUi();
}

McpPanel::~McpPanel() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        m_process->waitForFinished(2000);
        if (m_process->state() != QProcess::NotRunning)
            m_process->kill();
    }
}

// ---------------------------------------------------------------------------
void McpPanel::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(16);

    // ── Title ────────────────────────────────────────────────────────────────
    auto* titleLabel = new QLabel(tr("MCP Server"), this);
    titleLabel->setObjectName("panelTitle");
    titleLabel->setStyleSheet(
        "font-size: 18px; font-weight: 700; color: #f1f5f9; letter-spacing: 0.5px;");
    root->addWidget(titleLabel);

    auto* subLabel = new QLabel(
        tr("Model Context Protocol — expose SocketSpy tools to Claude Desktop or any MCP client."),
        this);
    subLabel->setStyleSheet("color: #7c8fa6; font-size: 12px;");
    subLabel->setWordWrap(true);
    root->addWidget(subLabel);

    // ── Config card ──────────────────────────────────────────────────────────
    auto* cfgCard = new QFrame(this);
    cfgCard->setObjectName("mcpCard");
    cfgCard->setStyleSheet(
        "QFrame#mcpCard {"
        "  background: #1a2235;"
        "  border: 1px solid #2a3a52;"
        "  border-radius: 8px;"
        "  padding: 4px;"
        "}");

    auto* cfgLayout = new QGridLayout(cfgCard);
    cfgLayout->setContentsMargins(16, 16, 16, 16);
    cfgLayout->setSpacing(10);

    // Mode
    auto* modeLabel = new QLabel(tr("Mode:"), cfgCard);
    modeLabel->setStyleSheet("color: #7c8fa6; font-size: 12px; font-weight: 600;");

    auto* modeRow = new QHBoxLayout;
    modeRow->setSpacing(16);
    m_tcpRadio   = new QRadioButton(tr("TCP"), cfgCard);
    m_stdioRadio = new QRadioButton(tr("Stdio"), cfgCard);
    m_tcpRadio->setChecked(true);

    auto* modeGroup = new QButtonGroup(this);
    modeGroup->addButton(m_tcpRadio);
    modeGroup->addButton(m_stdioRadio);
    modeRow->addWidget(m_tcpRadio);
    modeRow->addWidget(m_stdioRadio);
    modeRow->addStretch();

    cfgLayout->addWidget(modeLabel, 0, 0);
    cfgLayout->addLayout(modeRow, 0, 1);

    // Port row
    auto* portLabel = new QLabel(tr("Port:"), cfgCard);
    portLabel->setStyleSheet("color: #7c8fa6; font-size: 12px; font-weight: 600;");
    m_portRow = new QWidget(cfgCard);
    auto* portRowLayout = new QHBoxLayout(m_portRow);
    portRowLayout->setContentsMargins(0, 0, 0, 0);
    portRowLayout->setSpacing(8);

    m_portSpin = new QSpinBox(m_portRow);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(7891);
    m_portSpin->setFixedWidth(100);

    portRowLayout->addWidget(m_portSpin);
    portRowLayout->addWidget(new QLabel(tr("(127.0.0.1 only)"), m_portRow));
    portRowLayout->addStretch();

    cfgLayout->addWidget(portLabel, 1, 0);
    cfgLayout->addWidget(m_portRow, 1, 1);

    cfgLayout->setColumnStretch(1, 1);
    root->addWidget(cfgCard);

    connect(m_tcpRadio,   &QRadioButton::toggled, this, &McpPanel::onModeChanged);
    connect(m_stdioRadio, &QRadioButton::toggled, this, &McpPanel::onModeChanged);

    // ── Control row ──────────────────────────────────────────────────────────
    auto* ctrlRow = new QHBoxLayout;
    ctrlRow->setSpacing(10);

    m_startBtn = new QPushButton(tr("Start MCP Server"), this);
    m_startBtn->setObjectName("mcpStartBtn");
    m_startBtn->setMinimumWidth(160);

    m_stopBtn = new QPushButton(tr("Stop"), this);
    m_stopBtn->setProperty("secondary", true);
    m_stopBtn->setMinimumWidth(80);
    m_stopBtn->setEnabled(false);

    m_statusLabel = new QLabel(tr("Stopped"), this);
    m_statusLabel->setObjectName("mcpStatus");
    m_statusLabel->setStyleSheet(
        "color: #7c8fa6; font-size: 12px; font-weight: 600; letter-spacing: 0.3px;");

    ctrlRow->addWidget(m_startBtn);
    ctrlRow->addWidget(m_stopBtn);
    ctrlRow->addSpacing(16);
    ctrlRow->addWidget(m_statusLabel);
    ctrlRow->addStretch();
    root->addLayout(ctrlRow);

    connect(m_startBtn, &QPushButton::clicked, this, &McpPanel::onStartClicked);
    connect(m_stopBtn,  &QPushButton::clicked, this, &McpPanel::onStopClicked);

    // ── Horizontal split: console + tools ────────────────────────────────────
    auto* hSplit = new QHBoxLayout;
    hSplit->setSpacing(16);

    // Console
    auto* consoleGroup = new QGroupBox(tr("Server Output"), this);
    consoleGroup->setStyleSheet(
        "QGroupBox { color: #7c8fa6; font-size: 12px; font-weight: 600;"
        "  border: 1px solid #2a3a52; border-radius: 6px; margin-top: 8px;"
        "  padding-top: 12px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }");
    auto* consoleLayout = new QVBoxLayout(consoleGroup);
    consoleLayout->setContentsMargins(8, 8, 8, 8);

    m_console = new QPlainTextEdit(this);
    m_console->setReadOnly(true);
    m_console->setObjectName("mcpConsole");
    m_console->setPlaceholderText(tr("Server output will appear here…"));
    m_console->setStyleSheet(
        "QPlainTextEdit#mcpConsole {"
        "  background: #0b0f1a;"
        "  color: #a3e635;"
        "  font-family: 'JetBrains Mono', 'Fira Code', 'Cascadia Code', monospace;"
        "  font-size: 12px;"
        "  border: 1px solid #2a3a52;"
        "  border-radius: 4px;"
        "  padding: 6px;"
        "}");
    m_console->setMinimumHeight(160);
    consoleLayout->addWidget(m_console);

    // Tools list
    auto* toolsGroup = new QGroupBox(tr("Available Tools (%1)").arg(kMcpToolCount), this);
    toolsGroup->setStyleSheet(consoleGroup->styleSheet());
    toolsGroup->setFixedWidth(220);
    auto* toolsLayout = new QVBoxLayout(toolsGroup);
    toolsLayout->setContentsMargins(8, 8, 8, 8);
    toolsLayout->setSpacing(4);

    for (int i = 0; i < kMcpToolCount; ++i) {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);

        auto* dot = new QLabel(QString::fromUtf8("\xe2\x97\x8f"), toolsGroup); // filled circle
        dot->setStyleSheet("color: #6366f1; font-size: 8px; padding-top: 2px;");
        dot->setFixedWidth(12);

        auto* lbl = new QLabel(QString::fromLatin1(kMcpTools[i]), toolsGroup);
        lbl->setStyleSheet(
            "color: #f1f5f9; font-family: 'JetBrains Mono','Fira Code',monospace;"
            "font-size: 12px;");

        row->addWidget(dot);
        row->addWidget(lbl);
        row->addStretch();
        toolsLayout->addLayout(row);
    }
    toolsLayout->addStretch();

    hSplit->addWidget(consoleGroup, 1);
    hSplit->addWidget(toolsGroup, 0);
    root->addLayout(hSplit, 1);
}

// ---------------------------------------------------------------------------
void McpPanel::onModeChanged() {
    const bool isTcp = m_tcpRadio->isChecked();
    m_portRow->setVisible(isTcp);
}

void McpPanel::onStartClicked() {
    if (m_process->state() != QProcess::NotRunning)
        return;

    const QString binPath = QCoreApplication::applicationDirPath()
                            + QDir::separator() + "socketspy-mcp";

    QStringList args;
    if (m_tcpRadio->isChecked()) {
        m_currentPort = m_portSpin->value();
        args << "--tcp" << QString::number(m_currentPort);
    } else {
        args << "--stdio";
    }

    appendConsole(QString("[%1] Starting: %2 %3")
                  .arg(QDateTime::currentDateTime().toString("hh:mm:ss"),
                       binPath, args.join(' ')));

    m_process->start(binPath, args);
}

void McpPanel::onStopClicked() {
    if (m_process->state() == QProcess::NotRunning)
        return;

    appendConsole(QString("[%1] Stopping server…")
                  .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));

    m_process->terminate();
    // If it hasn't exited after 3 s, kill it
    QTimer::singleShot(3000, this, [this]() {
        if (m_process->state() != QProcess::NotRunning)
            m_process->kill();
    });
}

// ---------------------------------------------------------------------------
void McpPanel::onProcessStarted() {
    setRunning(true);
    const QString statusText = m_tcpRadio->isChecked()
        ? tr("Running on port %1").arg(m_currentPort)
        : tr("Running (stdio)");
    m_statusLabel->setText(statusText);
    m_statusLabel->setStyleSheet(
        "color: #22c55e; font-size: 12px; font-weight: 600; letter-spacing: 0.3px;");
    appendConsole(QString("[%1] Server started.")
                  .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
}

void McpPanel::onProcessFinished(int exitCode, QProcess::ExitStatus /*status*/) {
    setRunning(false);
    m_statusLabel->setText(
        exitCode == 0 ? tr("Stopped") : tr("Exited (code %1)").arg(exitCode));
    m_statusLabel->setStyleSheet(
        exitCode == 0
            ? "color: #7c8fa6; font-size: 12px; font-weight: 600; letter-spacing: 0.3px;"
            : "color: #f59e0b; font-size: 12px; font-weight: 600; letter-spacing: 0.3px;");
    appendConsole(QString("[%1] Server stopped (exit code %2).")
                  .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                  .arg(exitCode));
}

void McpPanel::onProcessError(QProcess::ProcessError error) {
    setRunning(false);
    QString msg;
    switch (error) {
        case QProcess::FailedToStart: msg = tr("Failed to start (binary not found?)"); break;
        case QProcess::Crashed:       msg = tr("Crashed");                             break;
        case QProcess::Timedout:      msg = tr("Timed out");                           break;
        default:                      msg = tr("Process error (%1)").arg(error);       break;
    }
    m_statusLabel->setText(tr("Error: %1").arg(msg));
    m_statusLabel->setStyleSheet(
        "color: #ef4444; font-size: 12px; font-weight: 600; letter-spacing: 0.3px;");
    appendConsole(tr("[error] %1").arg(msg), true);
}

void McpPanel::onReadyReadStdout() {
    const QString text = QString::fromUtf8(m_process->readAllStandardOutput());
    if (!text.trimmed().isEmpty())
        appendConsole(text.trimmed());
}

void McpPanel::onReadyReadStderr() {
    const QString text = QString::fromUtf8(m_process->readAllStandardError());
    if (!text.trimmed().isEmpty())
        appendConsole(text.trimmed());
}

// ---------------------------------------------------------------------------
void McpPanel::setRunning(bool running) {
    m_startBtn->setEnabled(!running);
    m_stopBtn->setEnabled(running);
    m_tcpRadio->setEnabled(!running);
    m_stdioRadio->setEnabled(!running);
    m_portSpin->setEnabled(!running);
}

void McpPanel::appendConsole(const QString& text, bool isError) {
    // Colour errors in red
    const QString html = isError
        ? QString("<span style='color:#ef4444;'>%1</span>").arg(text.toHtmlEscaped())
        : text.toHtmlEscaped();
    m_console->appendHtml(html);

    // Auto-scroll to bottom
    QScrollBar* sb = m_console->verticalScrollBar();
    sb->setValue(sb->maximum());
}

} // namespace socketspy::gui
