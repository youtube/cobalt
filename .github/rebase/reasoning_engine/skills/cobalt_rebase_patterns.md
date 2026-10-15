# Cobalt Rebase Patterns

### Privacy Sandbox Pruning & Lifecycle Policy (`b/505811196`)

Cobalt is a single-domain application that does **not** use Chromium's Privacy Sandbox features. In `cobalt/build/configs/cobalt.gni`, `enable_privacy_sandbox_apis = !is_cobalt` (`false` on Cobalt), which sets `BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS)` to `0` and strips the following subsystems to save ~1.9MB+ of binary size:
- **Shared Storage** (`content/browser/shared_storage/*`, `third_party/blink/renderer/modules/shared_storage/*`, `SharedStorageWorkletHost`)
- **Ad Auction / Protected Audience / Interest Group** (`content/browser/interest_group/*`, `third_party/blink/renderer/modules/ad_auction/*`)
- **Attribution Reporting / Private Aggregation / Aggregation Service** (`content/browser/attribution_reporting/*`, `content/browser/private_aggregation/*`, `content/browser/aggregation_service/*`)
- **Browsing Topics** (`content/browser/browsing_topics/*`, `components/browsing_topics/*`)
- **Fenced Frames** (`content/browser/fenced_frame/*`, `fenced_frame_viewport_observer`)
- **Private State Tokens / Trust Tokens** (`services/network/trust_tokens/*`)
- **IP Protection / Masked Domain List / Probabilistic Reveal Tokens** (`components/ip_protection/*`)
- **FedCM / WebID & Credential Management** (`content/browser/webid/*`, `third_party/blink/renderer/modules/credentialmanagement/*`, `protocol::FedCmHandler`)
- **Storage Access API** (`third_party/blink/renderer/modules/storage_access/*`)

The **core goal** for Privacy Sandbox during any rebase is to **keep Privacy Sandbox code stripped from Cobalt** and **delete Cobalt's temporary `#if BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS)` / `if (enable_privacy_sandbox_apis)` blocks as soon as upstream Chromium itself deletes those features**.

#### 1. Scenario A — Upstream Chromium Deletes a Privacy Sandbox Feature: DELETE the Guarded Block Completely!
Cobalt's `#if BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS)` (often combined with `&& CHROMIUM_MILESTONE_LE_138` or `&& CHROMIUM_MILESTONE_LE_150`) and `if (enable_privacy_sandbox_apis)` blocks exist **only** to strip Privacy Sandbox code before upstream deletes it (as documented in `BUILD.gn`: `# If in the future privacy sandbox components are removed from the code base, ignore this if block`).
- When upstream Chromium deletes a Privacy Sandbox member, method, `#include`, or GN `deps` entry (`<<<< HEAD` removes it), **ACCEPT the upstream deletion completely** and delete Cobalt's `#if BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS)` / `if (enable_privacy_sandbox_apis)` block too!
- **NEVER** resurrect upstream-deleted Privacy Sandbox code inside a Cobalt `#if` block.

**Real Example 1 — Upstream Deleting IP Protection in C++ (`services/network/network_context.h` & `network_service.cc`, Igalia M144 `#12760` / `#12818`)**:
```cpp
// [BAD] Resurrecting upstream-deleted IP Protection members/includes inside Cobalt's #if block:
#if BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS) && CHROMIUM_MILESTONE_LE_150
#include "components/ip_protection/common/ip_protection_core.h"  // nogncheck
#endif
...
#if BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS) && CHROMIUM_MILESTONE_LE_150
  ip_protection::IpProtectionCore* ip_protection_core() {
    return ip_protection_core_.get();
  }
  std::unique_ptr<ip_protection::IpProtectionCore> ip_protection_core_;
#endif

// [GOOD] Upstream deleted ip_protection_core_, masked_domain_list_manager_, and
// probabilistic_reveal_token_registry_ in M144. Delete the entire #if block completely!
```

