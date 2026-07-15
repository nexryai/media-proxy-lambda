#include <array>
#include <string_view>

#include <gtest/gtest.h>
#include <zlib.h>

TEST(BuildSmoke, RoundTripsWithPinnedZlib)
{
    constexpr std::string_view input =
        "bounded compressed response body for MediaProxy";
    std::array<Bytef, 128> compressed{};
    uLongf compressed_length = compressed.size();

    ASSERT_EQ(
        compress2(
            compressed.data(),
            &compressed_length,
            reinterpret_cast<const Bytef*>(input.data()),
            input.size(),
            Z_BEST_COMPRESSION),
        Z_OK);

    std::array<char, 64> output{};
    uLongf output_length = output.size();
    ASSERT_EQ(
        uncompress(
            reinterpret_cast<Bytef*>(output.data()),
            &output_length,
            compressed.data(),
            compressed_length),
        Z_OK);
    EXPECT_EQ(std::string_view(output.data(), output_length), input);

    std::array<Bytef, 4> undersized_output{};
    uLongf undersized_length = undersized_output.size();
    EXPECT_EQ(
        uncompress(
            undersized_output.data(),
            &undersized_length,
            compressed.data(),
            compressed_length),
        Z_BUF_ERROR);
}
