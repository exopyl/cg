#pragma once

// ============================================================================
//  OverlappedSocket — async WSASend with WSAOVERLAPPED + event
// ============================================================================
//
// Wraps the WSASend / WSACreateEvent / WSAWaitForMultipleEvents /
// WSAGetOverlappedResult dance for non-blocking sends on a connected socket.
//
// Usage:
//   cgnet::OverlappedSend op(socket);
//   if (!op.Issue(buf, n)) { ... };       // returns immediately (or fails)
//   ... do other work ...
//   int bytes = op.Wait();                // blocks until the send completes
//
// One Issue / one Wait per instance — re-issuing without waiting is UB.
// The caller retains ownership of the underlying SOCKET (we do not close it).
//
// ============================================================================

#include <cstddef>
#include <cstdint>

namespace cgnet {

class OverlappedSend
{
public:
    // `socket` must be a connected SOCKET (stored as uintptr_t to keep this
    // header free of <winsock2.h>). The caller retains ownership.
    explicit OverlappedSend(std::uintptr_t socket);
    ~OverlappedSend();

    OverlappedSend(const OverlappedSend&)            = delete;
    OverlappedSend& operator=(const OverlappedSend&) = delete;

    // Submit an async WSASend. Returns true if the request was either
    // completed synchronously or is pending. Returns false if WSASend
    // failed outright — caller should consult WsaErrorString().
    bool Issue(const void* data, std::size_t bytes);

    // Block until the pending Issue() completes. Returns bytes transferred,
    // or -1 on error (consult WsaErrorString()). Safe to call multiple times
    // after a single Issue() (subsequent calls return the cached result).
    int Wait();

    // True once Wait() has returned the final result.
    bool IsComplete() const { return m_complete; }

private:
    std::uintptr_t m_socket    = 0;
    void*          m_event     = nullptr; // WSAEVENT
    void*          m_overlapped = nullptr; // WSAOVERLAPPED*
    int            m_result    = -1;
    bool           m_pending   = false;
    bool           m_complete  = false;
};

} // namespace cgnet