**Real Example 2 — Upstream Deleting IP Protection Deps in GN (`services/network/BUILD.gn`, Igalia M144/M145 `#12871`)**:
```gn
# [BAD] Keeping removed //components/ip_protection/common:* targets in BUILD.gn:
  if (enable_privacy_sandbox_apis) {
    deps += [
      "//components/ip_protection/common:ip_protection_core_host_remote",
      "//components/ip_protection/common:ip_protection_core_impl",
      ...
      "//services/network/trust_tokens",
    ]
  }

# [GOOD] Removing the deleted ip_protection targets and keeping only what still exists upstream:
  if (enable_privacy_sandbox_apis) {
    deps += [ "//services/network/trust_tokens" ]
  }
```

#### 2. Scenario B — Upstream Chromium Adds New References to a Stripped Privacy Sandbox Subsystem: Gate the Caller!
When upstream adds a new function, Mojo binder, DevTools handler, or call site referencing a stripped Privacy Sandbox class, **NEVER re-add excluded Privacy Sandbox `.cc` files or `deps` to `BUILD.gn`**. Instead, gate the caller under `#if BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS)`:

- **Pattern B1 — Function Signature Uses a Stripped Class (`content/browser/network/reporting_service_proxy.{h,cc}`, Igalia M144 `#12818`)**:
  ```cpp
  // [GOOD] Gate both the header declaration and .cc implementation when a parameter
  // type (SharedStorageWorkletHost) belongs to a stripped Privacy Sandbox subsystem:
  #if BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS) && CHROMIUM_MILESTONE_LE_150
  void CreateReportingServiceProxyForSharedStorageWorklet(
      SharedStorageWorkletHost* shared_storage_worklet_host,
      mojo::PendingReceiver<blink::mojom::ReportingServiceProxy> receiver);
  #endif  // BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS) && CHROMIUM_MILESTONE_LE_150
  ```

- **Pattern B2 — `void` Hook or Pointer-Returning Getter (`content/browser/devtools/devtools_instrumentation.cc` & `browser_context.cc`, PR `#11535`)**:
  ```cpp
  // [GOOD] Guard the header include (with // nogncheck if needed) and function body:
  #if BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS)
  #include "content/browser/devtools/protocol/fedcm_handler.h"  // nogncheck
  #endif  // BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS)

  void WillSendFedCmRequest(RenderFrameHost& render_frame_host,
                            bool* intercept,
                            bool* disable_delay) {
  #if BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS)
    FrameTreeNode* ftn = FrameTreeNode::From(&render_frame_host);
    if (!ftn) {
      return;
    }
    DispatchToAgents(ftn, &protocol::FedCmHandler::WillSendRequest, intercept,
                     disable_delay);
  #endif  // BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS)
  }

  FederatedIdentityPermissionContextDelegate*
  BrowserContext::GetFederatedIdentityPermissionContext() {
  #if BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS)
    return impl()->GetFederatedPermissionContext();
  #else
    return nullptr;
  #endif
  }
  ```

- **Pattern B3 — Async Mojo Method Taking a Callback (`services/network/network_context.cc`, PR `#11509`)**:
  ```cpp
  // [GOOD] Always run the Mojo callback with an empty/disabled result in the #else branch
  // so callers do not hang waiting for a dropped Mojo callback:
  void NetworkContext::ClearTrustTokenData(mojom::ClearDataFilterPtr filter,
                                           base::OnceClosure done) {
  #if BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS)
    if (!trust_token_store_) {
      std::move(done).Run();
      return;
    }
    ...
  #else
    std::move(done).Run();
  #endif  // BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS)
  }
  ```

- **Pattern B4 — New Blink IDL Bindings for a Stripped Privacy Sandbox Module (`third_party/blink/renderer/bindings/bindings.gni`, PR `#11535` & Igalia M141)**:
  - Never edit generated V8 binding files under `out/*/gen/`. Add the wildcard pattern (e.g., `"*fed_cm*"`) to `cobalt_bindings_exclude_patterns` under `if (!enable_privacy_sandbox_apis)` in `third_party/blink/renderer/bindings/bindings.gni`.

