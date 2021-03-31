// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "value.hpp"
#include <cassert>
#include <cstddef>

namespace fizzy
{
/// The storage for information shared by calls in the same execution "thread".
/// Users may decide how to allocate the execution context, but some good defaults are available.
class ExecutionContext
{
    static constexpr size_t DefaultStackSpaceSegmentSize = 100;

    /// Call depth increment guard.
    /// It will automatically decrement the call depth to the original value
    /// when going out of scope.
    class [[nodiscard]] LocalContext
    {
        ExecutionContext& m_shared_ctx;  ///< Reference to the shared execution context.

    public:
        Value* stack_space = nullptr;
        Value* prev_stack_space_segment = nullptr;
        size_t prev_free_stack_space = 0;

        LocalContext(const LocalContext&) = delete;
        LocalContext(LocalContext&&) = delete;
        LocalContext& operator=(const LocalContext&) = delete;
        LocalContext& operator=(LocalContext&&) = delete;

        LocalContext(ExecutionContext& ctx, size_t required_stack_space) : m_shared_ctx{ctx}
        {
            ++m_shared_ctx.depth;

            prev_free_stack_space = m_shared_ctx.free_stack_space;

            if (required_stack_space <= m_shared_ctx.free_stack_space)
            {
                // Must be a segment of default size or required_stack_space is 0.
                const auto offset =
                    DefaultStackSpaceSegmentSize - m_shared_ctx.free_stack_space;
                stack_space = m_shared_ctx.stack_space_segment + offset;
                prev_free_stack_space = m_shared_ctx.free_stack_space;
                m_shared_ctx.free_stack_space -= required_stack_space;
            }
            else
            {
                prev_stack_space_segment = m_shared_ctx.stack_space_segment;
                const auto new_segment_size =
                    std::max(DefaultStackSpaceSegmentSize, required_stack_space);
                m_shared_ctx.stack_space_segment = new Value[new_segment_size];
                stack_space = m_shared_ctx.stack_space_segment;
                m_shared_ctx.free_stack_space = new_segment_size - required_stack_space;
            }
        }

        ~LocalContext() noexcept
        {
            --m_shared_ctx.depth;

            m_shared_ctx.free_stack_space = prev_free_stack_space;
            if (prev_stack_space_segment != nullptr)
            {
                assert(m_shared_ctx.stack_space_segment == stack_space);
                delete[] stack_space;
                m_shared_ctx.stack_space_segment = prev_stack_space_segment;
            }
        }
    };

public:
    Value first_stack_space_segment[DefaultStackSpaceSegmentSize];
    Value* stack_space_segment = first_stack_space_segment;
    size_t free_stack_space = DefaultStackSpaceSegmentSize;

    int depth = 0;  ///< Current call depth.

    /// Increments the call depth and returns the local call context which
    /// decrements the call depth back to the original value when going out of scope.
    LocalContext create_local_context(size_t required_stack_space = 0)
    {
        return LocalContext{*this, required_stack_space};
    }
};
}  // namespace fizzy
