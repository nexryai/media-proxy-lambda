#include <cstddef>
#include <cstdlib>
#include <limits>
#include <string_view>

namespace {

int trigger_address_error()
{
    volatile char* const memory =
        static_cast<volatile char*>(std::malloc(4));
    if (memory == nullptr) {
        return 2;
    }
    volatile std::size_t offset = 4;
    memory[offset] = 1;
    std::free(const_cast<char*>(memory));
    return 0;
}

int trigger_undefined_error()
{
    volatile int maximum = std::numeric_limits<int>::max();
    volatile int one = 1;
    return maximum + one;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        return 2;
    }
    if (std::string_view{argv[1]} == "address") {
        return trigger_address_error();
    }
    if (std::string_view{argv[1]} == "undefined") {
        return trigger_undefined_error();
    }
    return 2;
}
