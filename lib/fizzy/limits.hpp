#pragma once

namespace fizzy
{
constexpr unsigned PageSize = 65536;
// Set hard limit of 256MB of memory.
constexpr unsigned MemoryPagesLimit = (256 * 1024 * 1024ULL) / PageSize;
}  // namespace fizzy
