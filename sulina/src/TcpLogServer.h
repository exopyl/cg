#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QtGlobal>

class QTcpServer;
class QTcpSocket;

/// Broadcasts log lines to TCP clients connected on 127.0.0.1:<port>.
///
/// Intended as the target of a cgre2::Logger sink so a remote client can
/// watch the renderer/viewer logs live (the GUI app's stdout isn't visible
/// when launched normally). `postLine()` is thread-safe — the cgre2 logger
/// may fire from the render thread, so it marshals to this object's thread
/// before touching the (non-thread-safe) sockets.
class TcpLogServer : public QObject
{
    Q_OBJECT
public:
    explicit TcpLogServer(quint16 port, QObject *parent = nullptr);
    ~TcpLogServer() override;

    [[nodiscard]] bool isListening() const;

    /// Queue a line for broadcast. Safe to call from any thread.
    void postLine(const QString &line);

private:
    void writeLine(const QString &line);   // runs on this->thread()
    void onNewConnection();

    QTcpServer        *m_server = nullptr;
    QList<QTcpSocket *> m_clients;
};
