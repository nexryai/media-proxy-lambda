#include <array>
#include <memory>
#include <string_view>

#include <gio/gio.h>
#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>
#include <gtest/gtest.h>

namespace {

struct GObjectDeleter {
    void operator()(gpointer object) const noexcept
    {
        g_object_unref(object);
    }
};

} // namespace

TEST(BuildSmoke, UsesPinnedGlibUnicodeAndRegex)
{
    EXPECT_EQ(glib_major_version, 2U);
    EXPECT_EQ(glib_minor_version, 88U);
    EXPECT_EQ(glib_micro_version, 2U);

    std::unique_ptr<gchar, decltype(&g_free)> normalized(
        g_utf8_normalize("e\xCC\x81", -1, G_NORMALIZE_DEFAULT_COMPOSE),
        &g_free);
    ASSERT_NE(normalized, nullptr);
    EXPECT_EQ(std::string_view{normalized.get()}, "\xC3\xA9");

    GError* raw_error = nullptr;
    std::unique_ptr<GRegex, decltype(&g_regex_unref)> expression(
        g_regex_new("^media-[0-9]+$", G_REGEX_DEFAULT,
            G_REGEX_MATCH_DEFAULT, &raw_error),
        &g_regex_unref);
    std::unique_ptr<GError, decltype(&g_error_free)> error(
        raw_error, &g_error_free);
    ASSERT_EQ(error, nullptr);
    ASSERT_NE(expression, nullptr);
    EXPECT_TRUE(g_regex_match(expression.get(), "media-42",
        G_REGEX_MATCH_DEFAULT, nullptr));
}

TEST(BuildSmoke, UsesPinnedGObjectAndGio)
{
    std::unique_ptr<GObject, GObjectDeleter> object(
        G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr)));
    ASSERT_NE(object, nullptr);
    EXPECT_TRUE(G_IS_OBJECT(object.get()));

    constexpr std::string_view input = "bounded-gio-stream";
    std::unique_ptr<GInputStream, GObjectDeleter> stream(
        g_memory_input_stream_new_from_data(
            input.data(), input.size(), nullptr));
    ASSERT_NE(stream, nullptr);
    std::array<char, 32> buffer{};
    const gssize bytes_read = g_input_stream_read(stream.get(),
        buffer.data(), buffer.size(), nullptr, nullptr);
    ASSERT_EQ(bytes_read, static_cast<gssize>(input.size()));
    EXPECT_EQ(std::string_view(buffer.data(), input.size()), input);
}

TEST(BuildSmoke, DisablesDynamicGmoduleLoading)
{
    EXPECT_FALSE(g_module_supported());
}
