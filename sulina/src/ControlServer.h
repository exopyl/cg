#pragma once

#include <QObject>
#include <QtGlobal>

class QTcpServer;
class QTcpSocket;
class QQuickWindow;
class MeshModel;

/// Tiny line-based control channel on 127.0.0.1:<port>, enabled only when
/// SULINA_CONTROL_PORT is set. Lets a local client drive the viewer for
/// debugging/automation without the GUI. All slots run on the GUI thread
/// (where this object lives), so MeshModel / QQuickWindow are touched
/// directly — no marshaling needed.
///
/// Commands (one per line, reply is one line):
///   load <abs-path>   -> load a mesh        -> "ok loaded vertices=.. faces=.. meshes=.."
///   shot <abs-path>   -> grabWindow to PNG  -> "ok <path> <w>x<h>"
///   state             -> current model info -> "ok source=.. loaded=.. vertices=.."
///   quit              -> quit the app       -> "ok"
class ControlServer : public QObject
{
    Q_OBJECT
public:
    explicit ControlServer(quint16 port, QObject *parent = nullptr);

    void setMeshModel(MeshModel *m) { m_meshModel = m; }
    void setWindow(QQuickWindow *w) { m_window = w; }

    [[nodiscard]] bool isListening() const;

private:
    void onNewConnection();
    void handleCommand(QTcpSocket *sock, const QString &line);

    QTcpServer   *m_server    = nullptr;
    MeshModel    *m_meshModel = nullptr;
    QQuickWindow *m_window    = nullptr;
};
