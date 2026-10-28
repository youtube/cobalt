# Splash selector (proof of concept)

The web app chooses which splash video Cobalt plays at launch, with a
WebAssembly module it puts into the splash cache. Cobalt runs the module
natively in the browser process, without a WebContents or a renderer.

## Flow

1.  **Last session**: the web app puts the selector and the videos into the
    splash Cache Storage of its origin. Same cache as the existing splash
    videos (`default` unless specified).

    ```js
    const cache = await caches.open('default');
    await cache.put('splash_selector.wasm', await fetch(selectorUrl));
    await cache.put('splash_2160.webm', await fetch(video4kUrl));
    await cache.put('splash.webm', await fetch(fallbackUrl));  // fallback
    ```

    Put the selector last, so that a selector never sees videos from a
    previous version missing.

2.  **This launch**: `SplashWasmSelector`

    1.  reads `splash_selector.wasm` from the cache (`SplashCacheReader`);
    2.  runs it in a fresh V8 isolate (`RunSplashSelectorWasm()`), passing
        it a JSON description of the launch. It returns a cache key;
    3.  reads the video with that key from the cache.

    If there is no selector, it fails, times out (100 ms by default) or picks
    a missing video, it reads `splash.webm` instead. If that is missing too,
    the caller shows its built-in splash.

3.  Playing the video is up to the caller.

## ABI v1

The module is a plain core Wasm module (no WASI, no imports). It exports:

| Export | Signature | Description |
| --- | --- | --- |
| `memory` | memory | The module's linear memory. |
| `splash_abi_version` | `() -> i32` | Must return `1`. |
| `splash_alloc` | `(size: i32) -> i32` | Returns a buffer of `size` bytes in `memory`, for the input. |
| `splash_select` | `(ptr: i32, len: i32) -> i64` | Reads the UTF-8 JSON input at `ptr`, returns the selected key as `(key_len << 32) \| key_ptr`. |

The key must be at most 256 bytes of `[A-Za-z0-9._-/]`. It is resolved against
the web app's origin, like the keys given to `cache.put()`.

Input (at most 64 KiB):

```json
{"abi_version":1,"now_ms":1793000000000.0,"locale":"en-US","topic":"music",
 "screen_width":3840,"screen_height":2160}
```

### Rust

```rust
#[no_mangle]
pub extern "C" fn splash_abi_version() -> u32 { 1 }

#[no_mangle]
pub extern "C" fn splash_alloc(size: u32) -> *mut u8 {
    let mut buf = Vec::<u8>::with_capacity(size as usize);
    let ptr = buf.as_mut_ptr();
    std::mem::forget(buf);
    ptr
}

#[no_mangle]
pub extern "C" fn splash_select(ptr: *const u8, len: u32) -> u64 {
    let input = unsafe { std::slice::from_raw_parts(ptr, len as usize) };
    let key: &'static str = choose(input);  // The web app's logic.
    ((key.len() as u64) << 32) | key.as_ptr() as u64
}
```

Build with `cargo build --target wasm32-unknown-unknown --release`. The same
module can be tested in a browser or Node.js with `WebAssembly.instantiate()`.

## Known limitations

*   **V8 initialization order.** Production Cobalt runs in single-process
    mode, where the in-process renderer initializes V8 and sets its flags when
    Blink starts. V8 freezes its flags once initialized, so the browser can't
    initialize it first. Until the renderer has initialized V8,
    `IsV8ReadyForSplashSelector()` returns false and the fallback
    video is used. Running the selector before the renderer starts needs V8's
    initialization, including the renderer's flags, moved earlier.
*   The selector runs on the calling (UI) thread.
*   Not wired into startup yet; only the tests use it.

## Tests

*   `splash_wasm_runner_unittest.cc` (`cobalt_unittests`): the ABI, errors,
    the timeout.
*   `splash_wasm_selector_browsertest.cc` (`cobalt_browsertests`): the whole
    flow, with the cache populated from JavaScript.
*   `testdata/test_selector.c` is the test selector. Regenerate its embedded
    bytes with `testdata/regenerate_test_selector.sh`.
