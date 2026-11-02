# Native splash screen (proof of concept)

A splash screen without a WebContents. The web app chooses which splash video
Cobalt plays at launch, with a WebAssembly module it puts into the splash
cache. Cobalt runs the module natively with WAMR (WebAssembly Micro Runtime,
`//third_party/wasm_micro_runtime`, interpreter only), then plays the video
directly with a Starboard player. Enabled with `--native-splash-player`.

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
    2.  runs it with WAMR (`RunSplashSelectorWasm()`), passing it a JSON
        description of the launch. It returns a cache key;
    3.  reads the video with that key from the cache.

    If there is no selector, it fails, exceeds its instruction limit (10M
    instructions by default) or picks a missing video, it reads `splash.webm`
    instead. If that is missing too, `NativeSplash` plays the built-in one.

3.  `SplashVideoPlayer` demuxes the WebM and plays its video track with an
    `SbPlayer` in punch-out mode. The shell keeps its WebContents out of view
    until the video ends.

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

`testdata/test_selector/` is a complete selector in Rust (`no_std`, 911 bytes
of Wasm), the one the tests use. The gist:

```rust
#[no_mangle]
pub extern "C" fn splash_abi_version() -> u32 { 1 }

#[no_mangle]
pub extern "C" fn splash_alloc(size: u32) -> *mut u8 { /* a buffer of `size` bytes */ }

#[no_mangle]
pub extern "C" fn splash_select(ptr: *const u8, len: u32) -> u64 {
    let input = unsafe { core::slice::from_raw_parts(ptr, len as usize) };
    let key: &'static str = choose(input);  // The web app's logic.
    ((key.len() as u64) << 32) | key.as_ptr() as u32 as u64
}
```

Build with `cargo build --release --target wasm32-unknown-unknown`;
`testdata/regenerate_test_selector.sh` shows how to do it with the Rust
toolchain in the Chromium tree. The same module can be tested in a browser or
Node.js with `WebAssembly.instantiate()`.

## Known limitations

*   The selector runs on the UI thread.
*   On Android, `SbPlayerCreate()` fails until the video surface exists, so
    `SplashVideoPlayer` retries every 50 ms. A real implementation should start
    on the surface's creation instead.
*   Video only (no audio), punch-out only. The main WebContents is shown when
    the video ends, whether or not it has loaded.

## Trying it on a device

1.  Build and install (a fresh checkout fetches WAMR at gn time):

    ```sh
    autoninja -C out/android-arm_devel cobalt_apk
    adb install -r -d out/android-arm_devel/apks/Cobalt.apk
    ```

2.  Seed the splash cache, as the web app would. Make the demo assets (the
    test selector and three visibly different videos), start Cobalt with the
    web app, then put them into the cache through DevTools:

    ```sh
    T=cobalt/shell/browser/splash/tools
    $T/make_test_assets.sh /tmp/splash_assets
    adb shell am start -n dev.cobalt.coat/dev.cobalt.app.MainActivity
    $T/seed_splash_cache.py /tmp/splash_assets/*
    ```

    `seed_splash_cache.py --list` shows the cache; `--clear` empties it.

3.  Cold-start with the native splash:

    ```sh
    adb shell am force-stop dev.cobalt.coat
    adb shell am start -n dev.cobalt.coat/dev.cobalt.app.MainActivity \
        --esa commandLineArgs --native-splash-player
    adb logcat -d | grep -E "Splash selection|NativeSplash"
    ```

    On a 1080p device the selector picks `splash_1080.webm`, the inverted
    video; `by_wasm=1` in the log confirms it ran.

## Benchmark

`tools/splash_benchmark.py` cold-starts Cobalt N times per arm, round-robin,
and reads the startup timeline that `SplashScreenMetrics` logs with
`--vmodule=splash_screen_metrics=1`, all relative to the app's start. The
main metric is `MainReady`: when the web app calls
`h5vcc.system.hideSplashScreen()`. Arms: `image` (default splash.html),
`html_video` (`ForceVideoSplashScreen`), `native_wamr` (this PoC). Seed the
cache first, so that the selector has something to choose from.

```sh
tools/splash_benchmark.py --apk out/android-arm_devel/apks/Cobalt.apk \
    --runs 30 --out results.csv
tools/splash_benchmark.py --report --out results.csv
```

The report gives p50/p90 per arm, and the median difference to `image` with a
bootstrap 95% CI and a Mann-Whitney p-value. `tools/results/` has the runs
done so far.

## Tests

*   `splash_wasm_runner_unittest.cc` (`cobalt_unittests`): the ABI, errors,
    the instruction limit.
*   `splash_wasm_selector_browsertest.cc` (`cobalt_browsertests`): the whole
    flow, with the cache populated from JavaScript.
*   `testdata/test_selector/` is the test selector, in Rust. Regenerate its
    embedded bytes with `testdata/regenerate_test_selector.sh`.
