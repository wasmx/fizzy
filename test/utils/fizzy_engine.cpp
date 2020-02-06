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
    bool parse(bytes_view input) final;
    std::optional<FuncRef> find_function(std::string_view name) const final;
    bool instantiate() final;
    bytes_view get_memory() const final;
    void set_memory(bytes_view memory) final;
    Result execute(FuncRef func_ref, const std::vector<uint64_t>& args) final;
};

std::unique_ptr<WasmEngine> create_fizzy_engine()
{
    return std::make_unique<FizzyEngine>();
}

bool FizzyEngine::parse(bytes_view input)
{
    try
    {
        m_instance.module = fizzy::parse(input);
    }
    catch (const fizzy::parser_error&)
    {
        return false;
    }
    return true;
}

bool FizzyEngine::instantiate()
{
    try
    {
        m_instance = fizzy::instantiate(std::move(m_instance.module));
    }
    catch (const fizzy::instantiate_error&)
    {
        return false;
    }
    return true;
}

bytes_view FizzyEngine::get_memory() const
{
    return {m_instance.memory.data(), m_instance.memory.size()};
}

void FizzyEngine::set_memory(bytes_view memory)
{
    m_instance.memory = memory;
}

std::optional<WasmEngine::FuncRef> FizzyEngine::find_function(std::string_view name) const
{
    return fizzy::find_exported_function(m_instance.module, name);
}

WasmEngine::Result FizzyEngine::execute(
    WasmEngine::FuncRef func_ref, const std::vector<uint64_t>& args)
{
    const auto [trapped, result_stack] =
        fizzy::execute(m_instance, static_cast<uint32_t>(func_ref), args);
    assert(result_stack.size() <= 1);
    return {trapped, !result_stack.empty() ? result_stack.back() : std::optional<uint64_t>{}};
}
}  // namespace fizzy::test
