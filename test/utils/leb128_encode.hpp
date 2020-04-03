#pragma once

#include "bytes.hpp"

namespace fizzy::test
{
/// Encodes the value as unsigned LEB128.
fizzy::bytes leb128u_encode(uint64_t value) noexcept;
}  // namespace fizzy::test
