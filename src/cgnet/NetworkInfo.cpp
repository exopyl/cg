#include "NetworkInfo.h"
#include "WsaError.h"

#define _WINSOCKAPI_
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#include <winnetwk.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mpr.lib")

namespace cgnet {

namespace {

std::string sockaddrToString(const SOCKADDR* addr, socklen_t len)
{
    char buf[INET6_ADDRSTRLEN] = { 0 };
    if (::getnameinfo(addr, len, buf, sizeof(buf), nullptr, 0, NI_NUMERICHOST) != 0)
        return {};
    return buf;
}

std::string formatMac(const BYTE* bytes, ULONG len)
{
    if (len == 0) return {};
    char buf[32];
    if (len == 6)
        std::snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                      bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]);
    else
        return {}; // skip non-Ethernet MACs (IPoIB, etc.)
    return buf;
}

std::string wideToUtf8(const wchar_t* w)
{
    if (w == nullptr || *w == 0) return {};
    const int needed = ::WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1) return {};
    std::string out(static_cast<size_t>(needed - 1), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), needed, nullptr, nullptr);
    return out;
}

} // namespace

std::vector<Adapter> ListAdapters()
{
    constexpr ULONG flags =
        GAA_FLAG_INCLUDE_PREFIX |
        GAA_FLAG_SKIP_ANYCAST |
        GAA_FLAG_SKIP_MULTICAST |
        GAA_FLAG_SKIP_DNS_SERVER;

    ULONG bufLen = 16 * 1024;
    std::unique_ptr<BYTE[]> buf(new BYTE[bufLen]);

    ULONG rc = ::GetAdaptersAddresses(AF_UNSPEC, flags, nullptr,
                                      reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.get()),
                                      &bufLen);
    if (rc == ERROR_BUFFER_OVERFLOW)
    {
        buf.reset(new BYTE[bufLen]);
        rc = ::GetAdaptersAddresses(AF_UNSPEC, flags, nullptr,
                                    reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.get()),
                                    &bufLen);
    }
    if (rc != ERROR_SUCCESS)
    {
        std::fprintf(stderr, "ListAdapters: GetAdaptersAddresses failed (rc=%lu)\n", rc);
        return {};
    }

    std::vector<Adapter> out;
    for (auto* a = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.get());
         a != nullptr; a = a->Next)
    {
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK ||
            a->IfType == IF_TYPE_TUNNEL)
            continue;

        Adapter ad;
        ad.name         = a->AdapterName ? a->AdapterName : "";
        ad.description  = wideToUtf8(a->Description);
        ad.mac          = formatMac(a->PhysicalAddress, a->PhysicalAddressLength);
        ad.dhcp_enabled = (a->Flags & IP_ADAPTER_DHCP_ENABLED) != 0;
        ad.is_up        = (a->OperStatus == IfOperStatusUp);

        for (auto* u = a->FirstUnicastAddress; u != nullptr; u = u->Next)
        {
            const auto family = u->Address.lpSockaddr->sa_family;
            const std::string s = sockaddrToString(u->Address.lpSockaddr,
                                                   u->Address.iSockaddrLength);
            if (s.empty()) continue;
            if (family == AF_INET)       ad.ipv4.push_back(s);
            else if (family == AF_INET6) ad.ipv6.push_back(s);
        }

        for (auto* g = a->FirstGatewayAddress; g != nullptr; g = g->Next)
        {
            const std::string s = sockaddrToString(g->Address.lpSockaddr,
                                                   g->Address.iSockaddrLength);
            if (!s.empty()) ad.gateways.push_back(s);
        }

        out.push_back(std::move(ad));
    }
    return out;
}

std::optional<std::string> GetDefaultGateway()
{
    for (const auto& a : ListAdapters())
    {
        if (!a.is_up) continue;
        for (const auto& g : a.gateways)
        {
            if (g.find('.') != std::string::npos)
                return g; // IPv4 preferred
        }
    }
    for (const auto& a : ListAdapters())
    {
        if (!a.is_up) continue;
        if (!a.gateways.empty())
            return a.gateways.front(); // any (IPv6 fallback)
    }
    return std::nullopt;
}

std::vector<std::string> EnumerateSmbShares()
{
    std::vector<std::string> out;
    HANDLE hEnum = nullptr;

    DWORD rc = ::WNetOpenEnumA(RESOURCE_GLOBALNET, RESOURCETYPE_DISK,
                               0, nullptr, &hEnum);
    if (rc != NO_ERROR || hEnum == nullptr)
        return out;

    constexpr DWORD kBufBytes = 16 * 1024;
    std::unique_ptr<BYTE[]> buf(new BYTE[kBufBytes]);

    for (;;)
    {
        DWORD count = 0xFFFFFFFF;
        DWORD bufSize = kBufBytes;
        rc = ::WNetEnumResourceA(hEnum, &count, buf.get(), &bufSize);
        if (rc == ERROR_NO_MORE_ITEMS || count == 0)
            break;
        if (rc != NO_ERROR)
            break;

        auto* arr = reinterpret_cast<NETRESOURCEA*>(buf.get());
        for (DWORD i = 0; i < count; ++i)
        {
            if (arr[i].lpRemoteName != nullptr && arr[i].lpRemoteName[0] != '\0')
                out.emplace_back(arr[i].lpRemoteName);
        }
    }

    ::WNetCloseEnum(hEnum);
    return out;
}

} // namespace cgnet
