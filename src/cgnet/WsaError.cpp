#include "WsaError.h"

#define _WINSOCKAPI_
#include <winsock2.h>
#include <windows.h>

#include <cstdio>
#include <unordered_map>

namespace cgnet {

namespace {

struct CodeInfo
{
    const char* symbol;
    const char* gloss;
};

const std::unordered_map<int, CodeInfo>& errorTable()
{
    static const std::unordered_map<int, CodeInfo> table = {
        { WSANOTINITIALISED, { "WSANOTINITIALISED",
            "WSAStartup must be called before any other Winsock function." }},
        { WSAENETDOWN,       { "WSAENETDOWN",
            "The network subsystem has failed." }},
        { WSAEACCES,         { "WSAEACCES",
            "Permission denied (privileged port or firewall rule?)." }},
        { WSAEADDRINUSE,     { "WSAEADDRINUSE",
            "Address already in use — another socket is bound; consider SO_REUSEADDR." }},
        { WSAEADDRNOTAVAIL,  { "WSAEADDRNOTAVAIL",
            "Requested address is not valid on this machine." }},
        { WSAEFAULT,         { "WSAEFAULT",
            "Bad pointer or invalid sockaddr length." }},
        { WSAEINPROGRESS,    { "WSAEINPROGRESS",
            "A blocking Winsock 1.1 call is already in progress." }},
        { WSAEINVAL,         { "WSAEINVAL",
            "Invalid argument (often: socket already bound, or listen on unbound socket)." }},
        { WSAEMFILE,         { "WSAEMFILE",
            "Too many open sockets." }},
        { WSAENOBUFS,        { "WSAENOBUFS",
            "No buffer space available (resource exhaustion)." }},
        { WSAENOTSOCK,       { "WSAENOTSOCK",
            "Descriptor is not a socket (use-after-close?)." }},
        { WSAENETRESET,      { "WSAENETRESET",
            "Connection timed out (typically with SO_KEEPALIVE)." }},
        { WSAENOPROTOOPT,    { "WSAENOPROTOOPT",
            "Option unknown or unsupported for this socket type." }},
        { WSAENOTCONN,       { "WSAENOTCONN",
            "Socket is not connected (recv/send before connect/accept?)." }},
        { WSAECONNRESET,     { "WSAECONNRESET",
            "Peer reset the connection (typically: process exited)." }},
        { WSAECONNABORTED,   { "WSAECONNABORTED",
            "Connection aborted locally (timeout or protocol error)." }},
        { WSAECONNREFUSED,   { "WSAECONNREFUSED",
            "No listener on the target port." }},
        { WSAETIMEDOUT,      { "WSAETIMEDOUT",
            "Operation timed out (peer unreachable?)." }},
        { WSAEHOSTUNREACH,   { "WSAEHOSTUNREACH",
            "No route to host." }},
        { WSAEINTR,          { "WSAEINTR",
            "Blocking call cancelled (e.g. by WSACancelBlockingCall)." }},
        { WSAEWOULDBLOCK,    { "WSAEWOULDBLOCK",
            "Non-blocking socket would block; retry later." }},
        { WSAESHUTDOWN,      { "WSAESHUTDOWN",
            "Operation on a socket that has been shut down." }},
        { WSAHOST_NOT_FOUND, { "WSAHOST_NOT_FOUND",
            "DNS resolution failed: host not found." }},
        { WSATRY_AGAIN,      { "WSATRY_AGAIN",
            "DNS server temporarily unavailable; retry." }},
    };
    return table;
}

std::string systemMessage(int code)
{
    char* buf = nullptr;
    const DWORD len = ::FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        static_cast<DWORD>(code),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&buf),
        0,
        nullptr);

    if (len == 0 || buf == nullptr)
        return {};

    std::string msg(buf, len);
    ::LocalFree(buf);

    while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r' || msg.back() == '.' || msg.back() == ' '))
        msg.pop_back();
    return msg;
}

} // namespace

std::string WsaErrorString(int code)
{
    if (code == kLastWsaError)
        code = ::WSAGetLastError();

    if (code == 0)
        return "no error";

    const auto& table = errorTable();
    const auto it = table.find(code);
    const char* symbol = (it != table.end()) ? it->second.symbol : "WSA";
    const char* gloss  = (it != table.end()) ? it->second.gloss  : "";

    char header[64];
    std::snprintf(header, sizeof(header), "[%s (%d)] ", symbol, code);

    std::string out = header;
    const std::string sys = systemMessage(code);
    if (!sys.empty())
        out += sys;
    else
        out += "no system description available";
    if (gloss[0] != '\0')
    {
        out += " \xE2\x80\x94 ";
        out += gloss;
    }
    return out;
}

void LogLastWsaError(const char* context)
{
    const int code = ::WSAGetLastError();
    if (code == 0)
        return;
    std::fprintf(stderr, "%s: %s\n",
                 context ? context : "(no context)",
                 WsaErrorString(code).c_str());
}

} // namespace cgnet
