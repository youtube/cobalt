# Cobalt Video Surface & Fullscreen Hole-Punching Investigation Plan

## 1. Executive Summary & Core Findings
During rapid background/foreground transitions (`HOME && am start` / `repro_thumbnail_playback.sh`), the physical TV screen (`Z = 0` Main UI canvas) visually freezes showing the old Home Screen (`Recommended` shelf with `Search` bar and a `720x404` thumbnail hole), even though Chromium/Viz successfully transitions to the Fullscreen Video Playback page (`0,0 1920x1080`) and `SkiaRenderer::SwapBuffers(1920x1080)` completes (`DidReceiveSwapBuffersAck -> swap result=0`).

### The Root Cause: Z-Order Inversion of Orphaned SurfaceView (`rel -1`)
Live inspection via `dumpsys SurfaceFlinger` proved that this issue happens **only** when rapid relaunch leaves an orphaned `VideoSurfaceView` (`SurfaceView #1 / #3` at `RGBA_8888`) wedged at **Z-order `rel -1`** (directly below the `Z = 0` UI canvas and directly above the `rel -2` active hardware video plane). That lingering `rel -1` surface physically masks the `1920x1080` active video surface beneath it.

---

## 2. Architectural & Historical Context

### A. The Cobalt Rebase Timeline (Why the Bug Started in C26)
* **C25 (No Issue - Old Chromium Fork):**
  Cobalt ran on an older, specialized Chromium fork where the graphics and video window lifecycle (`video_window.cc` / `video_decoder.cc`) operated directly at the Starboard/JNI boundary without modern Chromium compositor delegation. When a `SurfaceView` was replaced, `WindowManagerService` stacked the windows cleanly without leaving any orphan above the active video (`rel -1` remained empty; orphans sat at/below `rel -2 / -3`).
* **C26 (Issue Introduced - Chromium m114 Rebase):**
  When Cobalt underwent the major structural rebase to **Chromium m114**, the entire compositing architecture transitioned to Chromium's modern **Viz (`components/viz`) compositor engine, `OverlayProcessor`, `OverlayStrategyUnderlayStarboard`, `SkiaOutputSurface`, and Mojo IPC (`VideoGeometrySetterService`)**. That structural shift decoupled frame rendering, overlay/underlay geometry (`DrawQuad::Material::kVideoHole`), and `MediaCodec` from the synchronous Java view lifecycle across IPC. That exact shift is when the `rel -1` orphaned window stacking inversion began.
* **C27 (Current Main - Chromium m138 Rebase):**
  Inheriting the exact same modern Viz / `OverlayStrategyUnderlayStarboard` architecture from C26, C27 continues to trigger the same behavior during rapid transitions (`HOME && am start`), leaving the old detached `SurfaceView (#1/#3)` wedged at `rel -1`.

### B. The Android OS Dependency (Android 11 vs Android 14)
* **Why Android 11 (API 30) is Vulnerable:**
  On Android 11, `SurfaceView` window creation, Z-ordering, and destruction (`removeView()` / `addView()`) rely on legacy `WindowManagerService` (`WMS`) Binder IPCs (`IWindowSession.remove()`, `relayoutWindow()`). When `createNewSurfaceView()` (`removeView` + `addView`) runs on the UI thread while C27's Viz/Mojo/Codec threads are asynchronously managing surface buffers, Android 11’s `WMS` processes the detachment of the old `SurfaceView (#1)` asynchronously. When the new `SurfaceView (#2)` asks `WMS` for a `TYPE_APPLICATION_MEDIA` window while the old `SurfaceView (#1)` is still undergoing asynchronous teardown across C27's threads, **Android 11’s legacy `WMS` inserts the new child (`#2`) at the bottom of the `TYPE_APPLICATION_MEDIA` sub-stack (`rel -2`), stranding the old detached window (`#1`) directly above it at `rel -1`**.