#### 3. Scenario C — Upstream Refactors Code *Inside* an Active `#if BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS)` Block
When upstream keeps a Privacy Sandbox feature but renames a class, moves a header, or updates a call signature inside a block Cobalt gated with `#if BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS)`:
- Keep the `#if BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS)` guard, and **update the code inside the guard to match the new upstream Chromium code** (for example, updating `#include "content/browser/webid/federated_auth_request_impl.h"` and `FederatedAuthRequestImpl::Create(...)` to `#include "content/browser/webid/request_service.h"` and `webid::RequestService::Create(...)`).
- Consolidate adjacent `#if BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS)` functions or `#include` lines into a single `#if ... #endif` block rather than splitting every line into separate `#if` blocks, and never concatenate `// nogncheck` with the next line's `#include`.

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

5. **Never Patch `build/` Scripts to Work Around the Build Host Environment**:
   - If a Python build script under `build/` (for example `build/android/gyp/**`) fails because of the machine running the build (the installed protobuf/python package version, missing host tools, PATH, locale), do NOT edit the script, and do NOT set environment variables inside it. The script is upstream code and works in Chromium's pinned environment, so the root cause is the CI container or the vpython spec, not the rebase.
   - Do not commit any such workaround. Report it in `result.md` and raise a flag (`ESCALATE_TO_HUMAN` / `[HUMAN_REVIEW_REQUIRED]`) naming the host dependency that needs fixing.
   - Only edit a `build/` script when the failure comes from the rebase itself (an upstream API or path change that Cobalt code depends on), and say so in the commit.
   - **Real Example (`build/android/gyp/util/protoresources.py`, M145.7632 AI #13054 and M146.7644 AI #13072)**:
     ```python
     # [BAD] AI (both milestones): environment workaround baked into upstream code.
     # Ensure legacy protoc-generated _pb2 files can be imported on protobuf >= 4.21
     os.environ.setdefault('PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION', 'python')

     # [GOOD] Human: no change to protoresources.py. Igalia removed the line in
     #        M146.7644 (#13071). Fix the CI host's protobuf instead.
     ```

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

3. **Resolution (Default): Link the Real C++ Implementation into `libchrobalt`**:
   - For each file listed under `Excess Java files`, find the C++ `generate_jni` target and the `source_set` that implements its native methods (`TOOL_GREP: <ClassName>_jni.h`), and add that implementation target to the `libchrobalt` deps in `cobalt/android/BUILD.gn`. Add a comment naming the upstream CL that pulled the Java class in.
   - If the upstream implementation target drags in heavy deps that Cobalt strips (or creates a dependency cycle), add a minimal Cobalt-only target under `if (is_cobalt)` next to it that contains just the JNI C++ side, and depend on that instead.
   - Real Example (`cobalt/android/BUILD.gn` and `components/permissions/android/BUILD.gn`, M143.7471, Human #12591):
     ```gn
     # cobalt/android/BUILD.gn -- libchrobalt deps
     # Added in 143.7471: https://crrev.com/c/6996548 added a dependency on
     # //components/permissions/android code to //services/device/geolocation.
     # //media/capture/video/android comes from https://crrev.com/c/7008390.
     "//components/permissions/android:core",
     "//components/webxr/android:features",
     "//media/capture/video/android",

     # components/permissions/android/BUILD.gn -- Cobalt-only minimal target
     if (is_cobalt) {
       source_set("core") {
         sources = [
           "permissions_android_feature_map.cc",
           "permissions_android_feature_map.h",
         ]
         deps = [
           ":core_jni",
           "//base",
           "//components/content_settings/core/common:features",
           "//components/permissions:permissions_common",
           "//media",
           "//third_party/blink/public/common",
         ]
       }
     }

     # components/permissions/BUILD.gn -- move the files out of the original
     # target so the symbols are defined exactly once
     if (is_cobalt && is_android) {
       sources -= [
         "android/permissions_android_feature_map.cc",
         "android/permissions_android_feature_map.h",
       ]
       public_deps += [ "//components/permissions/android:core" ]
     }
     ```
   - Why the carve-out is needed: linking the full `//components/permissions` target into `libchrobalt` creates a circular dependency and pulls in code Cobalt strips. The carve-out is a direct consequence of NOT stubbing (item 4): once you choose to link the real JNI implementation, you will often have to split out a minimal target like this.
   - A carve-out always has two halves. Do both, or you get duplicate symbols (files compiled in two targets) or missing symbols (files removed but no dep added):
     1. Add the Cobalt-only minimal `source_set` under `if (is_cobalt)` with just the JNI implementation files and their direct deps.
     2. In the original target, under `if (is_cobalt && is_android)`, `sources -=` those files and `public_deps +=` the new target so existing dependents still get the symbols.

4. **Last Resort Only: `add_stubs_for_missing_jni = true`, Always Flagged**:
   - `add_stubs_for_missing_jni = true` makes `jni_zero` generate empty native stubs so the assertion passes. The build succeeds, but any Java call into those methods at runtime reaches a stub with no real logic. This is a global bypass that hides missing functionality, so it is NOT an acceptable default (see `gn_healing.md` Rule 6).
   - Use it only if linking the real implementation is impossible (for example, the implementation depends on code Cobalt fundamentally cannot build). When you do, you MUST add a `[HUMAN_REVIEW_REQUIRED]` flag listing every Java class being stubbed and why the real target could not be linked:
     ```gn
     shared_library_with_jni("libchrobalt") {
       remove_uncalled_jni = true
       # TODO(cobalt-rebase): [HUMAN_REVIEW_REQUIRED] Stubbing JNI for
       # <ClassName>.java because <reason the real target cannot be linked>.
       # Confirm Cobalt never calls these native methods at runtime.
       add_stubs_for_missing_jni = true
     ```
   - [BAD] (AI #12593, M143.7471): added `add_stubs_for_missing_jni = true` with no flag instead of linking `//components/permissions/android:core`, `//components/webxr/android:features`, and `//media/capture/video/android`.


---

### Mojom `[EnableIf]` Members Stripped by a Split `mojom()` GN Target

Cobalt gates Starboard-only mojom fields, enum values, and interfaces behind `[EnableIf=use_starboard_media]`. The mojom bindings generator only keeps an `[EnableIf=foo]` entity if the **specific `mojom()` GN target that lists that `.mojom` file in its `sources`** declares `foo` in its `enabled_features`. `enabled_features` is **per-target, not global and not inherited through `deps` or `public_deps`**.

Chromium milestones routinely split one large `mojom()` target into several smaller ones. When a `.mojom` file migrates to a newly created target, upstream has no reason to copy Cobalt's `enabled_features`, so every `[EnableIf]` entity in that file silently disappears from the generated bindings.

1. **Symptom**:
   Compile errors in **checked-in `*_mojom_traits.h` / `*_mojom_traits.cc`** files that reference a member which is plainly present in the `.mojom` source:
   ```text
   media/mojo/mojom/media_types_enum_mojom_traits.h:455:44: error: no member named 'kStarboard' in 'media::mojom::RendererType'
           return media::mojom::RendererType::kStarboard;
   ```
   Other shapes of the same root cause:
   ```text
   error: no member named 'ReadMimeType' in 'media::mojom::AudioDecoderConfigDataView'
   error: no member named 'mime_type' in 'media::mojom::VideoDecoderConfig'
   ```

2. **Critical Anti-Patterns (these waste the entire iteration budget)**:
   - **DO NOT** add the member to the `.mojom` file. It is already there. Re-adding it produces a `SEARCH` block that can never match, or a duplicate-definition error. Always `TOOL_GREP: <member_name> <path_to_mojom>` to confirm it exists before concluding it is missing.
   - **DO NOT** edit the generated header under `out/*/gen/`. It is a build artifact and will be regenerated.
   - **DO NOT** delete the `#if` / `if (is_cobalt && use_starboard_media)` guarded block, the traits header reference, or the `cpp_typemaps` entry in order to make the error go away. Deleting Cobalt platform code to silence a build error is a regression, not a fix, even though it compiles and passes GN gen.
   - **DO NOT** trust a remembered line number for the enum or struct. Read the file.

3. **Diagnosis Procedure**:
   - Confirm the entity exists and is feature-gated:
     ```text
     TOOL_GREP: EnableIf media/mojo/mojom/media_types.mojom
     ```
   - Identify which `mojom()` target owns the `.mojom` file. Search for the filename inside `sources` lists, not just the target name:
     ```text
     TOOL_GREP: media_types.mojom media/mojo/mojom/BUILD.gn
     ```
   - Inspect `enabled_features` on **that** target. If the target has no `enabled_features` block at all, or has one that omits the required feature, that is the bug:
     ```text
     TOOL_GREP: enabled_features media/mojo/mojom/BUILD.gn
     ```
   - Compare against the pre-roll layout to see which target previously owned the file:
     ```text
     TOOL_UPSTREAM_DIFF: media/mojo/mojom/BUILD.gn
     ```

4. **Resolution**: add the feature to the owning target, guarded exactly as the original target guarded it.
   ```gn
   FILE: media/mojo/mojom/BUILD.gn
   <<<<<<< SEARCH
   mojom("media_types") {
     generate_java = true
     sources = [ "media_types.mojom" ]
   =======
   mojom("media_types") {
     generate_java = true
     sources = [ "media_types.mojom" ]

     enabled_features = []
     if (is_cobalt && use_starboard_media) {
       enabled_features += [ "use_starboard_media" ]
     }
   >>>>>>> REPLACE
   ```
   If the target already declares `enabled_features`, append to it instead of redeclaring it, since GN forbids overwriting a non-empty list.

5. **Generalization**:
   - This applies to any Cobalt mojom feature flag, not only `use_starboard_media`.
   - When a roll splits a `mojom()` target, audit **every** Cobalt-specific attribute on the original target and replicate the relevant ones onto the new target: `enabled_features`, `cpp_typemaps`, `traits_headers`, `traits_public_deps`, and any `if (is_cobalt)` block. Losing a `cpp_typemaps` entry produces a different but equally confusing error about a missing or mismatched typemap.
   - Also ensure `import("//starboard/build/buildflags.gni")` is present at the top of the `BUILD.gn` if the new guard references `use_starboard_media`, otherwise GN gen fails with an undefined-identifier error.

---

### Cobalt Stub & Gold-Build File Synchronization (`devtools_instrumentation_stub.cc`, `cobalt_modules_stubs.cc`)

Cobalt maintains lightweight stub implementations for subsystems that are stripped either in `gold` builds (such as DevTools) or across all Cobalt builds (such as unused Blink modules in `third_party/blink/renderer/modules/cobalt_modules_stubs.cc`). Because `gold`-only stubs are not compiled during standard `devel`/`qa` `autoninja` runs, signature mismatches in those stubs will silently break `gold` builds unless synchronized proactively.

1. **`content/browser/devtools/cobalt/devtools_instrumentation_stub.cc`**:
   - Whenever upstream Chromium changes function or struct method signatures in `content/browser/devtools/devtools_instrumentation.h` or `content/browser/devtools/devtools_instrumentation.cc`, you MUST inspect `content/browser/devtools/cobalt/devtools_instrumentation_stub.cc` and update the corresponding stub signature to match.
   - **Real Example (`content/browser/devtools/cobalt/devtools_instrumentation_stub.cc`, M145)**:
     When upstream added `mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>* header_client` to `WillCreateURLLoaderFactoryParams::Run` in `devtools_instrumentation.{h,cc}`, the stub in `devtools_instrumentation_stub.cc` had to be updated identically:
     ```cpp
     bool WillCreateURLLoaderFactoryParams::Run(
         bool is_navigation,
         bool is_download,
         network::URLLoaderFactoryBuilder& factory_builder,
         ukm::SourceIdObj ukm_source_id,
          scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner,
         mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
             header_client) {
       return false;
     }
     ```

2. **`third_party/blink/renderer/modules/cobalt_modules_stubs.cc`**:
   - When upstream Chromium refactors Blink base classes (such as migrating `Supplement<NavigatorBase>` to trace its base class in `Trace(Visitor*)`, or adding `buffers_with_mailbox_` to `GPUDevice::Trace`), ensure stub classes in `cobalt_modules_stubs.cc` are updated to match.

3. **`third_party/blink/renderer/core/inspector/cobalt/*_stub.cc` & `content/renderer/media/cobalt/inspector_media_event_handler_stub.h`**:
   - Whenever upstream Chromium adds or modifies methods on Blink Inspector agents (`InspectorDOMAgent`, `InspectorNetworkAgent`, `InspectorPageAgent`, `InspectorEmulationAgent`, `InspectorMediaAgent`, `WorkerInspectorController`, `MainThreadDebugger`, `WorkerThreadDebugger`, `MediaInspectorContextImpl`), add or update the corresponding no-op stub definitions in `third_party/blink/renderer/core/inspector/cobalt/*_stub.cc`.

4. **`third_party/blink/renderer/platform/graphics/gpu/cobalt_webgpu_stubs.cc` & `cobalt/browser/android/overlay/cobalt_video_overlay_window.{h,cc}`**:
   - When upstream Dawn/WebGPU C APIs or `content::VideoOverlayWindow` pure virtual methods change (e.g., `wgpuGetInstanceCapabilities` -> `wgpuGetInstanceLimits`, or adding `SetHidePictureInPictureButtonVisibility`), update the matching stub definitions instead of deleting them.

---

### Third-Party Generated Config Headers (`third_party/fontconfig/include/config.h`)

When upstream Chromium migrates a `third_party/` library from a large inline generated `config.h` to a thin Meson/wrapper header (for example, replacing `third_party/fontconfig/include/config.h` with `#include "meson-config.h"` and `#include "config-fixups.h"`), **always accept upstream's new wrapper header** instead of preserving Cobalt's legacy 100+ line `config.h` snapshot. If Cobalt needs platform-specific macro overrides, they belong in the per-platform config or fixup header, not by reverting the upstream wrapper.

---

### Cobalt Android Single-Process Mode, Packaging & Warning Suppressions

1. **Single-Process Mode in `CobaltActivity.java`**:
   - Cobalt on Android strictly runs Chromium in single-process mode. When resolving conflicts or updating `BrowserStartupController.startBrowserProcessesAsync(...)` in `cobalt/android/apk/app/src/main/java/dev/cobalt/coat/CobaltActivity.java`, ALWAYS pass `/* singleProcess= */ true` (never `false`).

2. **Removed Java JARs in `cobalt/build/android/package.json`**:
   - When upstream Chromium removes or folds an Android Java target into `base_java` (for example, removing `obj/base/android_info_java.javac.jar` or `obj/base/jank_tracker_java.javac.jar`), remove its stale entry from `cobalt/build/android/package.json` so Android packaging does not fail on missing JAR inputs.

3. **Global Clang Warnings in `build/config/warning_suppression.txt`**:
   - When upstream Chromium enables new Clang diagnostic flags (such as `-Wexit-time-destructors`), add path suppressions for `cobalt` and `starboard` in `build/config/warning_suppression.txt` (e.g., `src:*{/,\\}cobalt{/,\\}*` and `src:*{/,\\}starboard{/,\\}*`) rather than rewriting static variables across `cobalt/browser/` and `cobalt/shell/` with `base::NoDestructor`.

---

### Upstream Replaces a Silent Fallback with a Hard `CHECK`: Embedder Must Supply the Dependency

Upstream sometimes removes a default/fallback path and replaces it with a `CHECK` that the embedder supplied a required object (a provider, delegate, or factory in a `mojom::*Params` struct). Cobalt's own code (`cobalt/browser/cobalt_content_browser_client.cc`) is the embedder, so nothing in the conflict region changes, the build may even succeed, and Cobalt aborts at startup.

1. **How to detect it**: an upstream diff that deletes a fallback such as `if (!params->foo) foo = GetDefaultFoo();` and adds `CHECK(params->foo)`, or a new required field in `network::mojom::NetworkContextParams` / similar params. Grep for every place Cobalt builds that params struct.
2. **How to fix it**:
   - Look at how another lightweight embedder that also lacks Chrome's full profile stack supplies the object, and mirror it. `chromecast/browser/` is usually the closest reference for Cobalt; `content/shell/` is the next choice.
   - Keep each platform's pre-roll behavior. If a platform never had the feature before the roll, opt that platform out explicitly (using the opt-out field upstream provides) instead of silently turning the feature on for existing user data.
   - Add the GN deps the new objects need to `//cobalt/browser`.
3. **Flag it**: this is a runtime behavior change that compile healing cannot verify, so add `// TODO(cobalt-rebase): [HUMAN_REVIEW_REQUIRED]` describing the chosen behavior per platform.

**Real Example (`cobalt/browser/`, M143.7457, b/559470755)**:
Upstream https://crrev.com/c/6996667 ("Reland: Port net::CookieCryptoDelegate to os_crypt async") made `NetworkContext` `CHECK` for `cookie_encryption_provider` instead of falling back to `cookie_config::GetCookieCryptoDelegate()`. Cobalt aborted at startup with `Check failed: params_->cookie_encryption_provider`. The AI made no change.

```cpp
// [GOOD] Human: supply the provider the chromecast way -- an OSCryptAsync with
// no key providers, which keeps the legacy OSCrypt path Cobalt already used.
// cobalt_content_browser_client.cc (constructor)
os_crypt_async_(std::make_unique<os_crypt_async::OSCryptAsync>(
    std::vector<std::pair<os_crypt_async::OSCryptAsync::Precedence,
                          std::unique_ptr<os_crypt_async::KeyProvider>>>{})),
cookie_encryption_provider_(
    std::make_unique<CookieEncryptionProviderImpl>(os_crypt_async_.get())) {

// cobalt_content_browser_client.cc (network context params)
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS_TVOS)
  // Android had no cookie crypto delegate before the roll; opt out instead of
  // starting to encrypt existing cookie databases (crbug.com/449652881).
  network_context_params->enable_encrypted_cookies = false;
#else
  network_context_params->cookie_encryption_provider =
      cookie_encryption_provider_->BindNewRemote();
#endif
```
```gn
# cobalt/browser/BUILD.gn
deps += [
  "//components/os_crypt/async/browser",
  "//services/network/public/cpp",
]
```

---

### Reusing an Upstream Cast-Only Branch for Starboard (`USE_STARBOARD_MEDIA`)
Cobalt shares several constraints with Chromecast (CastOS): surfaceless output, video rendered as an underlay/punch-out, and a single-process media pipeline. When upstream adds or touches a `#if BUILDFLAG(IS_CASTOS)` branch in code Cobalt also runs (viz overlays, display, media), check whether the reason for the Cast branch also applies to Cobalt. If it does, extend the guard with `BUILDFLAG(USE_STARBOARD_MEDIA)` and write a comment explaining the shared constraint. This is easy to miss because it produces no conflict and no build error; it shows up only at runtime (visual corruption or crashes).

**Real Example (`components/viz/service/display/overlay_processor_ozone.cc`, M145.7577, Human #12871 vs AI #12875)**:
```cpp
// [BAD] AI: no change. Under Starboard media, SkiaRenderer tries to back a
//       primary plane overlay on a surfaceless device that cannot allocate
//       images.
bool OverlayProcessorOzone::ShouldCreatePrimaryPlane() const {
#if BUILDFLAG(IS_CASTOS)
  return false;
#else
  return true;
#endif
}

// [GOOD] Human: reuse the Cast branch and explain why it applies to Cobalt.
bool OverlayProcessorOzone::ShouldCreatePrimaryPlane() const {
#if BUILDFLAG(IS_CASTOS) || BUILDFLAG(USE_STARBOARD_MEDIA)
  // Cobalt reports supports_surfaceless from SkiaOutputDeviceGL, which never
  // sets renderer_allocates_images, so SkiaRenderer cannot back a primary
  // plane overlay.
  return false;
#else
  return true;
#endif
}
```
- Only extend the guard when you can state the shared constraint in the comment. If you cannot, do not guess: add `// TODO(cobalt-rebase): [HUMAN_REVIEW_REQUIRED]` next to the Cast branch asking whether Cobalt needs the same behavior.
