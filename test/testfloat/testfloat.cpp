// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <fizzy/execute.hpp>
#include <fizzy/parser.hpp>
#include <test/utils/floating_point_utils.hpp>
#include <test/utils/hex.hpp>
#include <test/utils/typed_value.hpp>
#include <cfenv>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

using namespace fizzy;
using namespace fizzy::test;

namespace
{
/// The "usage" help message.
///
/// The option names match the ones from the testfloat_gen / testfloat tools.
///
/// TODO: Rename <float>_{ceil,floor,trunc,nearest} to <float>_roundToInt_r_<round>
///       as in the testfloat tool?
/// TODO: Rename <float>_trunc_to_<int> to <float>_to_<int>_r_<round>
///       as in the testfloat tool?
constexpr auto USAGE = R"(Fizzy TestFloat

Tool for testing IEEE 754 floating-point compliance.
Expects inputs from TestFloat's testfloat_gen.
See http://www.jhauser.us/arithmetic/TestFloat-3/doc/TestFloat-general.html.

Usage:
  fizzy-testfloat [options] <function>

  -rnear_even           Round to nearest/even [default].
  -rminMag              Round to minimum magnitude (toward zero).
  -rmin                 Round to minimum (down).
  -rmax                 Round to maximum (up).
  -ignore_nan_payloads  Do not check for specific NaN payloads (NaN category is still checked).

<function>:
  The function names match the ones from the testfloat_gen tool, with the following exceptions:
  - <float>_to_<int> is <float>_trunc_to_<int> as the result is always rounded with -rminMag,
  - <float>_roundToInt is <float>_{ceil,floor,trunc,nearest} as they have the rounding direction fixed.

Example:
  testfloat_gen f32_add | fizzy-testfloat f32_add
)";

// The maximum number of failures reported.
constexpr auto max_failures = 5;

enum class Options
{
    None = 0,

    /// The Wasm trap happens iff IEEE "invalid operation" exception.
    TrapIsInvalidOperation = 1,
};

enum FPException : uint8_t
{
    /// An operand is invalid for the operation about to be performed.
    InvalidOperation = 0x10,
};

struct FunctionDescription
{
    const FuncIdx idx;
    const ValType result_type;
    const ValType param_types[2];
    const size_t num_arguments;
    const Options options = Options::None;

    constexpr FunctionDescription(FuncIdx func_idx, ValType result, ValType param) noexcept
      : idx{func_idx}, result_type{result}, param_types{param}, num_arguments{1}
    {}

    constexpr FunctionDescription(
        FuncIdx func_idx, ValType result_ty, ValType param1, ValType param2) noexcept
      : idx{func_idx}, result_type{result_ty}, param_types{param1, param2}, num_arguments{2}
    {}

    constexpr FunctionDescription(
        FuncIdx func_idx, ValType result, ValType param, Options opts) noexcept
      : idx{func_idx},
        result_type{result},
        param_types{param, ValType::f32},
        num_arguments{1},
        options{opts}
    {}
};

