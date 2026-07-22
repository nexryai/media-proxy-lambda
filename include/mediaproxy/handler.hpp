#pragma once

#include <cstddef>
#include <span>

#include <mediaproxy/http/dns_resolver.hpp>
#include <mediaproxy/http/origin_download.hpp>
#include <mediaproxy/http/response.hpp>

namespace mediaproxy {

[[nodiscard]] http::HttpResponse handle_function_url_event(
    std::span<const std::byte> event,
    http::OriginTimeoutApi timeout,
    http::AddressResolverApi resolver = {},
    http::OriginTransportApi transport = http::system_origin_transport());

} // namespace mediaproxy
