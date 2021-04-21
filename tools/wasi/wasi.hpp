// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <bytes.hpp>
#include <iosfwd>
#include <optional>

namespace fizzy::wasi
{
/// WASI error code.
/// @todo Make it [[nodiscard]].
using error_code = uint16_t;

/// WASI interface.
class WASI
{
public:
    virtual ~WASI();

    virtual error_code init(int argc, const char* argv[]) noexcept = 0;

    virtual error_code return_enosys() noexcept = 0;

    virtual error_code proc_exit(uint32_t exit_code) noexcept = 0;

    virtual error_code fd_write(bytes& memory, uint32_t fd, uint32_t iov_ptr, uint32_t iov_cnt,
        uint32_t nwritten_ptr) noexcept = 0;

    virtual error_code fd_read(bytes& memory, uint32_t fd, uint32_t iov_ptr, uint32_t iov_cnt,
        uint32_t nread_ptr) noexcept = 0;

    virtual error_code fd_prestat_get(
        bytes& memory, uint32_t fd, uint32_t prestat_ptr) noexcept = 0;

    virtual error_code environ_sizes_get(
        bytes& memory, uint32_t environc, uint32_t environ_buf_size) noexcept = 0;
};

/// Loads a binary file at the given path.
///
/// @param  file    Path to the file.
/// @param  err     Error output stream.
/// @return         Content of the loaded file.
std::optional<bytes> load_file(std::string_view file, std::ostream& err) noexcept;

/// Loads the wasm file (path in first CLI argument) and
/// executes WASI main function with given CLI arguments.
///
/// @param  argc    Number of CLI arguments. Must be >= 1.
/// @param  argv    Array of CLI arguments. The first argument must be wasm file path.
/// @param  err     Error output stream.
/// @return         False in case of error.
///
/// @todo Make noexcept.
bool load_and_run(int argc, const char** argv, std::ostream& err);

/// Executes WASI main function from the wasm binary with given CLI arguments.
///
/// @param  wasm_binary    Wasm binary.
/// @param  argc           Number of CLI arguments.
/// @param  argv           Array of CLI arguments. The first argument should be wasm file path.
/// @param  err            Error output stream.
/// @return                False in case of error.
bool run(bytes_view wasm_binary, int argc, const char* argv[], std::ostream& err);
}  // namespace fizzy::wasi