auto get_wasm_code()
{
    /* wat2wasm
    (func $f32_add (param f32 f32) (result f32) (f32.add (local.get 0) (local.get 1)))
    (func $f64_add (param f64 f64) (result f64) (f64.add (local.get 0) (local.get 1)))
    (func $f32_sub (param f32 f32) (result f32) (f32.sub (local.get 0) (local.get 1)))
    (func $f64_sub (param f64 f64) (result f64) (f64.sub (local.get 0) (local.get 1)))
    (func $f32_mul (param f32 f32) (result f32) (f32.mul (local.get 0) (local.get 1)))
    (func $f64_mul (param f64 f64) (result f64) (f64.mul (local.get 0) (local.get 1)))
    (func $f32_div (param f32 f32) (result f32) (f32.div (local.get 0) (local.get 1)))
    (func $f64_div (param f64 f64) (result f64) (f64.div (local.get 0) (local.get 1)))
    (func $f32_sqrt (param f32) (result f32) (f32.sqrt (local.get 0)))
    (func $f64_sqrt (param f64) (result f64) (f64.sqrt (local.get 0)))

    (func $i32_to_f32 (param i32) (result f32) (f32.convert_i32_s (local.get 0)))
    (func $ui32_to_f32 (param i32) (result f32) (f32.convert_i32_u (local.get 0)))
    (func $i64_to_f32 (param i64) (result f32) (f32.convert_i64_s (local.get 0)))
    (func $ui64_to_f32 (param i64) (result f32) (f32.convert_i64_u (local.get 0)))
    (func $i32_to_f64 (param i32) (result f64) (f64.convert_i32_s (local.get 0)))
    (func $ui32_to_f64 (param i32) (result f64) (f64.convert_i32_u (local.get 0)))
    (func $i64_to_f64 (param i64) (result f64) (f64.convert_i64_s (local.get 0)))
    (func $ui64_to_f64 (param i64) (result f64) (f64.convert_i64_u (local.get 0)))

    (func $f32_eq (param f32 f32) (result i32) (f32.eq (local.get 0) (local.get 1)))
    (func $f64_eq (param f64 f64) (result i32) (f64.eq (local.get 0) (local.get 1)))
    (func $f32_lt (param f32 f32) (result i32) (f32.lt (local.get 0) (local.get 1)))
    (func $f64_lt (param f64 f64) (result i32) (f64.lt (local.get 0) (local.get 1)))
    (func $f32_le (param f32 f32) (result i32) (f32.le (local.get 0) (local.get 1)))
    (func $f64_le (param f64 f64) (result i32) (f64.le (local.get 0) (local.get 1)))

    (func $f32_to_i32 (param f32) (result i32) (i32.trunc_f32_s (local.get 0)))
    (func $f32_to_ui32 (param f32) (result i32) (i32.trunc_f32_u (local.get 0)))
    (func $f32_to_i64 (param f32) (result i64) (i64.trunc_f32_s (local.get 0)))
    (func $f32_to_ui64 (param f32) (result i64) (i64.trunc_f32_u (local.get 0)))
    (func $f64_to_i32 (param f64) (result i32) (i32.trunc_f64_s (local.get 0)))
    (func $f64_to_ui32 (param f64) (result i32) (i32.trunc_f64_u (local.get 0)))
    (func $f64_to_i64 (param f64) (result i64) (i64.trunc_f64_s (local.get 0)))
    (func $f64_to_ui64 (param f64) (result i64) (i64.trunc_f64_u (local.get 0)))

    (func $f32_to_f64 (param f32) (result f64) (f64.promote_f32 (local.get 0)))
    (func $f64_to_f32 (param f64) (result f32) (f32.demote_f64 (local.get 0)))

    (func $f32_ceil (param f32) (result f32) (f32.ceil (local.get 0)))
    (func $f32_floor (param f32) (result f32) (f32.floor (local.get 0)))
    (func $f32_trunc (param f32) (result f32) (f32.trunc (local.get 0)))
    (func $f32_nearest (param f32) (result f32) (f32.nearest (local.get 0)))
    (func $f64_ceil (param f64) (result f64) (f64.ceil (local.get 0)))
    (func $f64_floor (param f64) (result f64) (f64.floor (local.get 0)))
    (func $f64_trunc (param f64) (result f64) (f64.trunc (local.get 0)))
    (func $f64_nearest (param f64) (result f64) (f64.nearest (local.get 0)))
    */
    auto wasm = from_hex(
        "0061736d0100000001551060027d7d017d60027c7c017c60017d017d60017c017c60017f017d60017e017d6001"
        "7f017c60017e017c60027d7d017f60027c7c017f60017d017f60017d017e60017c017f60017c017e60017d017c"
        "60017c017d032b2a0001000100010001020304040505060607070809080908090a0a0b0b0c0c0d0d0e0f020202"
        "02030303030a99022a070020002001920b070020002001a00b070020002001930b070020002001a10b07002000"
        "2001940b070020002001a20b070020002001950b070020002001a30b05002000910b050020009f0b05002000b2"
        "0b05002000b30b05002000b40b05002000b50b05002000b70b05002000b80b05002000b90b05002000ba0b0700"
        "200020015b0b070020002001610b0700200020015d0b070020002001630b0700200020015f0b07002000200165"
        "0b05002000a80b05002000a90b05002000ae0b05002000af0b05002000aa0b05002000ab0b05002000b00b0500"
        "2000b10b05002000bb0b05002000b60b050020008d0b050020008e0b050020008f0b05002000900b050020009b"
        "0b050020009c0b050020009d0b050020009e0b");
    return wasm;
}

