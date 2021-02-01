// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

/// A minimalistic version of std::ptr::NonNull for *const T.
pub(crate) struct ConstNonNull<T: ?Sized> {
    pointer: *const T,
}

impl<T: ?Sized> ConstNonNull<T> {
    #[inline]
    pub const unsafe fn new_unchecked(ptr: *const T) -> Self {
        // SAFETY: the caller must guarantee that `ptr` is non-null.
        unsafe { ConstNonNull { pointer: ptr } }
    }

    #[must_use]
    #[inline]
    pub const fn as_ptr(self) -> *const T {
        self.pointer
    }
}

impl<T: ?Sized> Clone for ConstNonNull<T> {
    #[inline]
    fn clone(&self) -> Self {
        *self
    }
}

impl<T: ?Sized> Copy for ConstNonNull<T> {}
