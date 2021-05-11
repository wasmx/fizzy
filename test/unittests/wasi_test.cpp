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
    std::optional<uvwasi_fd_t> read_fd;
    const bytes read_data = from_hex("3243f6a8885a308d313198a2e03707");

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

    uvwasi_errno_t fd_read(uvwasi_fd_t fd, const uvwasi_iovec_t* iovs, uvwasi_size_t iovs_len,
        uvwasi_size_t* nread) noexcept final
    {
        read_fd = fd;

        uvwasi_size_t total_len = 0;
        for (uvwasi_size_t i = 0; i < iovs_len; ++i)
        {
            const auto n =
                std::min(iovs[i].buf_len, static_cast<uvwasi_size_t>(read_data.size()) - total_len);
            std::copy_n(read_data.begin() + total_len, n, static_cast<uint8_t*>(iovs[i].buf));

            total_len += n;

            if (n < iovs[i].buf_len)
                break;
        }

        *nread = total_len;
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

TEST_F(wasi_mocked_test, fd_write_gather)
{
    /* wat2wasm
      (func (import "wasi_snapshot_preview1" "fd_write") (param i32 i32 i32 i32) (result i32))
      (memory (export "memory") 1)
      (data (i32.const 0)    "\10\00\00\00")    ;; buf1 ptr
      (data (i32.const 0x04) "\04\00\00\00")    ;; buf1 len
      (data (i32.const 0x08) "\14\00\00\00")    ;; buf2 ptr
      (data (i32.const 0x0c) "\08\00\00\00")    ;; buf2 len
      (data (i32.const 0x10) "\12\34\56\78")    ;; buf1 data
      (data (i32.const 0x14) "\11\22\33\44\55\66\77\88") ;; buf2 data
      (data (i32.const 0x1c) "\de\ad\be\ef")    ;; will be overwritten with nwritten
      (func (export "_start")
        (call 0
          (i32.const 1) ;; fd
          (i32.const 0) ;; iov_ptr
          (i32.const 2) ;; iov_cnt
          (i32.const 0x1c)) ;; nwritten_ptr
        (if (i32.popcnt) (then unreachable)))
    */
    const auto wasm = from_hex(
        "0061736d01000000010c0260047f7f7f7f017f60000002230116776173695f736e617073686f745f7072657669"
        "6577310866645f77726974650000030201010503010001071302066d656d6f72790200065f737461727400010a"
        "13011100410141004102411c1000690440000b0b0b44070041000b04100000000041040b04040000000041080b"
        "041400000000410c0b04080000000041100b04123456780041140b08112233445566778800411c0b04deadbee"
        "f");

    auto instance = wasi::instantiate(*mock_uvwasi, wasm);

    EXPECT_FALSE(mock_uvwasi->init_called);
    EXPECT_FALSE(mock_uvwasi->write_fd.has_value());
    EXPECT_TRUE(mock_uvwasi->write_data.empty());

    std::ostringstream err;
    EXPECT_TRUE(wasi::run(*mock_uvwasi, *instance, 0, nullptr, err)) << err.str();

    EXPECT_TRUE(mock_uvwasi->init_called);
    ASSERT_TRUE(mock_uvwasi->write_fd.has_value());
    EXPECT_EQ(*mock_uvwasi->write_fd, 1);
    EXPECT_THAT(
        mock_uvwasi->write_data, ElementsAre(from_hex("12345678"), from_hex("1122334455667788")));

    // nwritten
    EXPECT_EQ(instance->memory->substr(0x1c, 4), from_hex("0c000000"));
}

TEST_F(wasi_mocked_test, fd_write_invalid_input)
{
    /* wat2wasm
      (func (import "wasi_snapshot_preview1" "fd_write") (param i32 i32 i32 i32) (result i32))
      (memory (export "memory") 1)
      (data (i32.const 0)    "\00\00\01\00")    ;; buf ptr - out of memory bounds
      (data (i32.const 0x04) "\04\00\00\00")    ;; buf len
      (global (mut i32) (i32.const 0))
      (func (export "_start")
        (call 0
          (i32.const 1) ;; fd
          (i32.const 0) ;; iov_ptr
          (i32.const 1) ;; iov_cnt
          (i32.const 0x0c)) ;; nwritten_ptr
        (global.set 0))
    */
    const auto wasm = from_hex(
        "0061736d01000000010c0260047f7f7f7f017f60000002230116776173695f736e617073686f745f7072657669"
        "6577310866645f777269746500000302010105030100010606017f0141000b071302066d656d6f72790200065f"
        "737461727400010a10010e00410141004101410c100024000b0b13020041000b04000001000041040b04040000"
        "00");

    auto instance = wasi::instantiate(*mock_uvwasi, wasm);

    EXPECT_FALSE(mock_uvwasi->init_called);

    std::ostringstream err;
    EXPECT_TRUE(wasi::run(*mock_uvwasi, *instance, 0, nullptr, err)) << err.str();

    EXPECT_TRUE(mock_uvwasi->init_called);
    EXPECT_NE(instance->globals[0].i32, 0);
}

TEST_F(wasi_mocked_test, fd_read)
{
    /* wat2wasm
      (func (import "wasi_snapshot_preview1" "fd_read") (param i32 i32 i32 i32) (result i32))
      (memory (export "memory") 1)
      (data (i32.const 0)    "\08\00\00\00")    ;; buf ptr
      (data (i32.const 0x04) "\04\00\00\00")    ;; buf len
      (data (i32.const 0x08) "\12\34\56\78")    ;; buf data
      (data (i32.const 0x0c) "\de\ad\be\ef")    ;; will be overwritten with nread
      (func (export "_start")
        (call 0
          (i32.const 0) ;; fd
          (i32.const 0) ;; iov_ptr
          (i32.const 1) ;; iov_cnt
          (i32.const 0x0c)) ;; nread_ptr
        (if (i32.popcnt) (then unreachable)))
    */
    const auto wasm = from_hex(
        "0061736d01000000010c0260047f7f7f7f017f60000002220116776173695f736e617073686f745f7072657669"
        "6577310766645f726561640000030201010503010001071302066d656d6f72790200065f737461727400010a13"
        "011100410041004101410c1000690440000b0b0b25040041000b04080000000041040b04040000000041080b04"
        "1234567800410c0b04deadbeef");

    auto instance = wasi::instantiate(*mock_uvwasi, wasm);

    EXPECT_FALSE(mock_uvwasi->init_called);
    EXPECT_FALSE(mock_uvwasi->read_fd.has_value());

    std::ostringstream err;
    EXPECT_TRUE(wasi::run(*mock_uvwasi, *instance, 0, nullptr, err)) << err.str();

    EXPECT_TRUE(mock_uvwasi->init_called);
    ASSERT_TRUE(mock_uvwasi->read_fd.has_value());
    EXPECT_EQ(*mock_uvwasi->read_fd, 0);

    // read data
    EXPECT_EQ(instance->memory->substr(0x08, 4), mock_uvwasi->read_data.substr(0, 4));
    // nread
    EXPECT_EQ(instance->memory->substr(0x0c, 4), from_hex("04000000"));
}

TEST_F(wasi_mocked_test, fd_read_scatter)
{
    /* wat2wasm
      (func (import "wasi_snapshot_preview1" "fd_read") (param i32 i32 i32 i32) (result i32))
      (memory (export "memory") 1)
      (data (i32.const 0)    "\10\00\00\00")    ;; buf1 ptr
      (data (i32.const 0x04) "\04\00\00\00")    ;; buf1 len
      (data (i32.const 0x08) "\14\00\00\00")    ;; buf2 ptr
      (data (i32.const 0x0c) "\08\00\00\00")    ;; buf2 len
      (data (i32.const 0x10) "\12\34\56\78")    ;; buf1 data
      (data (i32.const 0x14) "\11\22\33\44\55\66\77\88") ;; buf2 data
      (data (i32.const 0x1c) "\de\ad\be\ef")    ;; will be overwritten with nread
      (func (export "_start")
        (call 0
          (i32.const 0) ;; fd
          (i32.const 0) ;; iov_ptr
          (i32.const 2) ;; iov_cnt
          (i32.const 0x1c)) ;; nread_ptr
        (if (i32.popcnt) (then unreachable)))
    */
    const auto wasm = from_hex(
        "0061736d01000000010c0260047f7f7f7f017f60000002220116776173695f736e617073686f745f7072657669"
        "6577310766645f726561640000030201010503010001071302066d656d6f72790200065f737461727400010a13"
        "011100410041004102411c1000690440000b0b0b44070041000b04100000000041040b04040000000041080b04"
        "1400000000410c0b04080000000041100b04123456780041140b08112233445566778800411c0b04deadbeef");

    auto instance = wasi::instantiate(*mock_uvwasi, wasm);

    EXPECT_FALSE(mock_uvwasi->init_called);
    EXPECT_FALSE(mock_uvwasi->read_fd.has_value());

    std::ostringstream err;
    EXPECT_TRUE(wasi::run(*mock_uvwasi, *instance, 0, nullptr, err)) << err.str();

    EXPECT_TRUE(mock_uvwasi->init_called);
    ASSERT_TRUE(mock_uvwasi->read_fd.has_value());
    EXPECT_EQ(*mock_uvwasi->read_fd, 0);

    // read data
    EXPECT_EQ(instance->memory->substr(0x10, 4), mock_uvwasi->read_data.substr(0, 4));
    EXPECT_EQ(instance->memory->substr(0x14, 8), mock_uvwasi->read_data.substr(4, 8));
    // nread
    EXPECT_EQ(instance->memory->substr(0x1c, 4), from_hex("0c000000"));
}

TEST_F(wasi_mocked_test, fd_read_invalid_input)
{
    /* wat2wasm
      (func (import "wasi_snapshot_preview1" "fd_read") (param i32 i32 i32 i32) (result i32))
      (memory (export "memory") 1)
      (data (i32.const 0)    "\00\00\01\00")    ;; buf ptr - out of memory bounds
      (data (i32.const 0x04) "\04\00\00\00")    ;; buf len
      (global (mut i32) (i32.const 0))
      (func (export "_start")
        (call 0
          (i32.const 0) ;; fd
          (i32.const 0) ;; iov_ptr
          (i32.const 1) ;; iov_cnt
          (i32.const 0x0c)) ;; nread_ptr
        (global.set 0))
    */
    const auto wasm = from_hex(
        "0061736d01000000010c0260047f7f7f7f017f60000002220116776173695f736e617073686f745f7072657669"
        "6577310766645f7265616400000302010105030100010606017f0141000b071302066d656d6f72790200065f73"
        "7461727400010a10010e00410041004101410c100024000b0b13020041000b04000001000041040b040400000"
        "0");

    auto instance = wasi::instantiate(*mock_uvwasi, wasm);

    EXPECT_FALSE(mock_uvwasi->init_called);

    std::ostringstream err;
    EXPECT_TRUE(wasi::run(*mock_uvwasi, *instance, 0, nullptr, err)) << err.str();

    EXPECT_TRUE(mock_uvwasi->init_called);
    EXPECT_NE(instance->globals[0].i32, 0);
}

