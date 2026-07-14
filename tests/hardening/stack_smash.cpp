#include <cstddef>
#include <cstdint>

extern "C" [[gnu::noinline]] int mediaproxy_corrupt_stack()
{
    volatile std::uint8_t buffer[8]{};
    auto* address = const_cast<std::uint8_t*>(buffer);

    asm volatile("" : "+r"(address) : : "memory");
    for (std::size_t index = 0; index < 64; ++index) {
        address[sizeof(buffer) + index] =
            static_cast<std::uint8_t>(0xa5U ^ index);
    }
    asm volatile("" : : : "memory");

    return buffer[0];
}

int main()
{
    return mediaproxy_corrupt_stack();
}
