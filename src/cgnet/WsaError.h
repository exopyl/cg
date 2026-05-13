#pragma once

// ============================================================================
//  WsaError — human-readable Winsock error strings
// ============================================================================
//
// Combines FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM) for the official Windows
// description with a short debug-oriented gloss for the codes we hit often
// (WSAEADDRINUSE, WSAENOTCONN, ...).
//
// ============================================================================

#include <string>

namespace cgnet {

// Sentinel meaning "use ::WSAGetLastError()". Passing 0 returns "no error".
inline constexpr int kLastWsaError = -1;

// Format `code` as "[WSAEADDRINUSE (10048)] <system message> — <gloss>".
// Trailing CR/LF from FormatMessage is stripped. The gloss is empty for codes
// not present in the internal map.
std::string WsaErrorString(int code = kLastWsaError);

// Write WsaErrorString(WSAGetLastError()) to stderr, prefixed by `context`.
// No-op if WSAGetLastError() returns 0.
void LogLastWsaError(const char* context);

} // namespace cgnet
