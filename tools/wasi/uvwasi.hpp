// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <wasi_types.h>
#include <memory>

namespace fizzy::wasi
{
/// UVWASI interface.
class UVWASI
{
public:
    virtual ~UVWASI();

    virtual uvwasi_errno_t init(uvwasi_size_t argc, const char** argv) noexcept = 0;

    virtual uvwasi_errno_t proc_exit(uvwasi_exitcode_t exit_code) noexcept = 0;

    virtual uvwasi_errno_t fd_write(uvwasi_fd_t fd, const uvwasi_ciovec_t* iovs,
        uvwasi_size_t iovs_len, uvwasi_size_t* nwritten) noexcept = 0;

    virtual uvwasi_errno_t fd_read(uvwasi_fd_t fd, const uvwasi_iovec_t* iovs,
        uvwasi_size_t iovs_len, uvwasi_size_t* nread) noexcept = 0;

    virtual uvwasi_errno_t fd_prestat_get(uvwasi_fd_t fd, uvwasi_prestat_t* buf) noexcept = 0;

    virtual uvwasi_errno_t environ_sizes_get(
        uvwasi_size_t* environ_count, uvwasi_size_t* environ_buf_size) noexcept = 0;
};

std::unique_ptr<UVWASI> create_uvwasi();

}  // namespace fizzy::wasi
