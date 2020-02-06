#pragma once

#include "types.hpp"
#include <memory>
#include <optional>

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

    virtual ~WasmEngine() noexcept = default;

    /// Parses input bytes and creates internal wasm module out of it.
    /// Returns false on parsing error.
    /// Consecutive invocations replace the internal module and invalidate the internal instance.
    virtual bool parse(bytes_view input) = 0;

    /// Finds an exported function in the internal instance.
    /// Requires parse().
    virtual std::optional<FuncRef> find_function(std::string_view name) const = 0;

    /// Instantiates the internal module creating an internal instance.
    /// Consecutive invocations replace the internal instance with a new one.
    /// Requires parse().
    virtual void instantiate() = 0;

    /// Returns the entire memory of the internal instance.
    /// It must return memory index 0 and the size must be a multiple of the page size.
    /// Can return an empty view if no memory is available (exported).
    /// Requires instantiate().
    virtual bytes_view get_memory() const = 0;

    /// Replaces the memory of the internal instance with the provided one.
    /// It must change memory index 0.
    /// Should not fail if the input is empty (no update) and there is no memory available.
    /// Requires instantiate().
    virtual void set_memory(bytes_view memory) = 0;

    /// Executes the function of the given index.
    /// Requires instantiate().
    virtual Result execute(FuncRef func_ref, const std::vector<uint64_t>& args) = 0;
};

std::unique_ptr<WasmEngine> create_fizzy_engine();
}  // namespace fizzy::test
