#include "scripting_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QFile>
#include <QFont>
#include <QMetaObject>
#include <thread>

namespace socketspy::gui {

ScriptingPanel::ScriptingPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

ScriptingPanel::~ScriptingPanel() = default;

void ScriptingPanel::setupUi()
{
    // Monospace font for editor and console
    QFont monoFont("Monospace");
    monoFont.setStyleHint(QFont::TypeWriter);
    monoFont.setPointSize(10);

    // Editor
    m_editor = new QPlainTextEdit(this);
    m_editor->setFont(monoFont);
    m_editor->setPlaceholderText("-- Write your Lua script here\nlog.print(\"Hello, CAN!\")\n");

    // Console (read-only)
    m_console = new QPlainTextEdit(this);
    m_console->setFont(monoFont);
    m_console->setReadOnly(true);
    m_console->setPlaceholderText("Script output will appear here\xe2\x80\xa6");

    // Buttons
    m_openBtn = new QPushButton("Open\xe2\x80\xa6", this);
    m_openBtn->setProperty("secondary", true);
    m_runBtn  = new QPushButton("Run", this);

    // Button bar
    auto* btnLayout = new QHBoxLayout;
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->addWidget(m_openBtn);
    btnLayout->addWidget(m_runBtn);
    btnLayout->addStretch();

    // Splitter: editor on top, console on bottom
    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(m_editor);
    splitter->addWidget(m_console);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    // Main layout
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);
    mainLayout->addLayout(btnLayout);
    mainLayout->addWidget(splitter);

    connect(m_openBtn, &QPushButton::clicked, this, &ScriptingPanel::onOpen);
    connect(m_runBtn,  &QPushButton::clicked, this, [this]() {
        if (m_running) onStop();
        else           onRun();
    });
}

void ScriptingPanel::setRunning(bool running)
{
    m_running = running;
    m_runBtn->setText(running ? "Stop" : "Run");
    m_openBtn->setEnabled(!running);
}

void ScriptingPanel::onOpen()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Open Lua Script",
        {},
        "Lua scripts (*.lua);;All files (*)"
    );
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    m_editor->setPlainText(QString::fromUtf8(file.readAll()));
}

void ScriptingPanel::onRun()
{
    m_console->clear();
    setRunning(true);

    const std::string code = m_editor->toPlainText().toStdString();

    // Build engine with a print callback that posts to the GUI thread
    socketspy::scripting::LuaEngine::Config cfg;
    cfg.watchdog_ms      = 0;         // no time limit
    cfg.max_output_bytes = 1u << 20u; // 1 MiB
    cfg.on_print = [this](std::string_view text) {
        // Called from the Lua worker thread — must not touch Qt widgets directly
        const QString line = QString::fromUtf8(text.data(), static_cast<int>(text.size()));
        QMetaObject::invokeMethod(m_console, [this, line]() {
            m_console->appendPlainText(line);
        }, Qt::QueuedConnection);
    };

    m_engine = std::make_unique<socketspy::scripting::LuaEngine>(std::move(cfg));

    // Run in a background thread; report back to the GUI thread when done
    std::thread([this, code]() {
        const auto result = m_engine->run(code);

        QMetaObject::invokeMethod(this, [this, result]() {
            if (!result.error_message.empty()) {
                m_console->appendPlainText(
                    QString("[Error] %1").arg(QString::fromStdString(result.error_message))
                );
            }
            m_console->appendPlainText(
                QString("[Done] elapsed: %1 ms  |  success: %2")
                    .arg(result.elapsed_ms)
                    .arg(result.success ? "true" : "false")
            );
            setRunning(false);
            m_engine.reset();
        }, Qt::QueuedConnection);
    }).detach();
}

void ScriptingPanel::onStop()
{
    if (m_engine) m_engine->abort();
}

} // namespace socketspy::gui
