#pragma once
#include <QWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <memory>
#include <thread>
#include "lua_engine.h"
#include "cancore.h"

namespace socketspy::gui {

class ScriptingPanel : public QWidget {
    Q_OBJECT

public:
    explicit ScriptingPanel(QWidget* parent = nullptr);
    ~ScriptingPanel() override;

public slots:
    void onFrameReceived(socketspy::core::CanFrame frame);

private slots:
    void onOpen();
    void onRun();
    void onStop();

private:
    void setupUi();
    void setRunning(bool running);

    QPlainTextEdit*  m_editor{nullptr};
    QPushButton*     m_runBtn{nullptr};
    QPushButton*     m_openBtn{nullptr};
    QPlainTextEdit*  m_console{nullptr};

    std::unique_ptr<socketspy::scripting::LuaEngine> m_engine;
    std::thread m_workerThread;
    bool m_running{false};
};

} // namespace socketspy::gui
