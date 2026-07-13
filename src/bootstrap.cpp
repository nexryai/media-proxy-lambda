#include <string_view>

int main()
{
    constexpr std::string_view runtime_name = "mediaproxy-lambda";
    return runtime_name.empty() ? 1 : 0;
}
