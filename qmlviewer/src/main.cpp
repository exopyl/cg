#include "WindowController.h"

#include <QDir>
#include <QFileSystemModel>
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
    // Force Vulkan as the RHI backend for QtQuick rendering.
    // Must be called before QGuiApplication is constructed.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    // Filesystem model used by the left-panel folder TreeView. We filter
    // to directories only — files don't belong in the tree, the bottom
    // strip handles mesh-file selection. setRootPath("") so the model is
    // ready to serve queries for any path on demand.
    auto *fsModel = new QFileSystemModel(&app);
    fsModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Drives);
    fsModel->setReadOnly(true);
    fsModel->setRootPath(QString());

    engine.rootContext()->setContextProperty(
        QStringLiteral("fsModel"), fsModel);

    engine.rootContext()->setContextProperty(
        QStringLiteral("dataDir"),
        QString::fromUtf8(QMLVIEWER_DATA_DIR));

    // Frameless-window helper for the custom title bar (native move/resize).
    auto *winCtrl = new WindowController(&app);
    engine.rootContext()->setContextProperty(
        QStringLiteral("winCtrl"), winCtrl);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("QmlViewer", "Main");

    // Hand the loaded window to the controller so QML can drive native
    // move/resize through it.
    if (!engine.rootObjects().isEmpty()) {
        if (auto *win = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst()))
            winCtrl->setWindow(win);
    }

    return app.exec();
}
