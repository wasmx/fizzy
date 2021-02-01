// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

namespace fizzy::wasi
{
/// Executes WASI main function with given CLI arguments.
// TODO: Make noexcept.
bool run(int argc, const char** argv);
}
