#include <src/binary-reader.h>
#include <src/interp/binary-reader-interp.h>
#include <src/interp/interp.h>

#include <test/utils/wasm_engine.hpp>

namespace fizzy::test
{
class WabtEngine : public WasmEngine
{
    wabt::interp::Environment m_env;
    wabt::interp::DefinedModule* m_module{nullptr};

public:
    bool parse(bytes_view input) final;
    std::optional<FuncRef> find_function(std::string_view name) const final;
    bool instantiate() final;
    bool init_memory(fizzy::bytes_view memory) final;
    bytes_view get_memory() const final;
    Result execute(FuncRef func_ref, const std::vector<uint64_t>& args) final;
};

std::unique_ptr<WasmEngine> create_wabt_engine()
{
    return std::make_unique<WabtEngine>();
}

bool WabtEngine::parse(bytes_view input)
{
    wabt::Errors errors;
    wabt::Result result = wabt::ReadBinaryInterp(
        &m_env, input.data(), input.size(), wabt::ReadBinaryOptions{}, &errors, &m_module);
    return (result == wabt::Result::Ok);
}

bool WabtEngine::instantiate()
{
    return true;
}

bool WabtEngine::init_memory(bytes_view memory)
{
    if (m_env.GetMemoryCount() == 0)
        return false;

    auto& dst = *m_env.GetMemory(0);
    if (dst.data.size() < memory.size())
        return false;

    std::memcpy(dst.data.data(), memory.data(), memory.size());
    return true;
}

bytes_view WabtEngine::get_memory() const
{
    if (m_env.GetMemoryCount() == 0)
        return {};

    auto& env = const_cast<wabt::interp::Environment&>(m_env);
    const auto& memory = env.GetMemory(0);
    return {reinterpret_cast<uint8_t*>(memory->data.data()), memory->data.size()};
}

std::optional<WasmEngine::FuncRef> WabtEngine::find_function(std::string_view name) const
{
    const wabt::interp::Export* e = m_module->GetExport({name.data(), name.size()});
    if (e != nullptr && e->kind == wabt::ExternalKind::Func)
        return reinterpret_cast<WasmEngine::FuncRef>(e);
    return {};
}

WasmEngine::Result WabtEngine::execute(
    WasmEngine::FuncRef func_ref, const std::vector<uint64_t>& args)
{
    // WABT Executor/Thread will init both value and call stack vectors with zeros up front.
    // To mitigate the performance overhead, we set sizes to much lower values
    // than default ones.
    wabt::interp::Thread::Options thread_options;
    thread_options.value_stack_size = 1024;  // Default: 32k.
    thread_options.call_stack_size = 1024;   // Default: 64k.
    wabt::interp::Executor executor{&m_env, nullptr, thread_options};

    const auto* e = reinterpret_cast<const wabt::interp::Export*>(func_ref);

    wabt::interp::TypedValues typed_args;
    for (const auto arg : args)
    {
        wabt::interp::Value v{};
        v.i32 = static_cast<uint32_t>(arg);
        typed_args.push_back(wabt::interp::TypedValue{wabt::Type::I32, v});
    }
    wabt::interp::ExecResult r = executor.RunExport(e, typed_args);

    if (r.result != wabt::interp::Result::Ok)
        return {true, {}};

    WasmEngine::Result result{false, {}};
    if (!r.values.empty())
    {
        const auto& value = r.values[0];
        if (value.type == wabt::Type::I32)
            result.value = value.get_i32();
        else if (value.type == wabt::Type::I64)
            result.value = value.get_i64();
    }
    return result;
}
}  // namespace fizzy::test