* **Why Android 14 (API 34) is Immune:**
  Between Android 12 and Android 14, Android OS completely overhauled `SurfaceView`, `ViewRootImpl`, and window compositing to use **`BLASTBufferQueue` and synchronized `SurfaceControl.Transaction` (`BLASTSyncEngine` / `SyncRtSurfaceTransactionApplier`)**. On Android 14, `ViewRootImpl` packages both the removal of the old `SurfaceView` window and the Z-ordering of the new `SurfaceView` window into a **single, synchronized `SurfaceControl.Transaction`** applied atomically with the root UI frame draw. Because Android 14 atomically synchronizes `SurfaceView` detachment and Z-ordering right at the `ViewRootImpl` transaction boundary, `SurfaceFlinger` **never** experiences the intermediate/orphaned state where the old detached `SurfaceView` sits at `rel -1` over the new active surface (`rel -2`).

---

## 3. Verified Data & Log Evidence (`task-575`)
To confirm whether the C++ native window release (`ANativeWindow_release`) was racing or running *after* Java's `removeView()`, exact `SB_LOG(INFO)` instrumentation was added to `JNI_VideoSurfaceView_OnVideoSurfaceChanged()`.

### Verification Verdict
Across all 5 surface re-creation / destruction cycles in `log.cobalt.latest.txt`, `ANativeWindow_release(g_native_video_window)` completed cleanly and strictly in order, exactly 2 to 3 milliseconds **before** `removeView()` detached the Java view. 
* **Conclusion:** The C++ and Java API calls on the Starboard boundary are ordered cleanly. Both C25 and C27 use identical `VideoSurfaceView` construction and layout parameters (`TYPE_APPLICATION_MEDIA`, added at index `0`). The root differentiator between C25 and C27 is how the **decoupled cross-thread timing between Viz/Mojo/Codec and the UI thread** interacts with Android 11's legacy `WindowManagerService` child window insertion order during view replacement (`removeView` vs `addView`).

---

## 4. Empirical Evaluation & Written-Off Theories (From `~/cobalt.m138/src/findings*.md` & `task-710`)
Based on extensive empirical device testing across 30+ logs and specific hypothesis validations documented in `/usr/local/google/home/kjyoun/cobalt.m138/src/findings.md` through `findings.6.md` and `task-710`, the following theories and plans are **conclusively written off**:

### ~~Written-Off Plan Option A: Preserving `VideoSurfaceView` & Synchronous Native Clearing (`Media.ForceClearSurfaceView=1`)~~
* **Hypothesis Tested:** Does skipping `createNewSurfaceView()` (`removeView` / `addView` window churn) by clearing (`ClearNativeWindow`) and preserving the same `VideoSurfaceView` across transitions resolve the occlusion?
* **Why It Was Written Off (`findings.2.md`, `findings.3.md`, `findings.4.md`):** Expanding the video from thumbnail (`720x404`) to fullscreen (`1920x1080`) still requires calling `setVideoSurfaceBounds(0, 0, 1920, 1080)` (`setLayoutParams`). On pre-BLAST Android 11, resizing via `setLayoutParams()` commands `WindowManagerService` to run an independent `Session.relayout` solely on `VideoSurfaceView`. Because the sibling web surface (`ContentViewRenderView`) does not participate in that single-window relayout, its `TRANSLUCENT` punch-out alpha state in `SurfaceFlinger` gets desynchronized. The moment `ContentViewRenderView` pushes its next EGL frame (`eglSwapBuffers`), `SurfaceFlinger` draws the web UI opaquely (`alpha = 1.0`) over the resized video plane.

### ~~Written-Off Plan Option B: Staged View Replacement (`surfaceDestroyed` Sync across 50ms)~~
* **Hypothesis Tested:** Does detaching `oldView`, waiting unconditionally for `surfaceDestroyed()` and a 50ms delay (`postDelayed`) so `WMS` completes Binder IPC detachment, and only then calling `addView(newView)` prevent `WMS` from inserting `#2` under `#1` on Android 11?
* **Why It Was Written Off (`task-710` log verification):** Even when `oldView` (`#1`) is 100% removed (`removeView`), `ANativeWindow_release` has completed, zero `VideoSurfaceView` exists on the screen for over 56 milliseconds, and a brand-new `VideoSurfaceView` (`#2`) is attached at index `0`... when `setVideoSurfaceBounds(0, 0, 1920, 1080)` resizes `#2`, `WMS` on Android 11 still ends up with `ContentViewRenderView` (`#1` at index 1) failing to composite the `TRANSLUCENT` punch-out hole over the new `VideoSurfaceView`. Independent window replacement (`createNewSurfaceView()`) or resizing on `VideoSurfaceView` alone cannot force `SurfaceFlinger` on Android 11 to re-bind the transparent hole with `ContentViewRenderView`.

