#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include <mediaproxy/handler.hpp>

namespace mediaproxy {

void log_invocation(
    std::FILE* output,
    std::string_view request_id,
    const HandlerDiagnostics& diagnostics,
    std::uint16_t status,
    std::size_t event_bytes,
    std::size_t response_bytes,
    std::uint64_t handler_microseconds) noexcept;

void log_runtime_failure(
    std::FILE* output,
    std::string_view request_id,
    std::string_view category) noexcept;

} // namespace mediaproxy
