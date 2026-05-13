#include "HttpServer.h"

#include <httplib.h>

namespace cgnet {

HttpServer::HttpServer()
    : m_server(std::make_unique<httplib::Server>())
{}

HttpServer::~HttpServer()
{
    Stop();
}

bool HttpServer::Start(int port, const std::string& host)
{
    if (m_running.exchange(true))
        return false; // already running

    m_port = port;
    m_host = host;

    if (!m_server->bind_to_port(host, port))
    {
        m_running.store(false);
        return false;
    }

    m_thread = std::thread([this]() {
        m_server->listen_after_bind();
    });
    return true;
}

void HttpServer::Stop()
{
    if (!m_running.exchange(false))
        return;
    m_server->stop();
    if (m_thread.joinable())
        m_thread.join();
}

} // namespace cgnet
