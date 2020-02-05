#include "execute.hpp"
#include "parser.hpp"
#include <test/utils/wasm_engine.hpp>
#include <cassert>

namespace fizzy::test
{
class FizzyEngine : public WasmEngine
{
    Instance m_instance;

public:
    void parse(bytes_view input) final;
    std::optional<uint32_t> find_function(std::string_view name) const final;
    void instantiate() final;
    bytes_view get_memory() const final;
    void set_memory(bytes_view memory) final;
    Result execute(FuncIdx func_idx, const std::vector<uint64_t>& args) final;
};

std::unique_ptr<WasmEngine> create_fizzy_engine()
{
    return std::make_unique<FizzyEngine>();
}

void FizzyEngine::parse(bytes_view input)
{
    m_instance.module = fizzy::parse(input);
}

void FizzyEngine::instantiate()
{
    m_instance = fizzy::instantiate(std::move(m_instance.module));
}

bytes_view FizzyEngine::get_memory() const
{
    return {m_instance.memory.data(), m_instance.memory.size()};
}

void FizzyEngine::set_memory(bytes_view memory)
{
    m_instance.memory = memory;
}

std::optional<uint32_t> FizzyEngine::find_function(std::string_view name) const
{
    return fizzy::find_exported_function(m_instance.module, name);
}

WasmEngine::Result FizzyEngine::execute(FuncIdx func_idx, const std::vector<uint64_t>& args)
{
    const auto [trapped, result_stack] = fizzy::execute(m_instance, func_idx, args);
    assert(result_stack.size() <= 1);
    return {trapped, !result_stack.empty() ? result_stack.back() : std::optional<uint64_t>{}};
}
}  // namespace fizzy::test
