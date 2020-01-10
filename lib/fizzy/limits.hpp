namespace fizzy
{
constexpr unsigned page_size = 65536;
// Set hard limit of 256MB of memory.
constexpr unsigned memory_pages_limit = (256 * 1024 * 1024ULL) / page_size;
}  // namespace fizzy
