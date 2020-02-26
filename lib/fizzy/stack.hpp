#pragma once

#include <cstdint>
#include <vector>

namespace fizzy
{
template <typename T>
class Stack
{
    T* top_item;

    T storage[1024];

public:
    using difference_type = typename std::vector<T>::difference_type;

    /// Default constructor. Sets the top_item pointer to below the stack bottom.
    [[clang::no_sanitize("bounds")]] Stack() noexcept : top_item{storage - 1} {}

    /// The current number of items on the stack.
    unsigned size() noexcept { return static_cast<unsigned>(top_item + 1 - storage); }

    /// Returns the reference to the top item.
    T& top() noexcept { return *top_item; }

    /// Returns the reference to the stack item on given position from the stack top.
    T& operator[](int index) noexcept { return *(top_item - index); }

    /// Pushes an item on the stack. The stack limit is not checked.
    inline void push(const T& item) noexcept { *++top_item = item; }

    /// Returns an item popped from the top of the stack.
    inline T pop() noexcept { return *top_item--; }

    inline T peek(size_t depth = 0) const noexcept { return *(top_item - depth); }

    void drop(size_t num_elements = 1) noexcept { top_item -= num_elements; }

    // Also used: size(), resize(), clear(), empty(), end()

    void clear() noexcept { top_item = storage - 1; }

    bool empty() noexcept { return (top_item == (storage - 1)); }

    void resize(size_t new_size) noexcept
    {
        const auto cur_size = size();
        if (cur_size < new_size)
            top_item += (new_size - cur_size);
        else
            drop(cur_size - new_size);
    }

    T* begin() noexcept { return storage; }

    T* end() noexcept { return top_item + 1; }
};
}  // namespace fizzy
