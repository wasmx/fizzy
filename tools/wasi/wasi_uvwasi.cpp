// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "wasi_uvwasi.hpp"
#include <uvwasi.h>
#include <vector>

namespace fizzy::wasi
{
namespace
{
class UVWASI final : public WASI
{
    /// UVWASI state.
    uvwasi_t m_state{};

public:
    UVWASI() = default;
    UVWASI(const UVWASI&) = delete;
    UVWASI& operator=(const UVWASI&) = delete;

    ~UVWASI() noexcept final { uvwasi_destroy(&m_state); }

    uvwasi_errno_t init(int argc, const char* argv[]) noexcept final
    {
        // Initialisation settings.
        // TODO: Make const after https://github.com/nodejs/uvwasi/pull/155 is released.
        uvwasi_options_t options = {
            3,           // sizeof fd_table
            0, nullptr,  // NOTE: no remappings
            static_cast<uvwasi_size_t>(argc), argv,
            nullptr,  // TODO: support env
            0, 1, 2,
            nullptr,  // NOTE: no special allocator
        };

        return uvwasi_init(&m_state, &options);
    }

    error_code fd_write(uint8_t* memory, uint32_t fd, uint32_t iov_ptr, uint32_t iov_cnt,
        uint32_t nwritten_ptr) noexcept
    {
        std::vector<uvwasi_ciovec_t> iovs(iov_cnt);
        // TODO: not sure what to pass as end, passing memory size...
        uvwasi_errno_t ret = uvwasi_serdes_readv_ciovec_t(
            instance.memory->data(), instance.memory->size(), iov_ptr, iovs.data(), iov_cnt);
        if (ret != UVWASI_ESUCCESS)
            return Value{uint32_t{ret}};

        uvwasi_size_t nwritten;
        ret =
            uvwasi_fd_write(&state, static_cast<uvwasi_fd_t>(fd), iovs.data(), iov_cnt, &nwritten);
        uvwasi_serdes_write_uint32_t(instance.memory->data(), nwritten_ptr, nwritten);

        return Value{uint32_t{ret}};
    }

    error_code proc_exit(uint32_t exit_code) noexcept final
    {
        return uvwasi_proc_exit(&m_state, exit_code);
    }
};
}  // namespace

std::unique_ptr<WASI> create_uvwasi()
{
    return std::make_unique<UVWASI>();
}
}  // namespace fizzy::wasi
