// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! A test splash selector, written against the splash selector ABI v1 (see
//! ../../README.md). In production the web app team writes the selector; this
//! one only exists for the tests. Regenerate the embedded bytes with
//! ../regenerate_test_selector.sh after changing it.
//!
//! Selection rules:
//!   "topic":"music"            -> "music.webm"
//!   "screen_height" >= 2160    -> "splash_2160.webm"
//!   otherwise                  -> "splash_1080.webm"
//! Test hooks:
//!   "test_mode":"loop"         -> never returns (exercises the host's limit)
//!   "test_mode":"trap"         -> traps (exercises the host's error path)
//!   "test_mode":"bad_output"   -> returns an out-of-bounds result
//!
//! `no_std`, no allocator: the input is copied into a fixed buffer, and the
//! JSON is matched textually, since the host produces it.

#![no_std]

use core::panic::PanicInfo;
use core::sync::atomic::{AtomicU32, Ordering};

#[panic_handler]
fn panic(_: &PanicInfo) -> ! {
    core::arch::wasm32::unreachable()
}

const ABI_VERSION: u32 = 1;
const INPUT_CAPACITY: usize = 16 * 1024;

/// The buffer that `splash_alloc` hands out, for the host's input.
static mut INPUT: [u8; INPUT_CAPACITY] = [0; INPUT_CAPACITY];

/// Packs a key for `splash_select`'s result: length in the high 32 bits,
/// address in the low 32 bits.
fn result(key: &'static str) -> u64 {
    ((key.len() as u64) << 32) | (key.as_ptr() as u32 as u64)
}

/// Returns the offset just past the first occurrence of `pattern`, if any.
fn find(haystack: &[u8], pattern: &[u8]) -> Option<usize> {
    haystack
        .windows(pattern.len())
        .position(|window| window == pattern)
        .map(|at| at + pattern.len())
}

/// Parses the decimal number at `at`, stopping at the first non-digit.
fn parse_uint(input: &[u8], at: usize) -> u32 {
    input[at..]
        .iter()
        .take_while(|c| c.is_ascii_digit())
        .fold(0u32, |n, c| n.wrapping_mul(10).wrapping_add((c - b'0') as u32))
}

#[no_mangle]
pub extern "C" fn splash_abi_version() -> u32 {
    ABI_VERSION
}

#[no_mangle]
pub extern "C" fn splash_alloc(size: u32) -> *mut u8 {
    if size as usize > INPUT_CAPACITY {
        return core::ptr::null_mut();
    }
    // The host calls splash_alloc once, before splash_select, on one thread,
    // and the buffer is only read through the returned pointer.
    core::ptr::addr_of_mut!(INPUT) as *mut u8
}

#[no_mangle]
pub extern "C" fn splash_select(ptr: *const u8, len: u32) -> u64 {
    // SAFETY: the host passes the pointer and length it got from
    // splash_alloc, filled with the input.
    let input = unsafe { core::slice::from_raw_parts(ptr, len as usize) };

    if find(input, br#""test_mode":"loop""#).is_some() {
        let counter = AtomicU32::new(0);
        loop {
            counter.fetch_add(1, Ordering::Relaxed);
        }
    }
    if find(input, br#""test_mode":"trap""#).is_some() {
        core::arch::wasm32::unreachable();
    }
    if find(input, br#""test_mode":"bad_output""#).is_some() {
        return (16u64 << 32) | 0xFFFF_FFF0;
    }
    if find(input, br#""topic":"music""#).is_some() {
        return result("music.webm");
    }
    if let Some(at) = find(input, br#""screen_height":"#) {
        if parse_uint(input, at) >= 2160 {
            return result("splash_2160.webm");
        }
    }
    result("splash_1080.webm")
}
