#pragma once

namespace fizzy
{
constexpr unsigned PageSize = 65536;
// Set hard limit of 256MB of memory.
constexpr unsigned MemoryPagesLimit = (256 * 1024 * 1024ULL) / PageSize;
// Call depth limit is set to default limit in wabt.
// https://github.com/WebAssembly/wabt/blob/ae2140ddc6969ef53599fe2fab81818de65db875/src/interp/interp.h#L1007
// TODO: review this
constexpr int CallStackLimit = 2048;
}  // namespace fizzy
