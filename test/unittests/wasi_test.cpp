// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "instantiate.hpp"
#include "uvwasi.hpp"
#include "wasi.hpp"
#include <gmock/gmock.h>
#include <test/utils/hex.hpp>
#include <sstream>

using namespace fizzy;
using namespace fizzy::test;
using testing::ElementsAre;
using testing::Test;

namespace
{
class MockUVWASI final : public wasi::UVWASI
{
public:
    bool init_called = false;
    std::optional<uvwasi_exitcode_t> exit_code;
    std::optional<uvwasi_fd_t> write_fd;
    std::vector<bytes> write_data;

    uvwasi_errno_t init(uvwasi_size_t /*argc*/, const char** /*argv*/) noexcept final
    {
        init_called = true;
        return UVWASI_ESUCCESS;
    }

    uvwasi_errno_t proc_exit(uvwasi_exitcode_t code) noexcept final
    {
        exit_code = code;
        return UVWASI_ESUCCESS;
    }

    uvwasi_errno_t fd_write(uvwasi_fd_t fd, const uvwasi_ciovec_t* iovs, uvwasi_size_t iovs_len,
        uvwasi_size_t* nwritten) noexcept final
    {
        write_fd = fd;

        uvwasi_size_t total_len = 0;
        for (uvwasi_size_t i = 0; i < iovs_len; ++i)
        {
            const auto* data = static_cast<const uint8_t*>(iovs[i].buf);
            write_data.emplace_back(data, data + iovs[i].buf_len);
            total_len += iovs[i].buf_len;
        }

        *nwritten = total_len;
        return UVWASI_ESUCCESS;
    }

    uvwasi_errno_t fd_read(uvwasi_fd_t /*fd*/, const uvwasi_iovec_t* /*iovs*/,
        uvwasi_size_t /*iovs_len*/, uvwasi_size_t* /*nread*/) noexcept final
    {
        return UVWASI_ESUCCESS;
    }

    uvwasi_errno_t fd_prestat_get(uvwasi_fd_t /*fd*/, uvwasi_prestat_t* /*buf*/) noexcept final
    {
        return UVWASI_ESUCCESS;
    }

    uvwasi_errno_t environ_sizes_get(
        uvwasi_size_t* /*environ_count*/, uvwasi_size_t* /*environ_buf_size*/) noexcept final
    {
        return UVWASI_ESUCCESS;
    }
};

class wasi_mocked_test : public Test
{
public:
    std::unique_ptr<MockUVWASI> mock_uvwasi = std::make_unique<MockUVWASI>();
};
}  // namespace

TEST(wasi, destroy_non_inited_uvwasi)
{
    auto uvwasi = wasi::create_uvwasi();
}

TEST(wasi, init)
{
    const char* args[]{"ABC"};

    auto uvwasi = wasi::create_uvwasi();
    EXPECT_EQ(uvwasi->init(std::size(args), args), UVWASI_ESUCCESS);
}

TEST(wasi, init_multiple)
{
    auto uvwasi = wasi::create_uvwasi();

    const char* args1[]{"ABC"};
    EXPECT_EQ(uvwasi->init(std::size(args1), args1), UVWASI_ESUCCESS);

    const char* args2[]{"DEF"};
    EXPECT_EQ(uvwasi->init(std::size(args2), args2), UVWASI_ESUCCESS);
}

TEST(wasi, no_file)
{
    const char* args[]{"ABC"};

    std::ostringstream err;
    EXPECT_FALSE(wasi::load_and_run(std::size(args), args, err));
    EXPECT_EQ(err.str(), "File does not exist: \"ABC\"\n");
}

TEST_F(wasi_mocked_test, proc_exit)
{
    /* wat2wasm
      (func (import "wasi_snapshot_preview1" "proc_exit") (param i32))
      (memory (export "memory") 1)
      (func (export "_start") (call 0 (i32.const 22)))
    */
    const auto wasm = from_hex(
        "0061736d0100000001080260017f0060000002240116776173695f736e617073686f745f707265766965773109"
        "70726f635f657869740000030201010503010001071302066d656d6f72790200065f737461727400010a080106"
        "00411610000b");

    auto instance = wasi::instantiate(*mock_uvwasi, wasm);

    EXPECT_FALSE(mock_uvwasi->init_called);
    EXPECT_FALSE(mock_uvwasi->exit_code.has_value());

    std::ostringstream err;
    EXPECT_FALSE(wasi::run(*mock_uvwasi, *instance, 0, nullptr, err));
    EXPECT_EQ(err.str(), "Execution aborted with WebAssembly trap\n");

    EXPECT_TRUE(mock_uvwasi->init_called);
    ASSERT_TRUE(mock_uvwasi->exit_code.has_value());
    EXPECT_EQ(*mock_uvwasi->exit_code, 22);
}

TEST_F(wasi_mocked_test, fd_write)
{
    /* wat2wasm
      (func (import "wasi_snapshot_preview1" "fd_write") (param i32 i32 i32 i32) (result i32))
      (memory (export "memory") 1)
      (data (i32.const 0)    "\08\00\00\00")    ;; buf ptr
      (data (i32.const 0x04) "\04\00\00\00")    ;; buf len
      (data (i32.const 0x08) "\12\34\56\78")    ;; buf data
      (data (i32.const 0x0c) "\de\ad\be\ef")    ;; will be overwritten with nwritten
      (func (export "_start")
        (call 0
          (i32.const 1) ;; fd
          (i32.const 0) ;; iov_ptr
          (i32.const 1) ;; iov_cnt
          (i32.const 0x0c)) ;; nwritten_ptr
        (if (i32.popcnt) (then unreachable)))
    */
    const auto wasm = from_hex(
        "0061736d01000000010c0260047f7f7f7f017f60000002230116776173695f736e617073686f745f7072657669"
        "6577310866645f77726974650000030201010503010001071302066d656d6f72790200065f737461727400010a"
        "13011100410141004101410c1000690440000b0b0b25040041000b04080000000041040b04040000000041080b"
        "041234567800410c0b04deadbeef");

    auto instance = wasi::instantiate(*mock_uvwasi, wasm);

    EXPECT_FALSE(mock_uvwasi->init_called);
    EXPECT_FALSE(mock_uvwasi->write_fd.has_value());
    EXPECT_TRUE(mock_uvwasi->write_data.empty());

    std::ostringstream err;
    EXPECT_TRUE(wasi::run(*mock_uvwasi, *instance, 0, nullptr, err)) << err.str();

    EXPECT_TRUE(mock_uvwasi->init_called);
    ASSERT_TRUE(mock_uvwasi->write_fd.has_value());
    EXPECT_EQ(*mock_uvwasi->write_fd, 1);
    EXPECT_THAT(mock_uvwasi->write_data, ElementsAre(from_hex("12345678")));

    // nwritten
    EXPECT_EQ(instance->memory->substr(0x0c, 4), from_hex("04000000"));
}
