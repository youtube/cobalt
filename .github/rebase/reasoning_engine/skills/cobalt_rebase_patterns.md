# Cobalt Rebase Patterns

### Handling Cobalt-Specific Version Macros

A common pattern in the Cobalt codebase is the use of preprocessor macros to conditionally compile code based on the Chromium milestone version. These macros follow the format `CHROMIUM_MILESTONE_LE_XXX`, where `XXX` is a Chromium version number (e.g., `CHROMIUM_MILESTONE_LE_138`).

When performing a rebase to a new Chromium version `YYY`, you must:
1.  Globally search the codebase for the pattern `CHROMIUM_MILESTONE_LE_`.
2.  For any macros referencing the *previous* version, update them to the *new* version number. For example, when rebasing from 138 to 140, all instances of `CHROMIUM_MILESTONE_LE_138` should be updated to `CHROMIUM_MILESTONE_LE_140` (or a higher version like 150 if the feature lifetime has been extended, as seen in PR #12161).
3.  This is a critical step for managing API churn and feature flags between versions. Failure to update these macros will result in using stale code paths.

---

### Cobalt Modules Stubs & Shim Hygiene (cobalt_modules_stubs.cc)

Cobalt strips several heavy Chromium Blink modules (such as WebXR, WebGPU, Bluetooth, WebUSB, HID) to minimize binary footprint on embedded and TV platforms. However, generated V8 bindings or core Blink headers may still reference stubbed classes.

When adding or fixing stubs in `third_party/blink/renderer/modules/cobalt_modules_stubs.cc`:
1. **Include Placement Rule**:
   - ALL `#include` directives MUST be placed at the top of the file before `namespace blink {`.
   - NEVER place `#include` directives in the middle of the file or after an open `namespace` block. Doing so nests third-party headers into `::blink`, breaking type lookups (e.g. `no template named 'X'; did you mean '::mojo::X'?`).
2. **Namespace Closure & Nesting**:
   - `namespace blink {` is opened near the top of the file (around line 50).
   - Do NOT declare duplicate `namespace blink {` openings inside the file without closing the earlier block. Multiple `namespace blink {` openings create `namespace blink::blink`, causing Clang error `cannot define or redeclare 'X' here because namespace 'blink' does not enclose namespace 'Y'`.
   - If you need to add new stubs, place them inside the existing `namespace blink { ... }` block, or ensure the earlier block is properly closed before opening a new one.
3. **Stubbing Strategy**:
   - If a method implementation is missing (e.g. `XRLayer::Trace` or `XRWebGLLayer::Trace`), provide an empty stub or call the base class:
     ```cpp
     void XRWebGLLayer::Trace(Visitor* visitor) const {
       XRLayer::Trace(visitor);
     }
     ```
   - If a class needs a wrapper type info, use the `STUB_V8_WRAPPER(ClassName)` macro defined in `cobalt_modules_stubs.cc`.

---

### Pruned Blink Modules & V8 Bindings Exclusions (bindings.gni vs cobalt_modules_stubs.cc)

Cobalt deliberately strips heavy upstream Chromium subsystems (WebXR, WebGPU, WebNN, AI / Translation, Bluetooth, HID, USB, etc.) to minimize binary footprint on embedded and TV platforms (`libchrobalt.so`).

1. **Architecture of Pruned Modules**:
   - C++ module implementations are excluded from compilation (e.g. in `third_party/blink/renderer/modules/BUILD.gn` or `modules/xr/BUILD.gn`).
   - The corresponding generated V8 IDL bindings MUST be excluded from compilation in `third_party/blink/renderer/bindings/modules/v8/BUILD.gn` using exclude patterns defined in `third_party/blink/renderer/bindings/bindings.gni`.

2. **Exclusion Pattern Groups in `third_party/blink/renderer/bindings/bindings.gni`**:
   - `cobalt_webgpu_exclude_patterns`: Covers WebGPU, WebXR, and WebNN. MUST contain:
     ```gn
     cobalt_webgpu_exclude_patterns = [
       "*v8_canvas_2d_gpu_*",
       "*v8_gpu.*",
       "*v8_gpu_*",
       "*v8_ml.*",
       "*v8_ml_*",
       "*v8_xr.*",
       "*v8_xr_*",
       "*v8_union_*gpu*",
     ]
     ```
   - `cobalt_hardware_exclude_patterns`: Covers Bluetooth, HID, Serial, USB, etc.
   - `cobalt_ai_exclude_patterns`: Covers AI rewrite, translate, summarize, etc.
   - `cobalt_bindings_exclude_patterns`: Covers WebRTC peer connection and privacy sandbox APIs when disabled.

3. **Linker Error Diagnostics (`undefined symbol: blink::XR*` / `blink::GPU*` / `blink::V8GPU*`)**:
   - When `ld.lld` fails during linking of `libchrobalt.so` with undefined symbols referencing `blink::XR*`, `blink::GPU*`, `blink::V8GPU*`, or symbols in `obj/third_party/blink/renderer/bindings/modules/v8/libv8.a`:
   - The diagnostic from `autoninja` will map the object archive `libv8.a` to its enclosing build file: `third_party/blink/renderer/bindings/modules/v8/BUILD.gn`.
   - **DO NOT** attempt to manually implement dozens of WebXR/WebGPU dummy stubs in `cobalt_modules_stubs.cc`.
   - **DO NOT** modify or delete exclude filters in `third_party/blink/renderer/bindings/modules/v8/BUILD.gn`.
   - **CHECK `third_party/blink/renderer/bindings/bindings.gni`**:
     Verify that `cobalt_webgpu_exclude_patterns` has NOT lost its entries during a cherry-pick or merge conflict.
     If `"*v8_xr.*"` and `"*v8_xr_*"` are missing, restore them immediately to `cobalt_webgpu_exclude_patterns` by targeting `FILE: third_party/blink/renderer/bindings/bindings.gni`.
   - GN pattern note: Both `"*v8_xr.*"` (matching `.../v8_xr.cc` and `.../v8_xr.h`) and `"*v8_xr_*"` (matching `.../v8_xr_*.cc` and `.../v8_xr_*.h`) are required because GN file patterns match the full input string.

4. **When to use `cobalt_modules_stubs.cc`**:
   - Use `cobalt_modules_stubs.cc` ONLY when a non-pruned subsystem (or core Blink) references a specific method or constructor that is missing because a single class implementation was stripped.
   - For entire modules whose bindings are compiled into `libv8.a`, use `bindings.gni` exclude patterns.
