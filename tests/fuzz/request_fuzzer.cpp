#include <cstddef>
#include <cstdint>
#include <string_view>

#include <mediaproxy/http/event.hpp>
#include <mediaproxy/http/query.hpp>
#include <mediaproxy/http/url_policy.hpp>

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data,
    std::size_t size)
{
    const std::string_view text{
        reinterpret_cast<const char*>(data), size};
    static_cast<void>(mediaproxy::http::parse_function_url_event(text));
    static_cast<void>(mediaproxy::http::parse_query(text));
    static_cast<void>(mediaproxy::http::validate_origin_url(text));
    return 0;
}
