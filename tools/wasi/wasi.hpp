// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <iosfwd>

namespace fizzy::wasi
{
/// Executes WASI main function with given CLI arguments.
///
/// @param  argc    Number of CLI arguments.
/// @param  argv    Array of CLI arguments.
/// @param  err     Error output stream.
/// @return         False in case of error.
///
/// @todo Make noexcept.
bool run(int argc, const char** argv, std::ostream& err);
}
