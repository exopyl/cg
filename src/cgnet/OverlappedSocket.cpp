#include "OverlappedSocket.h"
#include "WsaError.h"

#define _WINSOCKAPI_
#include <winsock2.h>

#include <cstdio>
#include <cstring>

namespace cgnet {

OverlappedSend::OverlappedSend(std::uintptr_t socket)
    : m_socket(socket)
{
    WSAEVENT ev = ::WSACreateEvent();
    if (ev == WSA_INVALID_EVENT)
    {
        LogLastWsaError("OverlappedSend: WSACreateEvent");
        return;
    }
    m_event = ev;

    auto* over = new WSAOVERLAPPED;
    std::memset(over, 0, sizeof(*over));
    over->hEvent = ev;
    m_overlapped = over;
}

OverlappedSend::~OverlappedSend()
{
    if (m_pending && !m_complete)
    {
        // Drain the in-flight operation so we don't free m_overlapped while
        // Winsock is still touching it.
        Wait();
    }
    if (m_overlapped)
        delete static_cast<WSAOVERLAPPED*>(m_overlapped);
    if (m_event)
        ::WSACloseEvent(static_cast<WSAEVENT>(m_event));
}

bool OverlappedSend::Issue(const void* data, std::size_t bytes)
{
    if (m_event == nullptr || m_overlapped == nullptr)
        return false;
    if (m_pending)
        return false; // single-shot

    WSABUF wsabuf;
    wsabuf.buf = static_cast<char*>(const_cast<void*>(data));
    wsabuf.len = static_cast<ULONG>(bytes);

    DWORD sent  = 0;
    const int rc = ::WSASend(static_cast<SOCKET>(m_socket),
                             &wsabuf, 1, &sent, 0,
                             static_cast<WSAOVERLAPPED*>(m_overlapped),
                             nullptr);
    if (rc == 0)
    {
        // Completed synchronously
        m_result   = static_cast<int>(sent);
        m_pending  = true;
        m_complete = true;
        return true;
    }

    const int err = ::WSAGetLastError();
    if (err == WSA_IO_PENDING)
    {
        m_pending = true;
        return true;
    }

    LogLastWsaError("OverlappedSend: WSASend");
    return false;
}

int OverlappedSend::Wait()
{
    if (!m_pending) return -1;
    if (m_complete) return m_result;

    const DWORD rc = ::WSAWaitForMultipleEvents(
        1,
        reinterpret_cast<const WSAEVENT*>(&m_event),
        TRUE, WSA_INFINITE, FALSE);
    if (rc == WSA_WAIT_FAILED)
    {
        LogLastWsaError("OverlappedSend: WSAWaitForMultipleEvents");
        m_complete = true;
        m_result   = -1;
        return -1;
    }

    DWORD transferred = 0;
    DWORD flags = 0;
    const BOOL ok = ::WSAGetOverlappedResult(
        static_cast<SOCKET>(m_socket),
        static_cast<WSAOVERLAPPED*>(m_overlapped),
        &transferred, FALSE, &flags);

    m_complete = true;
    if (!ok)
    {
        LogLastWsaError("OverlappedSend: WSAGetOverlappedResult");
        m_result = -1;
    }
    else
    {
        m_result = static_cast<int>(transferred);
    }

    ::WSAResetEvent(static_cast<WSAEVENT>(m_event));
    return m_result;
}

} // namespace cgnet
