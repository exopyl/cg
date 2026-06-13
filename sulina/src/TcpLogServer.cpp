#include "TcpLogServer.h"

#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

TcpLogServer::TcpLogServer(quint16 port, QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection,
            this, &TcpLogServer::onNewConnection);

    if (!m_server->listen(QHostAddress::LocalHost, port)) {
        qWarning("TcpLogServer: cannot listen on 127.0.0.1:%u: %s",
                 port, qPrintable(m_server->errorString()));
    } else {
        qInfo("TcpLogServer: listening on 127.0.0.1:%u", port);
    }
}

TcpLogServer::~TcpLogServer() = default;

bool TcpLogServer::isListening() const
{
    return m_server->isListening();
}

void TcpLogServer::postLine(const QString &line)
{
    // May be called from the render thread; hop to this object's thread
    // before touching sockets.
    QMetaObject::invokeMethod(
        this, [this, line]() { writeLine(line); }, Qt::QueuedConnection);
}

void TcpLogServer::onNewConnection()
{
    while (QTcpSocket *sock = m_server->nextPendingConnection()) {
        m_clients.append(sock);
        connect(sock, &QTcpSocket::disconnected, this, [this, sock]() {
            m_clients.removeAll(sock);
            sock->deleteLater();
        });
    }
}

void TcpLogServer::writeLine(const QString &line)
{
    const QByteArray bytes = (line + QLatin1Char('\n')).toUtf8();
    for (QTcpSocket *sock : std::as_const(m_clients)) {
        if (sock->state() == QAbstractSocket::ConnectedState)
            sock->write(bytes);
    }
}
