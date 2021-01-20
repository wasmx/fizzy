// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "limits.hpp"
#include "parser.hpp"
#include <uvwasi.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
// Global state.
// NOTE: we do not care about uvwasi_destroy(), because it only clears file descriptors (currently)
// and we are a single-run tool. This may change in the future and should reevaluate.
uvwasi_t state;

fizzy::ExecutionResult wasi_return_enosys(fizzy::Instance&, const fizzy::Value*, int)
{
    return fizzy::Value{uint32_t{UVWASI_ENOSYS}};
}

fizzy::ExecutionResult wasi_proc_exit(fizzy::Instance&, const fizzy::Value* args, int)
{
    uvwasi_proc_exit(&state, static_cast<uvwasi_exitcode_t>(args[0].as<uint32_t>()));
    // Should not reach this.
    return fizzy::Trap;
}

fizzy::ExecutionResult wasi_fd_write(fizzy::Instance& instance, const fizzy::Value* args, int)
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
        return fizzy::Value{uint32_t{ret}};

    uvwasi_size_t nwritten;
    ret = uvwasi_fd_write(&state, static_cast<uvwasi_fd_t>(fd), iovs.data(), iov_cnt, &nwritten);
    uvwasi_serdes_write_uint32_t(instance.memory->data(), nwritten_ptr, nwritten);

    return fizzy::Value{uint32_t{ret}};
}

fizzy::ExecutionResult wasi_fd_read(fizzy::Instance& instance, const fizzy::Value* args, int)
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
        return fizzy::Value{uint32_t{ret}};

    uvwasi_size_t nread;
    ret = uvwasi_fd_read(&state, static_cast<uvwasi_fd_t>(fd), iovs.data(), iov_cnt, &nread);
    uvwasi_serdes_write_uint32_t(instance.memory->data(), nread_ptr, nread);

    return fizzy::Value{uint32_t{ret}};
}

fizzy::ExecutionResult wasi_fd_prestat_get(fizzy::Instance& instance, const fizzy::Value* args, int)
{
    const auto fd = args[0].as<uint32_t>();
    const auto prestat_ptr = args[1].as<uint32_t>();

    uvwasi_prestat_t buf;
    uvwasi_errno_t ret = uvwasi_fd_prestat_get(&state, fd, &buf);

    uvwasi_serdes_write_prestat_t(instance.memory->data(), prestat_ptr, &buf);

    return fizzy::Value{uint32_t{ret}};
}

fizzy::ExecutionResult wasi_environ_sizes_get(
    fizzy::Instance& instance, const fizzy::Value* args, int)
{
    const auto environc = args[0].as<uint32_t>();
    const auto environ_buf_size = args[1].as<uint32_t>();
    // TODO: implement properly (only returns 0 now)
    uvwasi_serdes_write_uint32_t(instance.memory->data(), environc, 0);
    uvwasi_serdes_write_uint32_t(instance.memory->data(), environ_buf_size, 0);
    return fizzy::Value{uint32_t{UVWASI_ESUCCESS}};
}

bool run(int argc, const char** argv)
{
    const std::vector<fizzy::ImportedFunction> wasi_functions = {
        {"wasi_snapshot_preview1", "proc_exit", {fizzy::ValType::i32}, std::nullopt,
            wasi_proc_exit},
        {"wasi_snapshot_preview1", "fd_read",
            {fizzy::ValType::i32, fizzy::ValType::i32, fizzy::ValType::i32, fizzy::ValType::i32},
            fizzy::ValType::i32, wasi_fd_read},
        {"wasi_snapshot_preview1", "fd_write",
            {fizzy::ValType::i32, fizzy::ValType::i32, fizzy::ValType::i32, fizzy::ValType::i32},
            fizzy::ValType::i32, wasi_fd_write},
        {"wasi_snapshot_preview1", "fd_prestat_get", {fizzy::ValType::i32, fizzy::ValType::i32},
            fizzy::ValType::i32, wasi_fd_prestat_get},
        {"wasi_snapshot_preview1", "fd_prestat_dir_name",
            {fizzy::ValType::i32, fizzy::ValType::i32, fizzy::ValType::i32}, fizzy::ValType::i32,
            wasi_return_enosys},
        {"wasi_snapshot_preview1", "environ_sizes_get", {fizzy::ValType::i32, fizzy::ValType::i32},
            fizzy::ValType::i32, wasi_environ_sizes_get},
        {"wasi_snapshot_preview1", "environ_get", {fizzy::ValType::i32, fizzy::ValType::i32},
            fizzy::ValType::i32, wasi_return_enosys},
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

    const uvwasi_errno_t err = uvwasi_init(&state, &options);
    if (err != UVWASI_ESUCCESS)
    {
        std::cerr << "Failed to initialise UVWASI: " << uvwasi_embedder_err_code_to_string(err)
                  << "\n";
        return false;
    }

    if (!std::filesystem::is_regular_file(argv[0]))
    {
        std::cerr << "File does not exists or is irregular: " << argv[0] << "\n";
        return false;
    }

    std::ifstream wasm_file{argv[0]};
    if (!wasm_file)
    {
        std::cerr << "Failed to open file: " << argv[0] << "\n";
        return false;
    }
    const auto wasm_binary =
        fizzy::bytes(std::istreambuf_iterator<char>{wasm_file}, std::istreambuf_iterator<char>{});

    auto module = fizzy::parse(wasm_binary);
    auto imports = fizzy::resolve_imported_functions(*module, wasi_functions);
    auto instance = fizzy::instantiate(
        std::move(module), std::move(imports), {}, {}, {}, fizzy::MemoryPagesValidationLimit);
    assert(instance != nullptr);

    const auto start_function = fizzy::find_exported_function(*instance->module, "_start");
    if (!start_function.has_value())
    {
        std::cerr << "File is not WASI compatible (_start not found)\n";
        return false;
    }

    // Manually validate type signature here
    // TODO: do this in find_exported_function
    if (instance->module->get_function_type(*start_function) != fizzy::FuncType{})
    {
        std::cerr << "File is not WASI compatible (_start has invalid signature)\n";
        return false;
    }

    if (!fizzy::find_exported_memory(*instance, "memory").has_value())
    {
        std::cerr << "File is not WASI compatible (no memory exported)\n";
        return false;
    }

    const auto result = fizzy::execute(*instance, *start_function, {});
    if (result.trapped)
    {
        std::cerr << "Execution aborted with WebAssembly trap\n";
        return false;
    }
    assert(!result.has_value);

    return true;
}
}  // namespace

int main(int argc, const char** argv)
{
    try
    {
        if (argc < 2)
        {
            std::cerr << "Missing executable argument\n";
            return -1;
        }

        // Remove fizzy-wasi from the argv, but keep "argv[1]"
        const bool res = run(argc - 1, argv + 1);
        return res ? 0 : 1;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Exception: " << ex.what() << "\n";
        return -2;
    }
}
