#pragma once

// ============================================================================
//  LineConsole — minimal TCP loopback line-based debug console (Windows)
// ============================================================================
//
// Bind 127.0.0.1:<port>. One client at a time. Each '\n'-terminated line
// (CRLF tolerated) is handed to a user-provided handler that returns the
// text to send back and an optional "close" flag.
//
// Reusable across host applications: the handler captures the app context
// it needs (frame pointers, scene objects, marshaling to the main thread,
// etc.) — LineConsole itself knows nothing about the app.
//
// Lifetime: Start() spawns a worker thread that owns the listener socket.
// Stop() closes the socket (breaking accept()) and joins the thread.
// Destructor calls Stop(), so an instance can be embedded by value.
//
// Security: bind is explicitly INADDR_LOOPBACK — no exposure outside the
// machine. No authentication.
//
// ============================================================================

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

namespace cgnet {

struct Reply
{
    std::string text;
    bool        close = false; // if true, drop the connection after sending `text`
};

using LineHandler = std::function<Reply(const std::string& line)>;

class LineConsole
{
public:
    LineConsole() = default;
    ~LineConsole();

    LineConsole(const LineConsole&)            = delete;
    LineConsole& operator=(const LineConsole&) = delete;

    // Start the worker. No-op if already running.
    // `banner` is sent verbatim on each new connection (may be empty).
    void Start(unsigned short port, std::string banner, LineHandler handler);

    // Close the listener and join the worker. Safe to call multiple times.
    void Stop();

    bool IsRunning() const { return m_running.load(); }

private:
    void Loop();

    std::atomic<bool>     m_running{ false };
    std::thread           m_thread;
    unsigned short        m_port = 0;
    std::string           m_banner;
    LineHandler           m_handler;
    std::atomic<intptr_t> m_listenerSocket{ 0 }; // SOCKET stored as intptr_t
};

} // namespace cgnet
