#pragma once

#include <cstdint>
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

    T& operator[](size_t index) noexcept { return std::vector<T>::operator[](size() - index - 1); }

    T& top() noexcept { return (*this)[0]; }

    /// Drops @a num_elements elements from the top of the stack.
    void drop(size_t num_elements = 1) noexcept { resize(size() - num_elements); }

    void shrink(size_t new_size) noexcept { resize(new_size); }
};
}  // namespace fizzy
