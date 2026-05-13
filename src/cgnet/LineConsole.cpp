#include "LineConsole.h"

#define _WINSOCKAPI_ // avoid winsock.h pulled from windows.h
#include <winsock2.h>
#include <ws2tcpip.h>

namespace cgnet {

namespace {

bool sendAll(SOCKET s, const std::string& data)
{
    size_t sent = 0;
    while (sent < data.size())
    {
        int n = send(s, data.data() + sent, static_cast<int>(data.size() - sent), 0);
        if (n <= 0)
            return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

void handleClient(SOCKET client,
                  const std::string& banner,
                  const LineHandler& handler,
                  const std::atomic<bool>& running)
{
    if (!banner.empty())
        sendAll(client, banner);

    std::string buffer;
    char chunk[1024];
    while (running.load())
    {
        const int n = recv(client, chunk, sizeof(chunk), 0);
        if (n <= 0) break;
        buffer.append(chunk, chunk + n);

        for (;;)
        {
            const size_t pos = buffer.find('\n');
            if (pos == std::string::npos) break;
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            Reply reply = handler(line);
            if (!sendAll(client, reply.text)) return;
            if (reply.close) return;
        }
    }
}

} // namespace

LineConsole::~LineConsole()
{
    Stop();
}

void LineConsole::Start(unsigned short port, std::string banner, LineHandler handler)
{
    if (m_running.exchange(true))
        return; // already running
    m_port    = port;
    m_banner  = std::move(banner);
    m_handler = std::move(handler);
    m_thread  = std::thread([this]() { Loop(); });
}

void LineConsole::Stop()
{
    if (!m_running.exchange(false))
        return;
    const intptr_t s = m_listenerSocket.exchange(0);
    if (s != 0)
        closesocket(static_cast<SOCKET>(s));
    if (m_thread.joinable())
        m_thread.join();
}

void LineConsole::Loop()
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        m_running.store(false);
        return;
    }

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET)
    {
        WSACleanup();
        m_running.store(false);
        return;
    }

    int reuse = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(m_port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1 only

    if (::bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        closesocket(listener);
        WSACleanup();
        m_running.store(false);
        return;
    }

    if (listen(listener, 1) == SOCKET_ERROR)
    {
        closesocket(listener);
        WSACleanup();
        m_running.store(false);
        return;
    }

    m_listenerSocket.store(static_cast<intptr_t>(listener));

    while (m_running.load())
    {
        sockaddr_in cli{};
        int cliLen = sizeof(cli);
        SOCKET client = accept(listener, reinterpret_cast<sockaddr*>(&cli), &cliLen);
        if (client == INVALID_SOCKET)
            break; // listener closed by Stop() or genuine error

        handleClient(client, m_banner, m_handler, m_running);
        closesocket(client);
    }

    if (m_listenerSocket.load() != 0)
        closesocket(static_cast<SOCKET>(m_listenerSocket.exchange(0)));
    WSACleanup();
}

} // namespace cgnet