### ~~Written-Off Plan Option C: Explicit Z-Order / Visibility & User-Space Repaint Enforcements~~
* **Hypothesis Tested:** Can explicit Z-ordering (`setZOrderOnTop`), format/visibility toggles (`INVISIBLE -> VISIBLE`), or user-space Skia repaints (`SetNeedsComposite`) force `SurfaceFlinger` to demote the orphan or re-bind the transparent hole without window replacement?
* **Why It Was Written Off (`findings.3.md`, `findings.5.md`, `findings.6.md`):**
  1. **User-Space Skia Repaints (`SetNeedsComposite`):** Pushing delayed transparent EGL frames from user space (`TriggerPostLayoutRepaint`) after `setLayoutParams` does not command `WindowManagerService` or `SurfaceFlinger` inside the OS kernel to re-synchronize Child 1's window state, leaving the transparent hole frozen (`findings.6.md` Candidate 1).
  2. **Clean Web `Session.relayout` Refresh (`setZOrderOnTop(true)` + `requestLayout()`):** Calling `setZOrderOnTop(true)` + `requestLayout()` when dimensions and format haven't changed (`sizeChanged=false`, `formatChanged=false`) does not force `WindowManagerService` to reconstruct the physical `ANativeWindow` (`EGLSurface`), so `SurfaceFlinger` layer blending does not update (`findings.6.md` Candidate 3).
  3. **Visibility Toggling (`INVISIBLE -> VISIBLE`):** Android's UI traversal (`performTraversals()`) coalesces `INVISIBLE -> VISIBLE` state changes occurring in the same turn or across adjacent tasks without executing kernel-level window teardown (`findings.3.md`).
  4. **Locking `VideoSurfaceView` to Fullscreen (`1920x1080`):** Permanently locking the video window to `1920x1080` avoids `Session.relayout` churn, but causes cropped/partial thumbnail video because Amlogic's hardware decoder does not perform sub-rect scaling inside a fixed physical `SurfaceView` buffer without explicit `setLayoutParams(...)` resizing (`findings.6.md` Candidate 2).
  5. **Switching to Chromium `DialogOverlayCore` (`CobaltUsingAndroidOverlay`):** Moving the video plane into a standalone Android `Dialog` window does not prevent the top web UI (`ContentViewRenderView`) from losing its `TRANSLUCENT` alpha association on pre-BLAST Android 11 upon `eglSwapBuffers` (`findings.md` Experiment 2, `findings.3.md`).

---

## 5. Remaining Viable Path: Joint `Session.relayout` on `ContentViewRenderView` via Format Re-assertion
Because Option A, Option B, and Option C have all been empirically tested and written off on pre-BLAST Android 11, the empirical data proves that **modifying `VideoSurfaceView` alone (whether via `removeView`/`addView` staging, size locking, or EGL clearing) cannot solve the `rel -1` vs `rel -2` layering or transparent hole-punching disconnect on Android 11.**

### Why Joint Relayout is Required
`SurfaceFlinger` on pre-BLAST Android 11 (`API 30`) only binds the `TRANSLUCENT` punch-out alpha channel when `ContentViewRenderView` (`Child 1`) and `VideoSurfaceView` (`Child 0`) establish or refresh their native window transactions together. When `VideoSurfaceView` alone undergoes an independent `Session.relayout` (`createNewSurfaceView()` or `setLayoutParams()`), `ContentViewRenderView` does not participate in that kernel transaction, causing its transparent hole to desynchronize.