FunctionDescription from_name(std::string_view name)
{
    static const std::unordered_map<std::string_view, FunctionDescription> map{
        {"f32_add", {0, ValType::f32, ValType::f32, ValType::f32}},
        {"f64_add", {1, ValType::f64, ValType::f64, ValType::f64}},
        {"f32_sub", {2, ValType::f32, ValType::f32, ValType::f32}},
        {"f64_sub", {3, ValType::f64, ValType::f64, ValType::f64}},
        {"f32_mul", {4, ValType::f32, ValType::f32, ValType::f32}},
        {"f64_mul", {5, ValType::f64, ValType::f64, ValType::f64}},
        {"f32_div", {6, ValType::f32, ValType::f32, ValType::f32}},
        {"f64_div", {7, ValType::f64, ValType::f64, ValType::f64}},
        {"f32_sqrt", {8, ValType::f32, ValType::f32}},
        {"f64_sqrt", {9, ValType::f64, ValType::f64}},

        {"i32_to_f32", {10, ValType::f32, ValType::i32}},
        {"ui32_to_f32", {11, ValType::f32, ValType::i32}},
        {"i64_to_f32", {12, ValType::f32, ValType::i64}},
        {"ui64_to_f32", {13, ValType::f32, ValType::i64}},
        {"i32_to_f64", {14, ValType::f64, ValType::i32}},
        {"ui32_to_f64", {15, ValType::f64, ValType::i32}},
        {"i64_to_f64", {16, ValType::f64, ValType::i64}},
        {"ui64_to_f64", {17, ValType::f64, ValType::i64}},

        {"f32_eq", {18, ValType::i32, ValType::f32, ValType::f32}},
        {"f64_eq", {19, ValType::i32, ValType::f64, ValType::f64}},
        {"f32_lt", {20, ValType::i32, ValType::f32, ValType::f32}},
        {"f64_lt", {21, ValType::i32, ValType::f64, ValType::f64}},
        {"f32_le", {22, ValType::i32, ValType::f32, ValType::f32}},
        {"f64_le", {23, ValType::i32, ValType::f64, ValType::f64}},

        // Wasm only supports conversions to integer with truncation rounding direction:
        // testfloat_gen needs -rminMag option.
        // Conversion of a floating-point number to an integer format, when the source is NaN,
        // infinity, or a value that would convert to an integer outside the range of the result
        // format under the applicable rounding attribute.
        {"f32_trunc_to_i32", {24, ValType::i32, ValType::f32, Options::TrapIsInvalidOperation}},
        {"f32_trunc_to_ui32", {25, ValType::i32, ValType::f32, Options::TrapIsInvalidOperation}},
        {"f32_trunc_to_i64", {26, ValType::i64, ValType::f32, Options::TrapIsInvalidOperation}},
        {"f32_trunc_to_ui64", {27, ValType::i64, ValType::f32, Options::TrapIsInvalidOperation}},
        {"f64_trunc_to_i32", {28, ValType::i32, ValType::f64, Options::TrapIsInvalidOperation}},
        {"f64_trunc_to_ui32", {29, ValType::i32, ValType::f64, Options::TrapIsInvalidOperation}},
        {"f64_trunc_to_i64", {30, ValType::i64, ValType::f64, Options::TrapIsInvalidOperation}},
        {"f64_trunc_to_ui64", {31, ValType::i64, ValType::f64, Options::TrapIsInvalidOperation}},

        {"f32_to_f64", {32, ValType::f64, ValType::f32}},
        {"f64_to_f32", {33, ValType::f32, ValType::f64}},

        {"f32_ceil", {34, ValType::f32, ValType::f32}},
        {"f32_floor", {35, ValType::f32, ValType::f32}},
        {"f32_trunc", {36, ValType::f32, ValType::f32}},
        {"f32_nearest", {37, ValType::f32, ValType::f32}},
        {"f64_ceil", {38, ValType::f64, ValType::f64}},
        {"f64_floor", {39, ValType::f64, ValType::f64}},
        {"f64_trunc", {40, ValType::f64, ValType::f64}},
        {"f64_nearest", {41, ValType::f64, ValType::f64}},
    };

    if (const auto it = map.find(name); it != map.end())
        return it->second;
    throw std::invalid_argument{"unknown <function>: " + std::string{name}};
}

/// Creates TypedValue of given type from provided bits.
constexpr TypedValue from_bits(ValType ty, uint64_t bits) noexcept
{
    switch (ty)
    {
    case ValType::i32:
    case ValType::f32:  // For f32 interpret bits as integer value not to convert sNaN -> qNaN.
        return {ty, Value{static_cast<uint32_t>(bits)}};
    case ValType::i64:
    case ValType::f64:  // For f64 interpret bits as integer value not to convert sNaN -> qNaN.
        return {ty, bits};
    }
    __builtin_unreachable();
}

std::ostream& operator<<(std::ostream& os, const TypedValue& value)
{
    std::ios os_state{nullptr};
    os_state.copyfmt(os);
    os << std::hex << std::uppercase << std::setfill('0');
    switch (value.type)
    {
    case ValType::i32:
        os << std::setw(8) << value.value.as<uint32_t>();
        break;
    case ValType::i64:
        os << std::setw(16) << value.value.i64;
        break;
    case ValType::f32:
        os << std::setw(8) << FP{value.value.f32}.as_uint();
        break;
    case ValType::f64:
        os << std::setw(16) << FP{value.value.f64}.as_uint();
        break;
    }
    os.copyfmt(os_state);
    return os;
}

