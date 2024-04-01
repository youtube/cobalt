/* Copyright (c) 2021, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

use std::env;
use std::path::Path;

fn main() {
    let dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let crate_path = Path::new(&dir);

    // Find the bindgen generated target platform bindings file and set BINDGEN_RS_FILE
    let bindgen_file = crate_path
        .join("src")
        .read_dir()
        .unwrap()
        .map(|file| file.unwrap().file_name().into_string().unwrap())
        .find(|file| file.starts_with("wrapper_"))
        .unwrap();
    println!("cargo:rustc-env=BINDGEN_RS_FILE={}", bindgen_file);

    // building bssl-sys with: `cmake -G Ninja -B build -DRUST_BINDINGS="$(gcc -dumpmachine)" && ninja -C build`
    // outputs this crate to /build/rust/bssl-sys/ so need to go up 3 levels to the root of the repo
    let repo_root = crate_path.parent().unwrap().parent().unwrap();

    // Statically link libraries.
    println!(
        "cargo:rustc-link-search=native={}",
        repo_root.join("crypto").display()
    );
    println!("cargo:rustc-link-lib=static=crypto");

    println!(
        "cargo:rustc-link-search=native={}",
        repo_root.join("ssl").display()
    );
    println!("cargo:rustc-link-lib=static=ssl");

    println!("cargo:rustc-link-search=native={}", crate_path.display());
    println!("cargo:rustc-link-lib=static=rust_wrapper");
}
