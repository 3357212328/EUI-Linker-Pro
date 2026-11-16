#include "qt_backend.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QWindow>

int main(int argc, char* argv[]) {
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("FluentLinkerPro"));
    app.setApplicationDisplayName(QStringLiteral("Fluent Linker Pro"));
    app.setOrganizationName(QStringLiteral("FluentLinker"));
    app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/FluentLinker/assets/icon.png")));

    fl::QtLinkBackend backend;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule(QStringLiteral("FluentLinker"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    if (auto* window = qobject_cast<QWindow*>(engine.rootObjects().constFirst())) {
        backend.attachWindow(window);
    }

    return app.exec();
}
