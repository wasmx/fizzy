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
    struct Result
    {
        bool trapped = false;
        std::optional<uint64_t> value;
    };

    virtual ~WasmEngine() noexcept = default;

    /// Parses input bytes and creates internal wasm module out of it.
    /// Consecutive invocations replace the internal module and invalidate the internal instance.
    virtual void parse(bytes_view input) = 0;

    /// Finds an exported function in the internal instance.
    /// Requires parse().
    virtual std::optional<FuncIdx> find_function(std::string_view name) const = 0;

    /// Instantiates the internal module creating an internal instance.
    /// Consecutive invocations replace the internal instance with a new one.
    /// Requires parse().
    virtual void instantiate() = 0;

    /// Returns the memory of the internal instance.
    /// Requires instantiate().
    virtual bytes_view get_memory() const = 0;

    /// Replaces the memory of the internal instance with the provided one.
    /// Requires instantiate().
    virtual void set_memory(bytes_view memory) = 0;

    /// Executes the function of the given index.
    /// Requires instantiate().
    virtual Result execute(FuncIdx func_idx, const std::vector<uint64_t>& args) = 0;
};
}  // namespace fizzy::test
