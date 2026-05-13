#pragma once

// ============================================================================
//  RemoteConsole — sinaia debug commands over a cgnet::LineConsole
// ============================================================================
//
// Wraps a cgnet::LineConsole (the TCP loopback plumbing) with the set of
// sinaia-specific commands (info, mesh, face, vertex, material, flip,
// screenshot, help, quit). Used to drive sinaia from external tools
// (Claude/Codex/Gemini agents, scripts, telnet) for visual debugging.
//
// Threading: commands read VMeshes data directly from the worker thread
// (assumes stable state once a file is loaded — no lock). Operations that
// touch OpenGL or mutate geometry are marshaled to the main thread via
// wxTheApp->CallAfter and awaited through std::promise.
//
// ============================================================================

#include "../src/cgnet/LineConsole.h"

class MyFrame;

class RemoteConsole
{
public:
    static RemoteConsole& Get();

    // Bind on 127.0.0.1:port. No-op if already running.
    void Start(unsigned short port, MyFrame* frame);

    // Close the listener and join the worker. Safe to call multiple times.
    void Stop();

    bool IsRunning() const { return m_console.IsRunning(); }

private:
    RemoteConsole() = default;
    RemoteConsole(const RemoteConsole&)            = delete;
    RemoteConsole& operator=(const RemoteConsole&) = delete;

    cgnet::LineConsole m_console;
    MyFrame*           m_frame = nullptr;
};
