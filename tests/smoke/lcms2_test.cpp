#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <gtest/gtest.h>
#include <lcms2.h>

TEST(BuildSmoke, RoundTripsPinnedLcms2ProfileInMemory)
{
    EXPECT_EQ(cmsGetEncodedCMMversion(), 2190);

    using Profile = std::unique_ptr<void, decltype(&cmsCloseProfile)>;
    Profile generated(cmsCreate_sRGBProfile(), &cmsCloseProfile);
    ASSERT_NE(generated, nullptr);
    EXPECT_EQ(cmsGetColorSpace(generated.get()), cmsSigRgbData);

    cmsUInt32Number encoded_size = 0;
    ASSERT_NE(
        cmsSaveProfileToMem(generated.get(), nullptr, &encoded_size), FALSE);
    ASSERT_GT(encoded_size, 0U);
    constexpr cmsUInt32Number maximum_profile_size = 1024U * 1024U;
    ASSERT_LE(encoded_size, maximum_profile_size);

    std::vector<std::uint8_t> encoded(encoded_size);
    ASSERT_NE(cmsSaveProfileToMem(
                  generated.get(), encoded.data(), &encoded_size),
        FALSE);
    ASSERT_EQ(encoded_size, encoded.size());

    Profile parsed(
        cmsOpenProfileFromMem(encoded.data(), encoded_size),
        &cmsCloseProfile);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(cmsGetColorSpace(parsed.get()), cmsSigRgbData);
    EXPECT_GE(cmsGetProfileVersion(parsed.get()), 4.0);
}
