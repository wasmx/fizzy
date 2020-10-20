// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "asserts.hpp"
#include "execute.hpp"
#include <gtest/gtest.h>

using namespace fizzy;

#if !defined(NDEBUG) || __has_feature(undefined_behavior_sanitizer)
TEST(execute_death, malformed_instruction_opcode)
{
    constexpr auto malformed_opcode = static_cast<Instr>(6);

    Code code;
    code.instructions.emplace_back(malformed_opcode);
    code.instructions.emplace_back(Instr::end);

    auto module = std::make_unique<Module>();
    module->typesec.emplace_back(FuncType{});
    module->funcsec.emplace_back(TypeIdx{0});
    module->codesec.emplace_back(std::move(code));

    auto instance = instantiate(std::move(module));
    EXPECT_DEATH(execute(*instance, 0, nullptr), "unreachable");
}
#endif
