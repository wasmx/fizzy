// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "wasi.hpp"
#include "execute.hpp"
#include "limits.hpp"
#include "parser.hpp"
#include <uvwasi.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <ostream>

namespace fizzy::wasi
{
namespace
{
// Global state.
// NOTE: we do not care about uvwasi_destroy(), because it only clears file descriptors (currently)
// and we are a single-run tool. This may change in the future and should reevaluate.
uvwasi_t state;

ExecutionResult return_enosys(Instance&, const Value*, int)
{
    return Value{uint32_t{UVWASI_ENOSYS}};
}

ExecutionResult proc_exit(Instance&, const Value* args, int)
{
    uvwasi_proc_exit(&state, static_cast<uvwasi_exitcode_t>(args[0].as<uint32_t>()));
    // Should not reach this.
    return Trap;
}

ExecutionResult fd_write(Instance& instance, const Value* args, int)
{
    const auto fd = args[0].as<uint32_t>();
    const auto iov_ptr = args[1].as<uint32_t>();
    const auto iov_cnt = args[2].as<uint32_t>();
    const auto nwritten_ptr = args[3].as<uint32_t>();

    std::vector<uvwasi_ciovec_t> iovs(iov_cnt);
    // TODO: not sure what to pass as end, passing memory size...
    uvwasi_errno_t ret = uvwasi_serdes_readv_ciovec_t(
        instance.memory->data(), instance.memory->size(), iov_ptr, iovs.data(), iov_cnt);
    if (ret != UVWASI_ESUCCESS)
        return Value{uint32_t{ret}};

    uvwasi_size_t nwritten;
    ret = uvwasi_fd_write(&state, static_cast<uvwasi_fd_t>(fd), iovs.data(), iov_cnt, &nwritten);
    uvwasi_serdes_write_uint32_t(instance.memory->data(), nwritten_ptr, nwritten);

    return Value{uint32_t{ret}};
}

ExecutionResult fd_read(Instance& instance, const Value* args, int)
{
    const auto fd = args[0].as<uint32_t>();
    const auto iov_ptr = args[1].as<uint32_t>();
    const auto iov_cnt = args[2].as<uint32_t>();
    const auto nread_ptr = args[3].as<uint32_t>();

    std::vector<uvwasi_iovec_t> iovs(iov_cnt);
    // TODO: not sure what to pass as end, passing memory size...
    uvwasi_errno_t ret = uvwasi_serdes_readv_iovec_t(
        instance.memory->data(), instance.memory->size(), iov_ptr, iovs.data(), iov_cnt);
    if (ret != UVWASI_ESUCCESS)
        return Value{uint32_t{ret}};

    uvwasi_size_t nread;
    ret = uvwasi_fd_read(&state, static_cast<uvwasi_fd_t>(fd), iovs.data(), iov_cnt, &nread);
    uvwasi_serdes_write_uint32_t(instance.memory->data(), nread_ptr, nread);

    return Value{uint32_t{ret}};
}

ExecutionResult fd_prestat_get(Instance& instance, const Value* args, int)
{
    const auto fd = args[0].as<uint32_t>();
    const auto prestat_ptr = args[1].as<uint32_t>();

    uvwasi_prestat_t buf;
    uvwasi_errno_t ret = uvwasi_fd_prestat_get(&state, fd, &buf);

    uvwasi_serdes_write_prestat_t(instance.memory->data(), prestat_ptr, &buf);

    return Value{uint32_t{ret}};
}

ExecutionResult environ_sizes_get(Instance& instance, const Value* args, int)
{
    const auto environc = args[0].as<uint32_t>();
    const auto environ_buf_size = args[1].as<uint32_t>();
    // TODO: implement properly (only returns 0 now)
    uvwasi_serdes_write_uint32_t(instance.memory->data(), environc, 0);
    uvwasi_serdes_write_uint32_t(instance.memory->data(), environ_buf_size, 0);
    return Value{uint32_t{UVWASI_ESUCCESS}};
}
}  // namespace

std::optional<bytes> load_file(std::string_view file, std::ostream& err) noexcept
{
    try
    {
        std::filesystem::path path{file};

        if (!std::filesystem::exists(path))
        {
            err << "File does not exist: " << path << "\n";
            return std::nullopt;
        }
        if (!std::filesystem::is_regular_file(path))
        {
            err << "Not a file: " << path << "\n";
            return std::nullopt;
        }

        std::ifstream wasm_file{path};
        if (!wasm_file)
        {
            err << "Failed to open file: " << path << "\n";
            return std::nullopt;
        }
        return bytes(std::istreambuf_iterator<char>{wasm_file}, std::istreambuf_iterator<char>{});
    }
    catch (...)
    {
        // Generic error message due to other exceptions.
        err << "Failed to load: " << file << "\n";
        return std::nullopt;
    }
}

bool run(int argc, const char** argv, std::ostream& err)
{
    constexpr auto ns = "wasi_snapshot_preview1";
    const std::vector<ImportedFunction> wasi_functions = {
        {ns, "proc_exit", {ValType::i32}, std::nullopt, proc_exit},
        {ns, "fd_read", {ValType::i32, ValType::i32, ValType::i32, ValType::i32}, ValType::i32,
            fd_read},
        {ns, "fd_write", {ValType::i32, ValType::i32, ValType::i32, ValType::i32}, ValType::i32,
            fd_write},
        {ns, "fd_prestat_get", {ValType::i32, ValType::i32}, ValType::i32, fd_prestat_get},
        {ns, "fd_prestat_dir_name", {ValType::i32, ValType::i32, ValType::i32}, ValType::i32,
            return_enosys},
        {ns, "environ_sizes_get", {ValType::i32, ValType::i32}, ValType::i32, environ_sizes_get},
        {ns, "environ_get", {ValType::i32, ValType::i32}, ValType::i32, return_enosys},
    };

    // Initialisation settings.
    // TODO: Make const after https://github.com/nodejs/uvwasi/pull/155 is merged.
    uvwasi_options_t options = {
        3,           // sizeof fd_table
        0, nullptr,  // NOTE: no remappings
        static_cast<uvwasi_size_t>(argc), argv,
        nullptr,  // TODO: support env
        0, 1, 2,
        nullptr,  // NOTE: no special allocator
    };

    const uvwasi_errno_t uvwasi_err = uvwasi_init(&state, &options);
    if (uvwasi_err != UVWASI_ESUCCESS)
    {
        err << "Failed to initialise UVWASI: " << uvwasi_embedder_err_code_to_string(uvwasi_err)
            << "\n";
        return false;
    }

    const auto wasm_binary = load_wasm_binary(argv[0], err);
    if (wasm_binary.empty())
        return false;

    auto module = parse(wasm_binary);
    auto imports = resolve_imported_functions(*module, wasi_functions);
    auto instance =
        instantiate(std::move(module), std::move(imports), {}, {}, {}, MaxMemoryPagesLimit);
    assert(instance != nullptr);

    const auto start_function = find_exported_function(*instance->module, "_start");
    if (!start_function.has_value())
    {
        err << "File is not WASI compatible (_start not found)\n";
        return false;
    }

    // Manually validate type signature here
    // TODO: do this in find_exported_function
    if (instance->module->get_function_type(*start_function) != FuncType{})
    {
        err << "File is not WASI compatible (_start has invalid signature)\n";
        return false;
    }

    if (!find_exported_memory(*instance, "memory").has_value())
    {
        err << "File is not WASI compatible (no memory exported)\n";
        return false;
    }

    const auto result = execute(*instance, *start_function, {});
    if (result.trapped)
    {
        err << "Execution aborted with WebAssembly trap\n";
        return false;
    }
    assert(!result.has_value);

    return true;
}
}  // namespace fizzy::wasi
