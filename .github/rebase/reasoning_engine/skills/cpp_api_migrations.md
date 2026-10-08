# Cpp Api Migrations

## Core Philosophy: Design and Build Cobalt on the New Chromium Codebase
The goal during a milestone rebase is **not** to memorize static "use A over B" replacements or to freeze Cobalt on old Chromium APIs behind compatibility wrappers. **The goal is to design and build Cobalt natively on the new Chromium codebase:**
1. **First Attempt — Always Accept Upstream Chromium Changes**: When an upstream type, parameter, or return signature changes, inspect how upstream Chromium migrated its own first-party callers in the new commit (`TOOL_UPSTREAM_DIFF` / `TOOL_GREP`) and migrate Cobalt's code (`cobalt/`, `starboard/`, and Cobalt hooks in `content/`/`blink/`) to follow the exact same new Chromium pattern.
2. **Fallback Only If Migration Fails — MUST Raise a Flag for Human Review**: Only if migrating Cobalt to the new Chromium API fails after attempting it may you fall back to keeping Cobalt on the legacy type/signature or adding a local adapter. Whenever you fall back, you **must** leave an explicit human-review flag directly above the code:
   `// TODO(cobalt-rebase): [HUMAN_REVIEW_REQUIRED] Kept legacy Cobalt pattern instead of new Chromium <API>: <reason>`

### 1. Detecting and Propagating API Signature Changes

A frequent source of breakage during a rebase is the modification of function or method signatures in the upstream codebase.

**Detection Strategy:**
1.  If a build fails with a "no matching function for call" or "too few/many arguments" error, this strongly indicates a signature change.
2.  Use `TOOL_UPSTREAM_DIFF` on the header file where the function is declared. Look for changes to the function's parameter list, return type, or name.

**Resolution Strategy:**
1.  **Renaming:** If the function was simply renamed (e.g., `SetPrimaryMainFrameImportance` -> `SetPrimaryPageImportance`), update all call sites with the new name.
2.  **Added Parameters:** If new parameters were added, update the call site to pass them. For new parameters, you may need to:
    *   Infer a sensible default value (e.g., `ChildProcessImportance::NORMAL`).
    *   Look at upstream callers or test files (`..._unittest.cc`, `..._browsertest.cc`) to see how the new API is used in the new Chromium codebase.
3.  **Removed Parameters:** Simply remove the corresponding arguments from the call site.
4.  **Propagating to Overrides and Cobalt Implementations:** If your downstream code overrides a virtual method or delegates to a `cobalt::` implementation function, you **must** update the signature in the `cobalt/` header (`.h`) and implementation (`.cc`) files to match the new Chromium contract directly, rather than wrapping the legacy `cobalt::` signature in an adapter.

### 2. Adopting Upstream Type Migrations (e.g., JNI `JavaParamRef<T>` -> `base::android::JavaRef<T>`)
- **Principle**: When an upstream type alias or wrapper is deprecated/removed (such as `base::android::JavaParamRef<T>`), do not switch to an internal implementation detail (`jni_zero::JavaParamRef<T>`) just to keep the old name compiling. Check what upstream Chromium replaced `base::android::JavaParamRef<T>` with across `base/android/` and `content/browser/android/` (`base::android::JavaRef<T>`) and adopt the new upstream type.
- **Real Example (`cobalt/browser/android/mojo/cobalt_interface_registrar_android.cc`)**:
  ```cpp
  // [BAD] Bypassing upstream's base::android::JavaRef migration by reaching into internal jni_zero::JavaParamRef:
  static void JNI_CobaltInterfaceRegistrar_RegisterMojoInterfaces(
      JNIEnv* env,
      const jni_zero::JavaParamRef<jobject>& j_web_contents)

  // [GOOD] Accepting upstream Chromium's migration to base::android::JavaRef:
  static void JNI_CobaltInterfaceRegistrar_RegisterMojoInterfaces(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& j_web_contents)
  ```

### 3. `std::optional<T>` to Value-Type Getter Migrations (e.g., `gfx::HDRMetadata`)
- **Principle**: When upstream changes a getter from returning `std::optional<T>` to returning `T` (or `const T&`) directly (e.g., `VideoDecoderConfig::hdr_metadata()`, `StreamParserBuffer::GetHDRMetadata()`), inspect how upstream Chromium replaced `.has_value()` at its own call sites.
- **Resolution**:
  - Replace `.has_value()` checks with the type's semantic emptiness check used by upstream (`!config.hdr_metadata().IsEmpty()`), NOT `.IsValid()` (which checks strict SMPTE validation rules rather than whether metadata was populated).
  - Replace `*config.hdr_metadata()` / `.value()` with `config.hdr_metadata()`.
- **Real Example (`cobalt/media/service/VideoGeometrySetterService.cc`)**:
  ```cpp
  // [BAD] Using .IsValid() (changes runtime semantics from presence check to validation check):
  if (config.hdr_metadata().IsValid()) { ... }

  // [GOOD] Matching upstream Chromium's migration from .has_value() to !IsEmpty():
  if (!config.hdr_metadata().IsEmpty()) { ... }
  ```

### 4. Cross-Layer Interface Signature Propagation (e.g., `DecodeAudioFileData`)
- **Principle**: When upstream Chromium changes a platform/renderer interface signature in `content/` or `blink/` (e.g., changing `RendererBlinkPlatformImpl::DecodeAudioFileData` from `bool(WebAudioBus*, ...)` to `std::unique_ptr<WebAudioBus>(...)`), propagate the new Chromium signature all the way into `cobalt/` instead of keeping `cobalt/` on the legacy signature.
- **Real Example (`cobalt/media/audio/audio_decoder.{h,cc}`)**:
  ```cpp
  // [BAD] Leaving bool DecodeAudioFileData(blink::WebAudioBus* destination_bus, ...) on the old signature
  // and adding a heap-allocating wrapper in content/renderer/renderer_blink_platform_impl.cc.

  // [GOOD] Updating cobalt/media/audio/audio_decoder.{h,cc} to build natively on the new Chromium signature:
  std::unique_ptr<blink::WebAudioBus> DecodeAudioFileData(
      base::span<const char> data, float sample_rate) {
    CobaltAudioBusWriter writer(sample_rate);
    if (LoadAudioData(data, &writer)) {
      return writer.TakeAudioBus();
    }
    return nullptr;
  }
  ```
