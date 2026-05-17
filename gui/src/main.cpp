#include <QApplication>
#include <QFile>
#include <QLocale>
#include <QTimer>
#include <QTranslator>
#include "main_window.h"

int main(int argc, char* argv[]) {
    // Parse --headless and --exit-after <seconds> before creating QApplication
    // so that platform plugins can be suppressed when running headless.
    bool headless = false;
    int  exitAfterSec = -1;

    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "--headless") {
            headless = true;
        } else if (QString(argv[i]) == "--exit-after" && i + 1 < argc) {
            exitAfterSec = QString(argv[++i]).toInt();
        }
    }

    // In headless mode use the offscreen platform so no display is required.
    if (headless) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

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
    if (!headless)
        window.show();

    // Schedule a clean exit when --exit-after N is requested (e.g. CI smoke test).
    if (exitAfterSec > 0)
        QTimer::singleShot(exitAfterSec * 1000, &app, &QApplication::quit);

    return app.exec();
}
