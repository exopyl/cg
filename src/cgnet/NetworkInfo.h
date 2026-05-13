#pragma once

// ============================================================================
//  NetworkInfo — enumerate local network adapters, gateways and SMB shares
// ============================================================================
//
// Built on the modern Win32 API: GetAdaptersAddresses (IPv6-aware, replaces
// the deprecated GetAdaptersInfo) and WNetEnumResource for SMB browsing.
//
// All functions return owned values (no caller-owned buffers). Empty vectors
// indicate either an empty system state or an enumeration failure — call
// LogLastWsaError if you need to discriminate.
//
// ============================================================================

#include <optional>
#include <string>
#include <vector>

namespace cgnet {

struct Adapter
{
    std::string              name;          // friendly name (UTF-8)
    std::string              description;   // hardware description (UTF-8)
    std::string              mac;           // "AA:BB:CC:DD:EE:FF" or empty
    std::vector<std::string> ipv4;          // dotted-quad strings
    std::vector<std::string> ipv6;          // canonical IPv6 strings
    std::vector<std::string> gateways;      // dotted-quad / canonical IPv6
    bool                     dhcp_enabled = false;
    bool                     is_up        = false; // IfOperStatus == Up
};

// Enumerate all unicast adapters (IPv4 + IPv6, all states). Loopback and
// tunnel pseudo-interfaces are skipped.
std::vector<Adapter> ListAdapters();

// Returns the first non-empty gateway found across all "up" adapters,
// preferring IPv4. Empty optional means "no default gateway configured".
std::optional<std::string> GetDefaultGateway();

// Enumerate SMB network resources visible from this host (\\HOST\share style).
// Best-effort: may be empty on machines with the browser service disabled.
std::vector<std::string> EnumerateSmbShares();

} // namespace cgnet
