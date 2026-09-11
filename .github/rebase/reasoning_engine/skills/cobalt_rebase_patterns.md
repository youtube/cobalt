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

---

### Host Toolchain vs Target Runtime Resource Mismatches (Host Action Failures)

Cobalt optimizes and strips runtime data bundles (such as `third_party/icu/cobalt/icudtl.dat`, timezone data, or fonts) to minimize memory and binary footprint for embedded TV platforms.

1. **Architectural Principle: Host Tools Require Upstream Resources**:
   - Host tools (`current_toolchain == host_toolchain`, such as `clang_x64/character_data_generator`, V8 snapshot tools, or font generators) execute on the Linux compile host during build time.
   - Host tools expect complete upstream data and metadata (e.g., CLDR localization tables, full Unicode maps).
   - Stripped Cobalt datasets are strictly intended for the **target runtime device** (`current_toolchain != host_toolchain`).

2. **Crucial Rule: Never Patch Upstream Host Generators**:
   - When a host build tool crashes during code generation (e.g. `Check failed: U_SUCCESS(error)` or missing resource error), do NOT patch the upstream C++ source file.
   - Modifying upstream tools creates unnecessary divergence and will be blocked by the third-party safety guard:
     `[GUARD] Rejecting patch on unmodified third-party source file: ... Patch the referencing BUILD.gn instead.`
   - The root cause is almost always that a Cobalt-specific GN build argument or override is inadvertently applying target settings or stripped datasets to the host toolchain.

3. **Resolution Strategy: Scope Cobalt Overrides to Target Toolchains in `BUILD.gn`**:
   - In the referencing `BUILD.gn`, scope Cobalt data and configuration overrides using `current_toolchain != host_toolchain`.
   - Host toolchains (`current_toolchain == host_toolchain`) should fall through to standard upstream defaults.

4. **Case Study: ICU Data & Blink Host Actions**:
   - **Symptom**: `character_data_generator` crashes during `compiled_action("character_data")`:
     ```text
     [FATAL:third_party/blink/renderer/platform/text/character_property_data_generator.cc:48] Check failed: U_SUCCESS(error). ulocdata_getCLDRVersion: (2)U_MISSING_RESOURCE_ERROR
     ```
   - **Root Cause**: `third_party/icu/BUILD.gn` set `data_dir = "cobalt"` globally, copying the stripped runtime ICU table into `clang_x64/icudtl.dat`.
   - **Fix in `third_party/icu/BUILD.gn`**:
     ```gn
     FILE: third_party/icu/BUILD.gn
     <<<<<<< SEARCH
     if (is_cobalt) {
       data_dir = "cobalt"
     } else if (is_android) {
     =======
     if (is_cobalt && current_toolchain != host_toolchain) {
       data_dir = "cobalt"
     } else if (is_android) {
     >>>>>>> REPLACE
     ```
   - This ensures `clang_x64/icudtl.dat` receives the full common ICU dataset containing CLDR tables, allowing `character_data_generator` to succeed with zero modifications to third-party Blink code.

---

### JNI Zero Registration & Missing Native Stubs (`libchrobalt__jni_registration`)

Chromium milestones frequently introduce Java classes annotated with `@NativeMethods` (such as `PermissionsAndroidFeatureMap`, `WebXrAndroidFeatureMap`, `CameraAvailabilityObserver`, `VideoCapture`) into transitive APK dependencies. However, Cobalt strips or does not link their native C++ implementations into `libchrobalt.so`.

1. **Symptom**:
   Action failure on `//cobalt/android:libchrobalt__jni_registration`:
   ```text
   FAILED: ./gen/cobalt/android/libchrobalt__jni_registration.srcjar ACTION //cobalt/android:libchrobalt__jni_registration(//build/toolchain/android:android_clang_arm)
   python3 ../../third_party/jni_zero/jni_zero.py generate-final ...
   stderr:
   Failed JNI assertion!
   We reference Java files which use JNI, but our native library does not depend on the corresponding generate_jni().
   To bypass this check, add stubs to Java with --add-stubs-for-missing-jni.
   Excess Java files:
   ../../components/permissions/android/java/src/org/chromium/components/permissions/PermissionsAndroidFeatureMap.java
   ../../components/webxr/android/java/src/org/chromium/components/webxr/WebXrAndroidFeatureMap.java
   ../../media/capture/video/android/java/src/org/chromium/media/CameraAvailabilityObserver.java
   ../../media/capture/video/android/java/src/org/chromium/media/VideoCapture.java
   ```

2. **Root Cause & Architectural Mapping**:
   - The failing action `//cobalt/android:libchrobalt__jni_registration` is generated internally by the GN template `shared_library_with_jni("libchrobalt")` from `//third_party/jni_zero/jni_zero.gni`.
   - There is **NO standalone target** named `generate_jni("libchrobalt__jni_registration")` or `action("libchrobalt__jni_registration")` in `cobalt/android/BUILD.gn`.
   - When `jni_zero.py generate-final` runs, it checks whether all Java classes with native methods referenced by `cobalt_apk` have registered native methods. If unlinked classes are found, it asserts unless stub generation is enabled.

3. **Resolution in `cobalt/android/BUILD.gn`**:
   - In `cobalt/android/BUILD.gn`, locate `shared_library_with_jni("libchrobalt")` (which already defines `remove_uncalled_jni = true`).
   - Add `add_stubs_for_missing_jni = true` directly inside `shared_library_with_jni("libchrobalt")`:
     ```gn
     FILE: cobalt/android/BUILD.gn
     <<<<<<< SEARCH
     shared_library_with_jni("libchrobalt") {
       remove_uncalled_jni = true
     =======
     shared_library_with_jni("libchrobalt") {
       remove_uncalled_jni = true
       add_stubs_for_missing_jni = true
     >>>>>>> REPLACE
     ```
   - GN template `shared_library_with_jni` forwards `add_stubs_for_missing_jni` to `generate_jni_registration`, which appends `--add-stubs-for-missing-native` to `jni_zero.py generate-final`. This generates native stubs for the missing bindings and cleanly satisfies the assertion without modifying upstream Java or C++ sources.
