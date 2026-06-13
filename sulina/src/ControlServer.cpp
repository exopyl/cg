#include "ControlServer.h"

#include "MeshModel.h"

#include <QCoreApplication>
#include <QHostAddress>
#include <QImage>
#include <QQuickWindow>
#include <QTcpServer>
#include <QTcpSocket>

ControlServer::ControlServer(quint16 port, QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection,
            this, &ControlServer::onNewConnection);

    if (!m_server->listen(QHostAddress::LocalHost, port))
        qWarning("ControlServer: cannot listen on 127.0.0.1:%u: %s",
                 port, qPrintable(m_server->errorString()));
    else
        qInfo("ControlServer: listening on 127.0.0.1:%u", port);
}

bool ControlServer::isListening() const
{
    return m_server->isListening();
}

void ControlServer::onNewConnection()
{
    while (QTcpSocket *sock = m_server->nextPendingConnection()) {
        connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
            while (sock->canReadLine()) {
                const QString line = QString::fromUtf8(sock->readLine()).trimmed();
                if (!line.isEmpty())
                    handleCommand(sock, line);
            }
        });
        connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
    }
}

void ControlServer::handleCommand(QTcpSocket *sock, const QString &line)
{
    const int sp = line.indexOf(QLatin1Char(' '));
    const QString cmd = (sp < 0 ? line : line.left(sp)).toLower();
    const QString arg = (sp < 0 ? QString() : line.mid(sp + 1).trimmed());

    const auto reply = [sock](const QString &s) {
        sock->write((s + QLatin1Char('\n')).toUtf8());
        sock->flush();
    };

    if (cmd == QLatin1String("load")) {
        if (!m_meshModel) { reply(QStringLiteral("err no meshModel")); return; }
        if (m_meshModel->load(arg))
            reply(QStringLiteral("ok loaded vertices=%1 faces=%2 meshes=%3")
                      .arg(m_meshModel->vertexCount())
                      .arg(m_meshModel->faceCount())
                      .arg(m_meshModel->meshCount()));
        else
            reply(QStringLiteral("err load failed: ") + m_meshModel->lastError());

    } else if (cmd == QLatin1String("shot") || cmd == QLatin1String("screenshot")) {
        if (!m_window) { reply(QStringLiteral("err no window")); return; }
        const QImage img = m_window->grabWindow();
        if (img.isNull()) { reply(QStringLiteral("err grabWindow returned null")); return; }
        if (img.save(arg))
            reply(QStringLiteral("ok %1 %2x%3").arg(arg).arg(img.width()).arg(img.height()));
        else
            reply(QStringLiteral("err save failed: ") + arg);

    } else if (cmd == QLatin1String("state")) {
        if (!m_meshModel) { reply(QStringLiteral("err no meshModel")); return; }
        reply(QStringLiteral("ok source=%1 loaded=%2 vertices=%3 faces=%4 meshes=%5")
                  .arg(m_meshModel->source())
                  .arg(m_meshModel->loaded() ? 1 : 0)
                  .arg(m_meshModel->vertexCount())
                  .arg(m_meshModel->faceCount())
                  .arg(m_meshModel->meshCount()));

    } else if (cmd == QLatin1String("quit")) {
        reply(QStringLiteral("ok"));
        QCoreApplication::quit();

    } else {
        reply(QStringLiteral("err unknown command: ") + cmd);
    }
}
