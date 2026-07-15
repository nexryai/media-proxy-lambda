#include <cstddef>
#include <cstring>
#include <memory>
#include <type_traits>

#include <expat_config.h>
#include <expat.h>
#include <gtest/gtest.h>

namespace {

struct ParseContext {
    std::size_t element_count = 0;
    bool saw_namespaced_root = false;
};

void XMLCALL StartElement(
    void* user_data, const XML_Char* name, const XML_Char**) noexcept
{
    auto* const context = static_cast<ParseContext*>(user_data);
    ++context->element_count;
    if (std::strcmp(name, "urn:mediaproxy|root") == 0) {
        context->saw_namespaced_root = true;
    }
}

using ParserPointee = std::remove_pointer_t<XML_Parser>;
using ParserPtr =
    std::unique_ptr<ParserPointee, decltype(&XML_ParserFree)>;

} // namespace

TEST(BuildSmoke, ParsesBoundedXmlWithPinnedLibExpat)
{
    constexpr unsigned long long protection_activation_bytes =
        4ULL * 1024ULL * 1024ULL;

    EXPECT_STREQ(XML_ExpatVersion(), "expat_2.8.2");

    ParserPtr parser(XML_ParserCreateNS(nullptr, '|'), &XML_ParserFree);
    ASSERT_NE(parser, nullptr);
    ASSERT_EQ(XML_SetBillionLaughsAttackProtectionMaximumAmplification(
                  parser.get(), 8.0F),
        XML_TRUE);
    ASSERT_EQ(XML_SetBillionLaughsAttackProtectionActivationThreshold(
                  parser.get(), protection_activation_bytes),
        XML_TRUE);
    ASSERT_EQ(XML_SetAllocTrackerMaximumAmplification(
                  parser.get(), 8.0F),
        XML_TRUE);
    ASSERT_EQ(XML_SetAllocTrackerActivationThreshold(
                  parser.get(), protection_activation_bytes),
        XML_TRUE);
    ASSERT_EQ(XML_SetReparseDeferralEnabled(parser.get(), XML_TRUE),
        XML_TRUE);

    ParseContext context;
    XML_SetUserData(parser.get(), &context);
    XML_SetElementHandler(parser.get(), &StartElement, nullptr);

    constexpr char xml[] =
        "<root xmlns='urn:mediaproxy'><child value='safe'/></root>";
    constexpr std::size_t xml_size = sizeof(xml) - 1U;
    static_assert(xml_size < protection_activation_bytes);
    ASSERT_EQ(XML_Parse(parser.get(), xml, static_cast<int>(xml_size),
                  XML_TRUE),
        XML_STATUS_OK);
    EXPECT_EQ(context.element_count, 2U);
    EXPECT_TRUE(context.saw_namespaced_root);
}
