// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

extern crate fizzy;

fn main() {
    assert!(fizzy::validate(&[]).is_ok());
    println!("Fizzy works!");
}
