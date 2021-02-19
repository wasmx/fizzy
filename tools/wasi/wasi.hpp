// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <bytes.hpp>
#include <iosfwd>
#include <optional>

namespace fizzy::wasi
{
/// Loads a binary file at the given path.
///
/// @param  file    Path to the file.
/// @param  err     Error output stream.
/// @return         Content of the loaded file.
std::optional<bytes> load_file(std::string_view file, std::ostream& err) noexcept;

/// Executes WASI main function with given CLI arguments.
///
/// @param  argc    Number of CLI arguments.
/// @param  argv    Array of CLI arguments.
/// @param  err     Error output stream.
/// @return         False in case of error.
///
/// @todo Make noexcept.
bool run(int argc, const char** argv, std::ostream& err);
}  // namespace fizzy::wasi
