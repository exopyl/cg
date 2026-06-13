#include "ControlServer.h"
#include "MeshModel.h"
#include "TcpLogServer.h"
#include "WindowController.h"

#include "cgre2/Logger.hpp"

#include <QDir>
#include <QFileSystemModel>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QString>
#include <QStringList>

#include <string_view>

#ifndef SULINA_DATA_DIR
#  define SULINA_DATA_DIR ""
#endif

namespace {

// Resolve a TCP port from, in order: the command line "--flag [PORT]"
// (PORT optional — the flag alone enables the default), else the env var,
// else 0 (= feature disabled). The CLI flag takes precedence over the env.
quint16 resolvePort(const QStringList &args, const QString &flag,
                    const char *envVar, quint16 defaultPort)
{
    const int idx = args.indexOf(flag);
    if (idx >= 0) {
        if (idx + 1 < args.size()) {
            bool ok = false;
            const quint16 p = args.at(idx + 1).toUShort(&ok);
            if (ok && p != 0)
                return p;          // explicit port after the flag
        }
        return defaultPort;        // flag present, no/invalid port → default
    }
    bool ok = false;
    const quint16 p = qEnvironmentVariable(envVar).toUShort(&ok);
    return (ok && p != 0) ? p : 0; // env fallback, else disabled
}

} // namespace

int main(int argc, char *argv[])
{
    // Force Vulkan as the RHI backend for QtQuick rendering.
    // Must be called before QGuiApplication is constructed.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);

    QGuiApplication app(argc, argv);

    // Debug/automation channels — enabled by a CLI flag (optionally with an
    // explicit port) or the matching env var, else off:
    //   --log-port [PORT]      / SULINA_LOG_PORT      (default 7777)
    //   --control-port [PORT]  / SULINA_CONTROL_PORT  (default 7778)
    const QStringList cliArgs = app.arguments();
    const quint16 logPort  = resolvePort(cliArgs, QStringLiteral("--log-port"),
                                         "SULINA_LOG_PORT", 7777);
    const quint16 ctrlPort = resolvePort(cliArgs, QStringLiteral("--control-port"),
                                         "SULINA_CONTROL_PORT", 7778);

    // Optional TCP log stream: mirrors cgre2::Logger output (renderer +
    // Viewer logs) to TCP clients on 127.0.0.1:<logPort> — lets a remote
    // client watch the logs live, since a GUI app's stdout isn't visible
    // when launched normally.
    TcpLogServer *logServer = nullptr;
    {
        if (logPort != 0) {
            logServer = new TcpLogServer(logPort, &app);
            cgre2::Logger::setMinLevel(cgre2::Logger::Level::Debug);
            cgre2::Logger::addSink(
                [logServer](cgre2::Logger::Level level,
                            std::string_view module,
                            std::string_view message) {
                    const char *lvl = "INFO";
                    switch (level) {
                    case cgre2::Logger::Level::Debug: lvl = "DEBUG"; break;
                    case cgre2::Logger::Level::Info:  lvl = "INFO";  break;
                    case cgre2::Logger::Level::Warn:  lvl = "WARN";  break;
                    case cgre2::Logger::Level::Error: lvl = "ERROR"; break;
                    }
                    logServer->postLine(QStringLiteral("[%1][%2] %3").arg(
                        QString::fromLatin1(lvl),
                        QString::fromUtf8(module.data(),
                                          static_cast<qsizetype>(module.size())),
                        QString::fromUtf8(message.data(),
                                          static_cast<qsizetype>(message.size()))));
                });
        }
    }

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
        QString::fromUtf8(SULINA_DATA_DIR));

    // Frameless-window helper for the custom title bar (native move/resize).
    auto *winCtrl = new WindowController(&app);
    engine.rootContext()->setContextProperty(
        QStringLiteral("winCtrl"), winCtrl);

    // Optional control channel — lets a local client drive
    // load/screenshot/state for debugging without the GUI.
    ControlServer *controlServer = nullptr;
    if (ctrlPort != 0)
        controlServer = new ControlServer(ctrlPort, &app);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("Sulina", "Main");

    // Hand the loaded window to the controller so QML can drive native
    // move/resize through it; also wire the control server to the window
    // and the QML-instantiated MeshModel.
    if (!engine.rootObjects().isEmpty()) {
        QObject *root = engine.rootObjects().constFirst();
        if (auto *win = qobject_cast<QQuickWindow*>(root)) {
            winCtrl->setWindow(win);
            if (controlServer)
                controlServer->setWindow(win);
        }
        if (controlServer) {
            if (auto *mm = root->findChild<MeshModel*>())
                controlServer->setMeshModel(mm);
        }
    }

    const int rc = app.exec();

    // The log sink captured `logServer` (owned by `app`); drop it before
    // app teardown so late cgre2 logs (GPU resource destruction) don't
    // dereference a soon-to-be-freed pointer.
    cgre2::Logger::clearSinks();

    return rc;
}
