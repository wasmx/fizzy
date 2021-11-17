// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "uvwasi.hpp"
#include <uvwasi.h>

namespace fizzy::wasi
{
namespace
{
class UVWASIImpl final : public UVWASI
{
    /// UVWASI state.
    uvwasi_t m_state{};

public:
    ~UVWASIImpl() final { uvwasi_destroy(&m_state); }

    uvwasi_errno_t init(uvwasi_size_t argc, const char** argv) noexcept final
    {
        uvwasi_destroy(&m_state);

        // Initialisation settings.
        const uvwasi_options_t options = {
            3,           // sizeof fd_table
            0, nullptr,  // NOTE: no remappings
            argc, argv,
            nullptr,  // TODO: support env
            0, 1, 2,
            nullptr,  // NOTE: no special allocator
        };
        return uvwasi_init(&m_state, &options);
    }

    uvwasi_errno_t proc_exit(uvwasi_exitcode_t exit_code) noexcept final
    {
        return uvwasi_proc_exit(&m_state, exit_code);
    }

    uvwasi_errno_t fd_write(uvwasi_fd_t fd, const uvwasi_ciovec_t* iovs, uvwasi_size_t iovs_len,
        uvwasi_size_t* nwritten) noexcept final
    {
        return uvwasi_fd_write(&m_state, fd, iovs, iovs_len, nwritten);
    }

    uvwasi_errno_t fd_read(uvwasi_fd_t fd, const uvwasi_iovec_t* iovs, uvwasi_size_t iovs_len,
        uvwasi_size_t* nread) noexcept final
    {
        return uvwasi_fd_read(&m_state, fd, iovs, iovs_len, nread);
    }

    uvwasi_errno_t fd_prestat_get(uvwasi_fd_t fd, uvwasi_prestat_t* buf) noexcept final
    {
        return uvwasi_fd_prestat_get(&m_state, fd, buf);
    }

    uvwasi_errno_t environ_sizes_get(
        uvwasi_size_t* environ_count, uvwasi_size_t* environ_buf_size) noexcept final
    {
        return uvwasi_environ_sizes_get(&m_state, environ_count, environ_buf_size);
    }
};
}  // namespace

UVWASI::~UVWASI() {}

std::unique_ptr<UVWASI> create_uvwasi()
{
    return std::make_unique<UVWASIImpl>();
}
}  // namespace fizzy::wasi