### Next Investigation Phase
To force `WindowManagerService` and `SurfaceFlinger` on Android 11 to re-synchronize `ContentViewRenderView` right over the newly replaced/resized `VideoSurfaceView` without tearing down active `MediaCodec` buffers:
* We must investigate commanding a joint `Session.relayout` on `ContentViewRenderView.mSurfaceView` (e.g., via format re-assertion: `getSurfaceView().getHolder().setFormat(PixelFormat.OPAQUE)` followed immediately by `setFormat(PixelFormat.TRANSLUCENT)` on the UI thread when `VideoSurfaceView.surfaceChanged(...)` or `createNewSurfaceView()` completes).

----

Here is the exact side-by-side comparison of how the web UI surface is initialized in C25 versus C27:
  ──────
  ### 1. In C25: Root Activity Window (takeSurface)

  In C25 (~/cobalt25/src), inside CobaltActivity.java:

      @Override
      protected void onCreateSurfaceView() {
        mSurfaceView = null;

        // C25: Skia/Starboard takes ownership of the Activity's root ANativeWindow (Z = 0)!
        getWindow().takeSurface(this);

        mContentView = new View(this);
        setContentView(mContentView);
        mContentView.requestFocus();
      }

  • What C25 did: getWindow().takeSurface(this) handed the root Activity's physical window handle (Z = 0) directly to Skia/Starboard. There was
  no child SurfaceView for the web UI.
  • Why it never desynchronized: Because takeSurface(this) operates on the root window that contains all other views, when VideoSurfaceView
  (Child 0) resized or underwent relayout, the parent root window (Z = 0) stayed fixed at the top of the OS hierarchy without needing a joint
  relayout.
  ──────
  ### 2. In C27: Chromium Sibling SurfaceView (ContentViewRenderView)

  In C27 (~/cobalt.main.2/src), takeSurface(this) was removed during the Chromium m114/m138 rebase. Instead, the web UI is rendered into a
  child SurfaceView (ContentViewRenderView) created and managed across CobaltActivity.java, ShellManager.java, and Shell.java:

  Step A: In CobaltActivity.java:

        mShellManager = new ShellManager(this);
        mWindowAndroid = new ActivityWindowAndroid(this, true, mIntentRequestTracker, null, false);
        mShellManager.setWindow(mWindowAndroid);

        // C27: Retrieves the SurfaceView from ContentViewRenderView rather than using takeSurface!
        mWindowAndroid.setAnimationPlaceholderView(
            mShellManager.getContentViewRenderView().getSurfaceView());

  Step B: In ShellManager.java:

        public void setWindow(WindowAndroid window) {
            assert window != null;
            mWindow = window;
            // C27: Instantiates ContentViewRenderView (which extends FrameLayout & owns the web SurfaceView)
            mContentViewRenderView = new ContentViewRenderView(getContext());
            mContentViewRenderView.onNativeLibraryLoaded(window);
        }

  Step C: In Shell.java:

        public void setContentViewRenderView(@Nullable ContentViewRenderView contentViewRenderView) {
            if (contentViewRenderView != null) {
                // C27: Attaches ContentViewRenderView as Child #1 in the FrameLayout alongside VideoSurfaceView (Child #0)!
                mRootView.addView(contentViewRenderView,
                        new FrameLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT));
            }
            mContentViewRenderView = contentViewRenderView;
        }

  ### The Architectural Shift

  • C25: Activity Root Window (takeSurface, Z = 0) → Sibling below it: VideoSurfaceView (Z = -2).
  • C27: Activity Root Window (Z = 0) → Siblings inside it: ContentViewRenderView (SurfaceView #1) and VideoSurfaceView (SurfaceView #0).

  Because C27 renders the web UI into a sibling SurfaceView (ContentViewRenderView), whenever VideoSurfaceView alone is replaced or resized,
  WindowManagerService on Android 11 only relays out VideoSurfaceView. A joint Session.relayout on ContentViewRenderView is therefore the exact
  mechanism required to keep the two sibling SurfaceViews synchronized on C27.

