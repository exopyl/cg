// ============================================================================
//  cgnet_demo — exercise every module of src/cgnet
// ============================================================================

#include "cgnet/HttpServer.h"
#include "cgnet/NetworkInfo.h"
#include "cgnet/OverlappedSocket.h"
#include "cgnet/WsaError.h"

#include <httplib.h>

#define _WINSOCKAPI_
#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace {

// ----- helpers --------------------------------------------------------------

void section(const char* title)
{
    std::printf("\n===== %s =====\n", title);
}

std::string join(const std::vector<std::string>& v, const char* sep = ", ")
{
    std::string out;
    for (size_t i = 0; i < v.size(); ++i)
    {
        if (i) out += sep;
        out += v[i];
    }
    return out;
}

// ----- demo: NetworkInfo ----------------------------------------------------

void demoNetworkInfo()
{
    section("NetworkInfo");

    const auto adapters = cgnet::ListAdapters();
    std::printf("%zu adapters\n", adapters.size());
    for (const auto& a : adapters)
    {
        std::printf("  - %s [%s]%s\n",
                    a.description.c_str(),
                    a.is_up ? "up" : "down",
                    a.dhcp_enabled ? " (DHCP)" : "");
        if (!a.mac.empty())             std::printf("      MAC      : %s\n", a.mac.c_str());
        if (!a.ipv4.empty())            std::printf("      IPv4     : %s\n", join(a.ipv4).c_str());
        if (!a.ipv6.empty())            std::printf("      IPv6     : %s\n", join(a.ipv6).c_str());
        if (!a.gateways.empty())        std::printf("      Gateways : %s\n", join(a.gateways).c_str());
    }

    if (auto g = cgnet::GetDefaultGateway())
        std::printf("Default gateway: %s\n", g->c_str());
    else
        std::printf("Default gateway: <none>\n");

    const auto shares = cgnet::EnumerateSmbShares();
    std::printf("%zu SMB resources visible\n", shares.size());
    for (const auto& s : shares)
        std::printf("  %s\n", s.c_str());
}

// ----- demo: WsaError -------------------------------------------------------

void demoWsaError()
{
    section("WsaError");

    WSADATA wsa{};
    if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        std::printf("WSAStartup failed\n");
        return;
    }

    // Deliberately provoke WSAEADDRINUSE: bind twice on the same port
    // without SO_REUSEADDR on the second socket.
    const SOCKET s1 = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = ::htons(58031);
    addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);

    if (::bind(s1, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        std::printf("first bind failed: %s\n", cgnet::WsaErrorString().c_str());
        ::closesocket(s1);
        ::WSACleanup();
        return;
    }
    ::listen(s1, 1);

    const SOCKET s2 = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (::bind(s2, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
        std::printf("second bind (expected to fail): %s\n",
                    cgnet::WsaErrorString().c_str());
    else
        std::printf("second bind unexpectedly succeeded\n");

    // And a known code looked up by hand.
    std::printf("\nDirect WsaErrorString lookups:\n");
    const int codes[] = { WSAECONNREFUSED, WSAENOTCONN, WSAEHOSTUNREACH, 12345 };
    for (int code : codes)
        std::printf("  %s\n", cgnet::WsaErrorString(code).c_str());

    ::closesocket(s2);
    ::closesocket(s1);
    ::WSACleanup();
}

// ----- demo: HttpServer + OverlappedSend -----------------------------------

void demoHttpAndOverlapped()
{
    section("HttpServer + OverlappedSend");

    cgnet::HttpServer server;
    auto& raw = server.Raw();

    raw.Get("/info", [](const httplib::Request&, httplib::Response& res) {
        std::ostringstream os;
        os << "{\n  \"adapters\": [";
        bool first = true;
        for (const auto& a : cgnet::ListAdapters())
        {
            if (!first) os << ",";
            first = false;
            os << "\n    { \"name\": \"" << a.description << "\""
               << ", \"up\": " << (a.is_up ? "true" : "false");
            if (!a.ipv4.empty())
                os << ", \"ipv4\": \"" << a.ipv4.front() << "\"";
            os << " }";
        }
        os << "\n  ],\n  \"default_gateway\": ";
        if (auto g = cgnet::GetDefaultGateway()) os << "\"" << *g << "\"";
        else                                     os << "null";
        os << "\n}\n";
        res.set_content(os.str(), "application/json");
    });

    raw.Post("/echo", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content(req.body, "text/plain");
    });

    constexpr int kPort = 58032;
    if (!server.Start(kPort))
    {
        std::printf("HttpServer::Start failed on port %d\n", kPort);
        return;
    }
    std::printf("HTTP server listening on http://127.0.0.1:%d/info\n", kPort);

    // Give the accept thread a moment to enter the loop
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // ----- OverlappedSend self-test ---------------------------------------
    // Connect a raw client socket, push an HTTP request via OverlappedSend,
    // read the reply, and check the status line.

    WSADATA wsa{};
    ::WSAStartup(MAKEWORD(2, 2), &wsa);

    const SOCKET client = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = ::htons(kPort);
    addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);

    if (::connect(client, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        cgnet::LogLastWsaError("client connect");
    }
    else
    {
        const std::string req = "GET /info HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
        cgnet::OverlappedSend op(static_cast<std::uintptr_t>(client));
        if (op.Issue(req.data(), req.size()))
        {
            const int sent = op.Wait();
            std::printf("OverlappedSend transferred %d bytes (request size %zu)\n",
                        sent, req.size());

            char buf[512] = { 0 };
            int total = 0;
            while (total < static_cast<int>(sizeof(buf) - 1))
            {
                const int n = ::recv(client, buf + total, sizeof(buf) - 1 - total, 0);
                if (n <= 0) break;
                total += n;
                if (total >= 12) break; // enough to see "HTTP/1.1 200"
            }
            buf[std::min(total, static_cast<int>(sizeof(buf) - 1))] = 0;

            std::string line(buf);
            auto eol = line.find("\r\n");
            if (eol != std::string::npos) line.resize(eol);
            std::printf("Reply status line: %s\n", line.c_str());
        }
    }

    ::closesocket(client);
    ::WSACleanup();

    server.Stop();
}

// ----- main -----------------------------------------------------------------

void waitForQuit()
{
    section("Interactive");
    std::printf("Type 'quit' + Enter to exit.\n");
    std::string line;
    while (std::getline(std::cin, line))
    {
        if (line == "quit" || line == "q" || line == "exit")
            break;
    }
}

} // namespace

int main(int argc, char** argv)
{
    bool interactive = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "-i") == 0 || std::strcmp(argv[i], "--interactive") == 0)
            interactive = true;
    }

    demoNetworkInfo();
    demoWsaError();
    demoHttpAndOverlapped();

    if (interactive)
        waitForQuit();

    return 0;
}
