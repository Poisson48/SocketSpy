#include <QApplication>
#include <QFile>
#include "main_window.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("SocketSpy");
    app.setApplicationVersion("0.1.0");

    QFile qss(":/theme.qss");
    if (qss.open(QFile::ReadOnly))
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));

    socketspy::gui::MainWindow window;
    window.show();

    return app.exec();
}
