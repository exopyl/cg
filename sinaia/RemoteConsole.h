#pragma once

// ============================================================================
//  RemoteConsole — TCP debug console for sinaia
// ============================================================================
//
// Singleton TCP listener bound to 127.0.0.1:<port>. Accepts one client at a
// time, parses line-based text commands and replies with text. Used to drive
// sinaia from external tools (Claude/Codex/Gemini agents, scripts, telnet)
// for visual debugging — query the active VMeshes, dump a face, take a
// screenshot, flip a mesh's winding without recompiling.
//
// Lifetime: Start() spawns a worker thread that owns the listener socket.
// Stop() closes the socket (breaking accept()) and joins the thread.
//
// Threading: the worker reads VMeshes data directly (assumes stable state
// once a file is loaded — no lock). Operations that touch OpenGL or mutate
// geometry are marshaled to the main thread via wxTheApp->CallAfter and
// awaited through std::promise.
//
// Security: bind is explicitly INADDR_LOOPBACK — no exposure outside the
// machine. No authentication; anyone with access to the local loopback can
// connect (consistent with the local-debug-only use case).
//
// ============================================================================

#include <atomic>
#include <cstdint>
#include <thread>

class MyFrame;

class RemoteConsole
{
public:
    static RemoteConsole& Get();

    // Spawn the worker thread bound on 127.0.0.1:port. No-op if already running.
    void Start(unsigned short port, MyFrame* frame);

    // Close the listener and join the worker. Safe to call multiple times.
    void Stop();

    bool IsRunning() const { return m_running.load(); }

private:
    RemoteConsole() = default;
    ~RemoteConsole();
    RemoteConsole(const RemoteConsole&)            = delete;
    RemoteConsole& operator=(const RemoteConsole&) = delete;

    void Loop();

    std::atomic<bool>     m_running{ false };
    std::thread           m_thread;
    unsigned short        m_port  = 0;
    MyFrame*              m_frame = nullptr;
    std::atomic<intptr_t> m_listenerSocket{ 0 }; // SOCKET stored as intptr_t
};
