// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

//! This is a Rust interface to [Fizzy](https://github.com/wasmx/fizzy), a WebAssembly virtual machine.
//!
//! Currently it is only possible to validate inputs, but soon execution will be supported too.

mod sys;

/// Parse and validate the input according to WebAssembly 1.0 rules. Returns true if the supplied input is valid.
pub fn validate<T: AsRef<[u8]>>(input: T) -> bool {
    unsafe { sys::fizzy_validate(input.as_ref().as_ptr(), input.as_ref().len()) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn validate_wasm() {
        // Empty
        assert_eq!(validate(&[]), false);
        // Too short
        assert_eq!(validate(&[0x00]), false);
        // Valid
        assert_eq!(
            validate(&[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]),
            true
        );
        // Invalid version
        assert_eq!(
            validate(&[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x01]),
            false
        );
    }
}
