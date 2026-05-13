#pragma once

// ============================================================================
//  HttpServer — thin RAII wrapper over httplib::Server (cpp-httplib)
// ============================================================================
//
// Two responsibilities:
//   - Default-bind to loopback (127.0.0.1) so a misconfigured port doesn't
//     expose a debug service to the network.
//   - Own the accept thread so Start() returns immediately and Stop() joins
//     cleanly.
//
// Routing is delegated entirely to httplib via Raw(). Include httplib.h in
// the translation unit that calls Raw() — we forward-declare here to keep
// this header light.
//
// ============================================================================

#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace httplib { class Server; }

namespace cgnet {

class HttpServer
{
public:
    HttpServer();
    ~HttpServer();

    HttpServer(const HttpServer&)            = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    // Bind on host:port and spawn the accept thread. Returns false if the
    // bind fails (the underlying httplib::Server::bind_to_port reports it
    // synchronously). Default host is loopback only.
    bool Start(int port, const std::string& host = "127.0.0.1");

    // Stop the accept loop and join the worker. Safe to call multiple times.
    void Stop();

    bool IsRunning() const { return m_running.load(); }
    int  Port() const { return m_port; }
    const std::string& Host() const { return m_host; }

    // Install routes on the underlying server before calling Start().
    httplib::Server& Raw() { return *m_server; }

private:
    std::unique_ptr<httplib::Server> m_server;
    std::thread                      m_thread;
    std::atomic<bool>                m_running{ false };
    int                              m_port = 0;
    std::string                      m_host;
};

} // namespace cgnet
