#pragma once

#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

namespace fizzy
{
template <typename T>
class Stack : public std::vector<T>
{
public:
    using difference_type = typename std::vector<T>::difference_type;

    using std::vector<T>::vector;

    using std::vector<T>::back;
    using std::vector<T>::emplace_back;
    using std::vector<T>::pop_back;
    using std::vector<T>::resize;
    using std::vector<T>::size;

    // Also used: size(), resize(), clear(), empty(), end()

    void push(T val) { emplace_back(val); }

    T pop()
    {
        const auto res = back();
        pop_back();
        return res;
    }

    T peek(size_t depth = 0) const noexcept { return (*this)[size() - depth - 1]; }

    /// Drops @a num_elements elements from the top of the stack.
    void drop(size_t num_elements = 1) noexcept { resize(size() - num_elements); }
};

class OperandStack
{
    /// The pointer to the top item, or below the stack bottom if stack is empty.
    ///
    /// This pointer always alias m_storage, but it is kept as the first field
    /// because it is accessed the most. Therefore, it must be initialized
    /// in the constructor after the m_storage.
    uint64_t* m_top;

    /// The storage for items.
    std::unique_ptr<uint64_t[]> m_storage;

public:
    /// Default constructor. Sets the top item pointer to below the stack bottom.
    explicit OperandStack(size_t max_stack_height)
      : m_storage{std::make_unique<uint64_t[]>(max_stack_height)}
    {
        m_top = m_storage.get() - 1;
    }

    OperandStack(const OperandStack&) = delete;
    OperandStack& operator=(const OperandStack&) = delete;

    /// The current number of items on the stack (aka stack height).
    [[nodiscard]] size_t size() const noexcept
    {
        return static_cast<size_t>(m_top + 1 - &m_storage[0]);
    }

    /// Returns the reference to the top item.
    /// Requires non-empty stack.
    [[nodiscard]] auto& top() noexcept
    {
        assert(size() != 0);
        return *m_top;
    }

    /// Returns the reference to the stack item on given position from the stack top.
    /// Requires index < size().
    [[nodiscard]] auto& operator[](size_t index) noexcept
    {
        assert(index < size());
        return *(m_top - index);
    }

    /// Pushes an item on the stack.
    /// The stack max height limit is not checked.
    void push(uint64_t item) noexcept { *++m_top = item; }

    /// Returns an item popped from the top of the stack.
    /// Requires non-empty stack.
    auto pop() noexcept
    {
        assert(size() != 0);
        return *m_top--;
    }

    /// Shrinks the stack to the given size by dropping item from the top.
    ///
    /// Requires size <= size().
    /// shrink(0) clears entire stack and moves the top pointer below the stack base
    /// (as in constructor) causing pointer arithmetic overflow - this is intended
    /// and sanitizer checks for such behavior are disabled.
    [[gnu::no_sanitize("pointer-overflow"), clang::no_sanitize("pointer-overflow")]] void shrink(
        size_t size) noexcept
    {
        assert(size <= this->size());
        // For size == 0, the m_top will point below the storage.
        m_top = m_storage.get() + (size - 1);
    }
};
}  // namespace fizzy
