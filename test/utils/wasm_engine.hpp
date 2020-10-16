// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "bytes.hpp"
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace fizzy::test
{
/// The stateful representation of a wasm engine for testing purposes.
class WasmEngine
{
public:
    using FuncRef = uintptr_t;

    struct Result
    {
        bool trapped = false;
        std::optional<uint64_t> value;
    };

    virtual ~WasmEngine() noexcept;

    /// Parses input wasm binary. The created module is discarded.
    /// Returns false on parsing error.
    virtual bool parse(bytes_view input) const = 0;

    /// Instantiates the internal module from the wasm binary input (parsing included).
    /// Returns false in case an instance could not be made (e.g. because of parsing error,
    /// instantiation error, etc).
    virtual bool instantiate(bytes_view wasm_binary) = 0;

    /// Finds an exported function in the internal instance.
    /// Requires instantiate().
    virtual std::optional<FuncRef> find_function(
        std::string_view name, std::string_view signature) const = 0;

    /// Initializes the beginning of the instance's memory.
    /// The `memory` must not be empty.
    /// Returns false if no memory is available (exported)
    /// or if `memory` doesn't fit into instance's memory.
    /// Requires instantiate().
    virtual bool init_memory(bytes_view memory) = 0;

    /// Returns the entire memory of the internal instance.
    /// It must return memory index 0 and the size must be a multiple of the page size.
    /// Can return an empty view if no memory is available (exported).
    /// Requires instantiate().
    virtual bytes_view get_memory() const = 0;

    /// Executes the function of the given index.
    /// Requires instantiate().
    virtual Result execute(FuncRef func_ref, const std::vector<uint64_t>& args) = 0;
};

void validate_function_signature(std::string_view signature);

std::unique_ptr<WasmEngine> create_fizzy_engine();
std::unique_ptr<WasmEngine> create_fizzy_c_engine();
std::unique_ptr<WasmEngine> create_wabt_engine();
std::unique_ptr<WasmEngine> create_wasm3_engine();
}  // namespace fizzy::test
