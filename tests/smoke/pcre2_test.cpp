#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include <pcre2.h>
#include <gtest/gtest.h>

namespace {

using CodePtr = std::unique_ptr<pcre2_code, decltype(&pcre2_code_free)>;
using MatchContextPtr =
    std::unique_ptr<pcre2_match_context, decltype(&pcre2_match_context_free)>;
using MatchDataPtr =
    std::unique_ptr<pcre2_match_data, decltype(&pcre2_match_data_free)>;

} // namespace

TEST(BuildSmoke, MatchesBoundedUnicodeWithPinnedPcre2)
{
    PCRE2_UCHAR version[32] = {};
    ASSERT_GE(pcre2_config(PCRE2_CONFIG_VERSION, version), 0);
    EXPECT_TRUE(
        std::string_view{reinterpret_cast<const char*>(version)}.starts_with(
            "10.47 "));

    std::uint32_t jit_enabled = 1U;
    ASSERT_GE(pcre2_config(PCRE2_CONFIG_JIT, &jit_enabled), 0);
    EXPECT_EQ(jit_enabled, 0U);

    constexpr char pattern[] =
        R"(^/image/\p{L}{1,16}\.(?:png|webp)$)";
    int error_code = 0;
    PCRE2_SIZE error_offset = 0;
    CodePtr code(
        pcre2_compile(reinterpret_cast<PCRE2_SPTR>(pattern),
            sizeof(pattern) - 1U, PCRE2_UTF | PCRE2_UCP, &error_code,
            &error_offset, nullptr),
        &pcre2_code_free);
    ASSERT_NE(code, nullptr) << "PCRE2 error " << error_code << " at "
                             << error_offset;

    MatchContextPtr match_context(
        pcre2_match_context_create(nullptr), &pcre2_match_context_free);
    ASSERT_NE(match_context, nullptr);
    ASSERT_EQ(pcre2_set_match_limit(match_context.get(), 10'000U), 0);
    ASSERT_EQ(pcre2_set_depth_limit(match_context.get(), 1'000U), 0);
    ASSERT_EQ(pcre2_set_heap_limit(match_context.get(), 1'024U), 0);

    MatchDataPtr match_data(pcre2_match_data_create_from_pattern(
                                code.get(), nullptr),
        &pcre2_match_data_free);
    ASSERT_NE(match_data, nullptr);

    constexpr char subject[] = "/image/安全.webp";
    constexpr std::size_t subject_size = sizeof(subject) - 1U;
    static_assert(subject_size < 64U);
    ASSERT_EQ(pcre2_match(code.get(),
                  reinterpret_cast<PCRE2_SPTR>(subject), subject_size, 0U, 0U,
                  match_data.get(), match_context.get()),
        1);

    PCRE2_SIZE* const offsets = pcre2_get_ovector_pointer(match_data.get());
    ASSERT_NE(offsets, nullptr);
    EXPECT_EQ(offsets[0], 0U);
    EXPECT_EQ(offsets[1], subject_size);
}
