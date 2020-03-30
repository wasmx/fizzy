#include "execute.hpp"
#include "parser.hpp"

#include <test/utils/wasm_engine.hpp>
#include <cassert>
#include <cstring>

namespace fizzy::test
{
class FizzyEngine : public WasmEngine
{
    std::unique_ptr<Instance> m_instance;

public:
    bool parse(bytes_view input) final;
    std::optional<FuncRef> find_function(std::string_view name) const final;
    bool instantiate() final;
    bool init_memory(bytes_view memory) final;
    bytes_view get_memory() const final;
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
        auto module = fizzy::parse(input);
        m_instance =
            std::make_unique<Instance>(std::move(module), bytes_ptr{nullptr, [](bytes*) {}}, 0,
                table_ptr{nullptr, [](table_elements*) {}}, std::vector<uint64_t>{},
                std::vector<ExternalFunction>{}, std::vector<ExternalGlobal>{});
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
        m_instance = fizzy::instantiate(std::move(m_instance->module));
    }
    catch (const fizzy::instantiate_error&)
    {
        return false;
    }
    return true;
}

bool FizzyEngine::init_memory(bytes_view memory)
{
    if (m_instance->memory == nullptr || m_instance->memory->size() < memory.size())
        return false;

    std::memcpy(m_instance->memory->data(), memory.data(), memory.size());
    return true;
}

bytes_view FizzyEngine::get_memory() const
{
    if (!m_instance->memory)
        return {};

    return {m_instance->memory->data(), m_instance->memory->size()};
}

std::optional<WasmEngine::FuncRef> FizzyEngine::find_function(std::string_view name) const
{
    return fizzy::find_exported_function(m_instance->module, name);
}

WasmEngine::Result FizzyEngine::execute(
    WasmEngine::FuncRef func_ref, const std::vector<uint64_t>& args)
{
    const auto [trapped, result_stack] =
        fizzy::execute(*m_instance, static_cast<uint32_t>(func_ref), args);
    assert(result_stack.size() <= 1);
    return {trapped, !result_stack.empty() ? result_stack.back() : std::optional<uint64_t>{}};
}
}  // namespace fizzy::test
