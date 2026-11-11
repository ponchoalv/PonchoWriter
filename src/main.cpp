#include <QApplication>
#include <QFontDatabase>
#include <QIcon>
#include <QFileInfo>
#include <QQuickStyle>
#include <QTimer>
#include <qqml.h>
#include <memory>
#include <vector>

#include "cli.h"
#include "fileopenrequests.h"
#include "instancebroker.h"
#include "livepreview.h"
#include "windowmanager.h"

#include "profile.h"

#define PONCHOWRITER_VERSION "0.7.2"

int main(int argc, char *argv[]) {
    if (const auto code = Cli::handleArguments(argc, argv, QStringLiteral(PONCHOWRITER_VERSION)))
        return *code;
    if (qgetenv("QT_QPA_PLATFORMTHEME") == "gtk3")
        qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
    QApplication app(argc, argv);
    Profile::configureIdentity();
    app.setApplicationDisplayName(QStringLiteral("PonchoWriter"));
    app.setApplicationVersion(QStringLiteral(PONCHOWRITER_VERSION));
    app.setDesktopFileName(QStringLiteral("ponchowriter"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("ponchowriter"),
                                      QIcon(QStringLiteral(":/icons/ponchowriter.png"))));
    app.setQuitOnLastWindowClosed(false);

    FileOpenRequests nativeFiles(&app);
    std::unique_ptr<WindowManager> windows;
    std::vector<std::unique_ptr<InstanceBroker>> brokers;
    QList<QStringList> requests;
    bool ready = false;
    const auto connectBroker = [&](InstanceBroker *broker) {
        QObject::connect(broker, &InstanceBroker::openRequested, &app, [&](const QStringList &files) {
            if (ready) windows->openFiles(files); else requests.append(files);
        }, Qt::QueuedConnection); // ACK transport acceptance before any disk/QML work.
    };
    const auto becomePrimary = [&](const QStringList &initialFiles) {
        if (!Profile::activate()) { qCritical("Could not initialize PonchoWriter preferences; no drafts were changed."); app.exit(1); return; }
        nativeFiles.activate(false);
        for (const char *weight : {"Regular", "Italic", "Bold", "BoldItalic"})
            QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/IBMPlexMono-%1.ttf").arg(QLatin1String(weight)));
        QQuickStyle::setStyle(QStringLiteral("Material"));
        qmlRegisterType<LivePreview>("PonchoWriter", 1, 0, "LivePreview");
        windows = std::make_unique<WindowManager>();
        QObject::connect(windows.get(), &WindowManager::noWindowsRemaining, &app, &QCoreApplication::quit);
        QObject::connect(windows.get(), &WindowManager::errorOccurred, &app,
                         [](const QString &message) { qCritical().noquote() << message; });
        windows->start(initialFiles);
        ready = true;
        for (const auto &request : requests) windows->openFiles(request);
        requests.clear();
        if (windows->windows().isEmpty()) app.exit(1);
    };
    const auto deliver = [&](const QStringList &files) {
        if (ready) { windows->openFiles(files); return; }
        auto broker = std::make_unique<InstanceBroker>();
        const auto result = broker->start(files);
        if (result == InstanceBroker::Primary) {
            // The initial request is handled explicitly; remote requests are
            // connected before returning to the event loop.
            connectBroker(broker.get());
            brokers.push_back(std::move(broker)); // Retain ownership until all windows close.
            becomePrimary(files);
        } else if (result == InstanceBroker::Forwarded) {
            // Do not drop native events delivered after the first forwarded CLI
            // request. Every subsequent batch is forwarded and resets this wait.
            nativeFiles.activate(true);
        } else {
            qCritical().noquote() << broker->errorString();
            app.exit(1); // Fail closed: never open a competing editor on IPC failure.
        }
    };
    QObject::connect(&nativeFiles, &FileOpenRequests::filesRequested, &app, deliver);
    QObject::connect(&nativeFiles, &FileOpenRequests::forwardingIdle, &app, [&] {
        if (!ready) app.quit();
    });
    QTimer::singleShot(0, &app, [&] {
        QStringList files = app.arguments().mid(1);
        if (!files.isEmpty() && files.first() == QStringLiteral("--")) files.removeFirst();
        for (auto &file : files) file = QFileInfo(file).absoluteFilePath();
        files.append(nativeFiles.takePending());
        deliver(files);
    });
    return app.exec();
}
