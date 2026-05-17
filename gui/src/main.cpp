#include <QApplication>
#include <QFile>
#include <QLocale>
#include <QSettings>
#include <QTimer>
#include <QTranslator>
#include "main_window.h"
#include "splash_screen.h"

int main(int argc, char* argv[]) {
    bool headless = false;
    int  exitAfterSec = -1;

    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "--headless") {
            headless = true;
        } else if (QString(argv[i]) == "--exit-after" && i + 1 < argc) {
            exitAfterSec = QString(argv[++i]).toInt();
        }
    }

    if (headless)
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QApplication app(argc, argv);
    app.setApplicationName("SocketSpy");
    app.setApplicationVersion(APP_VERSION);
    app.setOrganizationName("SocketSpy");

    // Determine UI language: saved preference first, then system locale
    QSettings settings;
    const QString savedLang = settings.value("language", QString()).toString();

    QTranslator translator;
    if (!savedLang.isEmpty()) {
        // Use the explicitly saved language
        if (translator.load(":/i18n/socketspy_" + savedLang))
            app.installTranslator(&translator);
    } else {
        // Fall back to system locale
        for (const QString& lang : QLocale::system().uiLanguages()) {
            const QString baseName = "socketspy_" + QLocale(lang).name().left(2);
            if (translator.load(":/i18n/" + baseName)) {
                app.installTranslator(&translator);
                break;
            }
        }
    }

    QFile qss(":/theme.qss");
    if (qss.open(QFile::ReadOnly))
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));

    // Show splash screen (skipped in headless / CI mode)
    socketspy::gui::SplashScreen* splash = nullptr;
    if (!headless) {
        splash = new socketspy::gui::SplashScreen();
        splash->show();
        app.processEvents();
    }

    socketspy::gui::MainWindow window;

    if (!headless) {
        window.show();
        if (splash)
            QTimer::singleShot(1500, splash, [splash, &window]() {
                splash->finish(&window);
            });
    }

    if (exitAfterSec > 0)
        QTimer::singleShot(exitAfterSec * 1000, &app, &QApplication::quit);

    return app.exec();
}
