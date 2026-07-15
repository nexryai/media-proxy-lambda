#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

#include <ffi.h>
#include <gtest/gtest.h>

namespace {

std::int64_t Add(std::int64_t left, std::int64_t right) noexcept
{
    return left + right;
}

void AddBias(ffi_cif*, void* result, void** arguments,
    void* user_data) noexcept
{
    const auto left = *static_cast<const std::int64_t*>(arguments[0]);
    const auto right = *static_cast<const std::int64_t*>(arguments[1]);
    const auto bias = *static_cast<const std::int64_t*>(user_data);
    *static_cast<std::int64_t*>(result) = left + right + bias;
}

struct FfiClosureDeleter {
    void operator()(ffi_closure* closure) const noexcept
    {
        ffi_closure_free(closure);
    }
};

struct LargeStruct {
    std::array<std::uint64_t, 4> words;
};

std::uint64_t SumLargeStructs(LargeStruct value0, LargeStruct value1,
    LargeStruct value2, LargeStruct value3, LargeStruct value4,
    LargeStruct value5, LargeStruct value6, LargeStruct value7,
    LargeStruct value8, LargeStruct value9, LargeStruct value10,
    LargeStruct value11, LargeStruct value12, LargeStruct value13,
    LargeStruct value14, LargeStruct value15) noexcept
{
    return value0.words[0] + value1.words[0] + value2.words[0]
        + value3.words[0] + value4.words[0] + value5.words[0]
        + value6.words[0] + value7.words[0] + value8.words[0]
        + value9.words[0] + value10.words[0] + value11.words[0]
        + value12.words[0] + value13.words[0] + value14.words[0]
        + value15.words[0];
}

ffi_status PrepareBinaryInt64Call(ffi_cif& call_interface,
    ffi_type* (&argument_types)[2]) noexcept
{
    argument_types[0] = &ffi_type_sint64;
    argument_types[1] = &ffi_type_sint64;
    return ffi_prep_cif(&call_interface, FFI_DEFAULT_ABI, 2,
        &ffi_type_sint64, argument_types);
}

} // namespace

TEST(BuildSmoke, CallsFunctionThroughPinnedLibFfi)
{
    ffi_type* argument_types[2]{};
    ffi_cif call_interface{};
    ASSERT_EQ(PrepareBinaryInt64Call(call_interface, argument_types), FFI_OK);
    std::int64_t left = 17;
    std::int64_t right = 25;
    void* arguments[] = {&left, &right};
    std::int64_t result = 0;

    ffi_call(&call_interface, reinterpret_cast<void (*)()>(&Add), &result,
        arguments);

    EXPECT_EQ(result, 42);
}

TEST(BuildSmoke, CallsPinnedLibFfiStaticTrampolineClosure)
{
    ffi_type* argument_types[2]{};
    ffi_cif call_interface{};
    ASSERT_EQ(PrepareBinaryInt64Call(call_interface, argument_types), FFI_OK);
    void* code = nullptr;
    std::unique_ptr<ffi_closure, FfiClosureDeleter> closure(
        static_cast<ffi_closure*>(ffi_closure_alloc(
            sizeof(ffi_closure), &code)));
    ASSERT_NE(closure, nullptr);
    ASSERT_NE(code, nullptr);
    std::int64_t bias = 5;
    ASSERT_EQ(ffi_prep_closure_loc(closure.get(), &call_interface, &AddBias,
                  &bias, code),
        FFI_OK);

    std::int64_t left = 17;
    std::int64_t right = 20;
    void* arguments[] = {&left, &right};
    std::int64_t result = 0;
    ffi_call(&call_interface, reinterpret_cast<void (*)()>(code), &result,
        arguments);

    EXPECT_EQ(result, 42);
}

TEST(BuildSmoke, PassesSixteenLargeStructsByValueThroughPinnedLibFfi)
{
    ffi_type* large_struct_elements[] = {
        &ffi_type_uint64,
        &ffi_type_uint64,
        &ffi_type_uint64,
        &ffi_type_uint64,
        nullptr,
    };
    ffi_type large_struct_type{
        0, 0, FFI_TYPE_STRUCT, large_struct_elements};
    std::array<ffi_type*, 16> argument_types{};
    argument_types.fill(&large_struct_type);
    ffi_cif call_interface{};
    ASSERT_EQ(ffi_prep_cif(&call_interface, FFI_DEFAULT_ABI,
                  argument_types.size(), &ffi_type_uint64,
                  argument_types.data()),
        FFI_OK);

    std::array<LargeStruct, 16> values{};
    std::array<void*, 16> arguments{};
    std::uint64_t expected = 0;
    for (std::size_t index = 0; index < values.size(); ++index) {
        values[index].words = {index + 1, index + 101,
            index + 201, index + 301};
        arguments[index] = &values[index];
        expected += values[index].words[0];
    }
    std::uint64_t result = 0;

    ffi_call(&call_interface,
        reinterpret_cast<void (*)()>(&SumLargeStructs), &result,
        arguments.data());

    EXPECT_EQ(result, expected);
}
