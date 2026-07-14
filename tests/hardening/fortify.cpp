#include <cstddef>
#include <string.h>

#ifndef _FORTIFY_STRING_H
#error "The pinned fortify string wrapper is not active"
#endif

namespace {

[[gnu::noinline]] std::size_t opaque_size(std::size_t value)
{
    asm volatile("" : "+r"(value) : : "memory");
    return value;
}

} // namespace

int main(int argc, char**)
{
    const std::size_t destination_size =
        opaque_size(argc == 1 ? 16U : 8U);
    auto* destination =
        static_cast<unsigned char*>(__builtin_alloca(destination_size));
    const unsigned char source[16]{};

    ::memcpy(destination, source, opaque_size(sizeof(source)));
    return destination[0];
}
