#include <QApplication>
#include <QFile>
#include <QLocale>
#include <QTranslator>
#include "main_window.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("SocketSpy");
    app.setApplicationVersion("0.1.0");

    // Load the translation that best matches the system locale.
    // The .qm files are embedded as Qt resources under :/i18n/ by CMake.
    // Supported locales: en (default), fr.
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString& lang : uiLanguages) {
        // Qt resource path: :/i18n/socketspy_<lang>.qm  (e.g. socketspy_fr.qm)
        const QString baseName = "socketspy_" + QLocale(lang).name().left(2);
        if (translator.load(":/i18n/" + baseName)) {
            app.installTranslator(&translator);
            break;
        }
    }

    QFile qss(":/theme.qss");
    if (qss.open(QFile::ReadOnly))
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));

    socketspy::gui::MainWindow window;
    window.show();

    return app.exec();
}