bool eq(TypedValue v, uint64_t expected_bits, bool ignore_nan_payloads)
{
    switch (v.type)
    {
    case ValType::i32:
        return v.value.i32 == expected_bits;
    case ValType::i64:
        return v.value.i64 == expected_bits;
    case ValType::f32:
    {
        const FP fp_value{v.value.f32};
        const FP expected{static_cast<uint32_t>(expected_bits)};
        if (ignore_nan_payloads && expected.is_nan())
        {
            if (expected.is_canonical_nan())
                return fp_value.is_canonical_nan();
            if (expected.is_arithmetic_nan())
                return fp_value.is_arithmetic_nan();
            throw std::invalid_argument{"invalid input: unexpected signaling NaN"};
        }
        return fp_value == expected;
    }

    case ValType::f64:
    {
        const FP fp_value{v.value.f64};
        const FP expected{expected_bits};
        if (ignore_nan_payloads && expected.is_nan())
        {
            if (expected.is_canonical_nan())
                return fp_value.is_canonical_nan();
            if (expected.is_arithmetic_nan())
                return fp_value.is_arithmetic_nan();
            throw std::invalid_argument{"invalid input: unexpected signaling NaN"};
        }
        return fp_value == expected;
    }
    }
    __builtin_unreachable();
}

bool check(const FunctionDescription& func, Instance& instance, const uint64_t inputs[],
    bool ignore_nan_payloads)
{
    const auto report_failure = [&](auto result, auto expected) {
        std::cerr << "FAILURE: " << result << " <-";
        for (size_t i = 0; i < func.num_arguments; ++i)
            std::cerr << ' ' << from_bits(func.param_types[i], inputs[i]);
        std::cerr << "\n         " << expected << " (expected)\n";
    };

    Value args[2]{};
    assert(func.num_arguments <= std::size(args));
    for (size_t i = 0; i < func.num_arguments; ++i)
        args[i] = from_bits(func.param_types[i], inputs[i]).value;

    const auto r = execute(instance, func.idx, args);

    if (func.options == Options::TrapIsInvalidOperation)
    {
        const auto expected_exceptions = inputs[func.num_arguments + 1];
        assert(expected_exceptions == InvalidOperation || expected_exceptions == 0);
        const bool invalid_operation = expected_exceptions == InvalidOperation;

        if (r.trapped == invalid_operation)
            return true;

        report_failure(r.trapped, invalid_operation);
        return false;
    }

    assert(!r.trapped && r.has_value);
    const auto result = TypedValue{func.result_type, r.value};
    const auto expected_result_bits = inputs[func.num_arguments];
    if (!eq(result, expected_result_bits, ignore_nan_payloads))
    {
        report_failure(result, from_bits(func.result_type, expected_result_bits));
        return false;
    }
    return true;
}
}  // namespace

int main(int argc, const char* argv[])
{
    try
    {
        std::string_view function_name;
        int rounding_direction = FE_TONEAREST;
        bool ignore_nan_payloads = false;

        for (int i = 1; i < argc; ++i)
        {
            const std::string_view arg{argv[i]};

            if (!arg.empty() && arg[0] == '-')  // Option?
            {
                if (arg == "-rnear_even")
                    rounding_direction = FE_TONEAREST;
                else if (arg == "-rminMag")
                    rounding_direction = FE_TOWARDZERO;
                else if (arg == "-rmin")
                    rounding_direction = FE_DOWNWARD;
                else if (arg == "-rmax")
                    rounding_direction = FE_UPWARD;
                else if (arg == "-ignore_nan_payloads")
                    ignore_nan_payloads = true;
                else
                    throw std::invalid_argument{"unknown option: " + std::string{arg}};
            }
            else if (function_name.empty())
                function_name = arg;
            else
                throw std::invalid_argument{"unexpected argument: " + std::string{arg}};
        }

        if (function_name.empty())
            throw std::invalid_argument{"missing <function> argument"};

        const auto func = from_name(function_name);

        auto instance = instantiate(parse(get_wasm_code()));

        std::fesetround(rounding_direction);

        // Input format:
        // Values (including exceptions bitfield) are hex-encoded without 0x prefix and with all
        // leading zeros. Input arguments (1 or 2) are followed by expected result and bitfield
        // of expected IEEE exceptions.
        // Examples:
        // f32_add:
        //   015E834A C700FFBF C700FFBF 01
        // f64_to_f32:
        //   B68FFFF8000000FF 80000000 03

        int num_failures = 0;
        std::cin >> std::hex;
        while (num_failures < max_failures)
        {
            uint64_t inputs[4]{};
            for (size_t i = 0; i < func.num_arguments + 2; ++i)
                std::cin >> inputs[i];

            if (std::cin.eof())
                break;

            if (std::cin.fail())
                throw std::invalid_argument{"invalid input format"};

            num_failures += !check(func, *instance, inputs, ignore_nan_payloads);
        }

        return num_failures;
    }
    catch (const std::invalid_argument& error)
    {
        std::cerr << "ERROR: " << error.what() << "\n\n" << USAGE;
        return -1;
    }
}
