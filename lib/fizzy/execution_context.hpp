// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "value.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>

namespace fizzy
{
/// The storage for information shared by calls in the same execution "thread".
/// Users may decide how to allocate the execution context, but some good defaults are available.
///
/// The ExecutionContext manages WebAssembly stack space shared between calls in the same execution
/// thread. The shared stack space is allocated and managed by create_local_context() and
/// LocalContext objects.
///
/// The shared stack space is conceptually implemented as linked list of stack space segments.
/// If the required stack space for a new call fits in the current segment no new
/// allocation is needed. Otherwise new segment is allocated. The size of the new segment is
/// at least DefaultStackSpaceSegmentSize but can be larger if the call's requires stack space
/// exceeds the default size (in this case the call occupies the segment exclusively).
///
/// When the LocalContext which allocated new stack segment is being destroyed (i.e. when the first
/// call occupying this stack segment ends) this segment is freed. This may not be the optimal
/// strategy in case the same segment is going to be allocated multiple times.
/// There is alternative design when segments are not freed when not used any more and can be reused
/// when more stack space is needed. However, this requires additional housekeeping (e.g. having
/// forward pointer to the next segment) and handling some additional edge-cases (e.g. reallocate
/// an unused segment in case it is smaller then the required stack space).
class ExecutionContext
{
    static constexpr size_t DefaultStackSpaceSegmentSize = 100;

    /// Call depth increment guard.
    /// It will automatically decrement the call depth to the original value
    /// when going out of scope.
    class [[nodiscard]] LocalContext
    {
        /// Reference to the shared execution context.
        ExecutionContext& m_shared_ctx;

    public:
        /// Pointer to the reserved "required" stack space.
        Value* stack_space = nullptr;

        /// Pointer to the previous segment.
        /// This is not null only for LocalContexts which allocated new segment.
        Value* prev_stack_space_segment = nullptr;

        /// Amount of free stack space before this LocalContext has been created.
        /// This is used to restore "free" space information in ExecutionContext (m_shared_ctx)
        /// when this LocalContext object is destroyed.
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
                const auto offset = DefaultStackSpaceSegmentSize - m_shared_ctx.free_stack_space;
                stack_space = m_shared_ctx.stack_space_segment + offset;
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
                assert(stack_space == m_shared_ctx.stack_space_segment);
                assert(stack_space != m_shared_ctx.first_stack_space_segment);
                delete[] stack_space;
                m_shared_ctx.stack_space_segment = prev_stack_space_segment;
            }
        }
    };

public:
    /// Pre-allocated first segment of the shared stack space.
    Value first_stack_space_segment[DefaultStackSpaceSegmentSize];

    /// Point to the current stack space segment.
    Value* stack_space_segment = first_stack_space_segment;

    /// Amount of free stack space remaining in the current segment.
    /// It is better to keep information about "free" than "used" space
    /// because then we don't need to know the current segment size.
    size_t free_stack_space = DefaultStackSpaceSegmentSize;

    /// Current call depth.
    int depth = 0;

    /// Increments the call depth and returns the local call context which
    /// decrements the call depth back to the original value when going out of scope.
    /// This also allocates and manages the shared stack space.
    /// @param  required_stack_space    Size of the required stack space in bytes.
    /// @see ExecutionContext
    LocalContext create_local_context(size_t required_stack_space = 0)
    {
        return LocalContext{*this, required_stack_space};
    }
};
}  // namespace fizzy
