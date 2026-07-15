#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include <gtest/gtest.h>
#include <nghttp2/nghttp2.h>

TEST(BuildSmoke, SerializesPinnedNghttp2ClientPreface)
{
    const nghttp2_info* const version = nghttp2_version(0);
    ASSERT_NE(version, nullptr);
    EXPECT_EQ(version->version_num, NGHTTP2_VERSION_NUM);

    nghttp2_session_callbacks* raw_callbacks = nullptr;
    ASSERT_EQ(nghttp2_session_callbacks_new(&raw_callbacks), 0);
    std::unique_ptr<nghttp2_session_callbacks,
        decltype(&nghttp2_session_callbacks_del)>
        callbacks(raw_callbacks, &nghttp2_session_callbacks_del);

    nghttp2_session* raw_session = nullptr;
    ASSERT_EQ(
        nghttp2_session_client_new(&raw_session, callbacks.get(), nullptr),
        0);
    std::unique_ptr<nghttp2_session, decltype(&nghttp2_session_del)> session(
        raw_session,
        &nghttp2_session_del);

    const nghttp2_settings_entry settings = {
        NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS,
        1,
    };
    ASSERT_EQ(
        nghttp2_submit_settings(
            session.get(), NGHTTP2_FLAG_NONE, &settings, 1),
        0);

    const std::uint8_t* serialized = nullptr;
    const nghttp2_ssize preface_length =
        nghttp2_session_mem_send2(session.get(), &serialized);
    ASSERT_EQ(preface_length, NGHTTP2_CLIENT_MAGIC_LEN);
    ASSERT_NE(serialized, nullptr);
    EXPECT_EQ(
        std::string_view(
            reinterpret_cast<const char*>(serialized),
            static_cast<std::size_t>(preface_length)),
        std::string_view(NGHTTP2_CLIENT_MAGIC, NGHTTP2_CLIENT_MAGIC_LEN));

    EXPECT_GT(nghttp2_session_mem_send2(session.get(), &serialized), 0);
}
