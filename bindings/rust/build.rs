// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

extern crate bindgen;
extern crate cmake;

use cmake::Config;

use std::env;
use std::path::PathBuf;

fn main() {
    // TODO: Improve this.
    //
    // Unfortunately there is no environment variable for the original (and not build-time)
    // location of the manifest, and `env::current_dir()` points to the build-time directory.
    //
    // Tried also libgit2 to find the root, but that requires the root to begin with (unlike the commandline git command).
    let current = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let src = current.parent().unwrap().parent().unwrap();
    let src = if !src.join("LICENSE").exists() {
        // Assume this run is within `cargo package` which has an extra directory depth.
        //
        // TODO: this may be wrong.
        src.parent().unwrap()
    } else {
        src
    };

    let dst = Config::new(src).define("FIZZY_TESTING", "OFF").build();

    println!("cargo:rustc-link-lib=static=fizzy");
    println!("cargo:rustc-link-search=native={}/lib", dst.display());

    // We need to link against C++ std lib
    if let Some(cpp_stdlib) = get_cpp_stdlib() {
        println!("cargo:rustc-link-lib={}", cpp_stdlib);
    }

    let bindings = bindgen::Builder::default()
        .header(format!("{}/include/fizzy/fizzy.h", src.display()))
        // See https://github.com/rust-lang-nursery/rust-bindgen/issues/947
        .trust_clang_mangling(false)
        .generate_comments(true)
        // https://github.com/rust-lang-nursery/rust-bindgen/issues/947#issuecomment-327100002
        .layout_tests(false)
        .whitelist_function("fizzy_.*")
        // TODO: consider removing this
        .size_t_is_usize(true)
        .generate()
        .expect("Unable to generate bindings");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Could not write bindings");
}

// See https://github.com/alexcrichton/gcc-rs/blob/88ac58e25/src/lib.rs#L1197
fn get_cpp_stdlib() -> Option<String> {
    env::var("TARGET").ok().and_then(|target| {
        if target.contains("msvc") {
            None
        } else if target.contains("darwin") || target.contains("freebsd") {
            Some("c++".to_string())
        } else if target.contains("musl") {
            Some("static=stdc++".to_string())
        } else {
            Some("stdc++".to_string())
        }
    })
}
