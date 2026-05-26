#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>

#ifndef QMLVIEWER_DATA_DIR
#  define QMLVIEWER_DATA_DIR ""
#endif

int main(int argc, char *argv[])
{
    // Force Vulkan as the RHI backend for QtQuick + Quick3D rendering.
    // Must be called before QGuiApplication is constructed.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty(
        QStringLiteral("dataDir"),
        QString::fromUtf8(QMLVIEWER_DATA_DIR));

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("QmlViewer", "Main");

    return app.exec();
}
