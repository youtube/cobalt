# Cobalt Android Video Surface & Lifecycle Findings

This document summarizes the findings regarding how Cobalt on Android manages `VideoSurfaceView`, `SurfaceHolder.Callback` lifecycle events (`surfaceChanged`), player bounds updates (`setVideoSurfaceBounds`), and video decoder (`MediaCodec`) behavior.

---

## 1. `VideoSurfaceView.surfaceChanged()` & The Warning Message

In [`VideoSurfaceView.java`](file:///usr/local/google/home/kjyoun/cobalt.main.2/src/cobalt/android/apk/app/src/main/java/dev/cobalt/media/VideoSurfaceView.java#L81-L89), the surface change callback is implemented as follows:

```java
@Override
public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
  Log.i(TAG, "Video surface changed: format=" + format + ", size=" + width + "x" + height);
  // We should only ever see the initial change after creation.
  if (mSawInitialChange) {
    Log.e(TAG, "Video surface changed; decoding may break");
  }
  mSawInitialChange = true;
}
```

### Why / When does `surfaceChanged` trigger a second time?
1. **First Call (Normal Initialization):** When `VideoSurfaceView` is created, Android calls `surfaceCreated(...)` followed immediately by the initial `surfaceChanged(...)`. On this first call, `mSawInitialChange` is `false`, so the warning is **not** logged, and `mSawInitialChange` is set to `true`.
2. **Subsequent Calls (Warning Triggers):** If `surfaceChanged(...)` is invoked again during the lifetime of the same `SurfaceHolderCallback` / `VideoSurfaceView` instance, `mSawInitialChange` is `true`, triggering the `Log.e` error warning.

This happens whenever the structural properties of the `Surface` change while the view remains alive:
- **Resizing / Layout Changes:** The video player view's width or height changes dynamically via `setLayoutParams()`.
- **Format Changes:** The underlying pixel format of the surface buffer changes.
- **Window/Screen Mode Transitions:** Device rotation, foldable screen unfolding, or entering/exiting split-screen mode where the surface buffer geometry changes without `surfaceDestroyed()` occurring first.

---

## 2. Does Cobalt Destroy and Recreate the Surface on Bounds Changes?

**No.** Cobalt is specifically designed **not** to destroy and recreate the `SurfaceView` when player bounds change during playback.

### Why Cobalt updates in-place instead of recreating:
As documented by the `TODO` comment in [`VideoSurfaceView.java:initialize()`](file:///usr/local/google/home/kjyoun/cobalt.main.2/src/cobalt/android/apk/app/src/main/java/dev/cobalt/media/VideoSurfaceView.java#L66-L69):
```java
// TODO: Avoid recreating the surface when the player bounds change.
// Recreating the surface is time-consuming and complicates synchronizing
// punch-out video when the position / size is animated.
```
Recreating the surface (`surfaceDestroyed()` + `surfaceCreated()`) is expensive and can cause visible black flashes or desynchronization when punch-out video bounds are animated.

### When *does* Cobalt destroy and recreate the surface?
Cobalt explicitly destroys and creates a new `VideoSurfaceView` (via [`CobaltActivity.createNewSurfaceView()`](file:///usr/local/google/home/kjyoun/cobalt.main.2/src/cobalt/android/apk/app/src/main/java/dev/cobalt/coat/CobaltActivity.java#L694-L714)) only during:
- **Activity Stop/Backgrounding:** In `onStop()`, `mForceCreateNewVideoSurfaceView = true;` is set.
- **Explicit Native Cleanup:** When the Starboard player calls `VideoSurfaceHolder::CleanUpVideoWindow()` in C++ -> `ResetVideoSurface(env)` -> [`CobaltActivity.resetVideoSurface()`](file:///usr/local/google/home/kjyoun/cobalt.main.2/src/cobalt/android/apk/app/src/main/java/dev/cobalt/coat/CobaltActivity.java#L646-L657).

---

## 3. What Happens When `surfaceChanged()` Occurs Mid-Playback?

When `surfaceChanged(holder, format, width, height)` happens while a hardware `MediaCodec` decoder is actively decoding frames to the surface:

### At the Cobalt / Starboard Layer:
- `VideoSurfaceView.surfaceChanged()` does **not** notify C++ / Starboard (`VideoSurfaceViewJni.get().onVideoSurfaceChanged(...)` is only called in `surfaceCreated` and `surfaceDestroyed`).
- Starboard (`SbPlayer` / `MediaCodecBridge`) remains unaware of the surface geometry change and continues dequeueing/queueing frames to the existing `Surface` (`sCurrentSurface`).

### At the Android OS / `MediaCodec` Layer:
Because the underlying `BufferQueue` geometry changed underneath an actively running hardware decoder:
- **Scenario A (Layout/Bounds change only - Best Case):** If only the view width/height changed but the pixel format remained compatible, `MediaCodec` continues rendering frames at the intrinsic video resolution (e.g., 1080p or 4K). SurfaceFlinger / Android's compositor automatically scales and composites those frames to fit the new `width x height` of the `SurfaceView`. Playback continues smoothly.
- **Scenario B (BufferQueue Conflict / Driver Error - Why the warning exists):** On certain Android OEM hardware decoders (`OMX.*` / `c2.*`), or if the pixel format/stride changed, altering `BufferQueue` geometry mid-stream invalidates `MediaCodec`'s producer connection. When `releaseOutputBuffer(..., render=true)` is called next, the driver throws an `IllegalStateException`, returns `BUFFER_FLAG_ERROR`, or crashes natively. This causes video freezing or green/pink visual corruption.

---

## 4. How Player Bounds Are Updated (`setVideoSurfaceBounds` Call Chain)

Whenever the web page's `<video>` element changes its CSS layout position or size on the screen, the following chain executes:

1. **Web / DOM Layer (`<video>` element layout):**
   The Blink/web engine calculates the layout box of the HTML5 `<video>` tag on the page and notifies the media renderer of the geometry.
2. **Chromium Media Layer (`StarboardRenderer`):**
   In [`starboard_renderer.cc:OnVideoGeometryChange()`](file:///usr/local/google/home/kjyoun/cobalt.main.2/src/media/starboard/starboard_renderer.cc#L504-L520), if `output_rect_` changed, it calls `ApplyPendingBounds()` -> `player_bridge_->SetBounds(*output_rect_)`.
3. **Starboard Bridge (`SbPlayerBridge` & `SbPlayerSetBounds`):**
   Forwards the rectangle to Starboard C++ via [`SbPlayerSetBounds(player, z_index, x, y, width, height)`](file:///usr/local/google/home/kjyoun/cobalt.main.2/src/starboard/android/shared/player_set_bounds.cc#L27-L42). Inside `SbPlayerSetBounds`, it calls `StarboardBridge::GetInstance()->SetVideoSurfaceBounds(env, x, y, width, height);`.
4. **Android UI Layer (`CobaltActivity.java`):**
   The JNI method [`CobaltActivity.setVideoSurfaceBounds(x, y, width, height)`](file:///usr/local/google/home/kjyoun/cobalt.main.2/src/cobalt/android/apk/app/src/main/java/dev/cobalt/coat/CobaltActivity.java#L659-L692) posts a `Runnable` to the UI thread, updating `mVideoSurfaceView.setLayoutParams(layoutParams)`.
   - **If `width` or `height` changed:** Android triggers `requestLayout()` -> `onLayout()`, reconfigures the `BufferQueue` size, and fires `surfaceChanged(...)`.
   - **If only `x` or `y` (position/margins) changed:** Android updates the on-screen position (`SurfaceControl.setPosition()`). Because buffer dimensions did not change, `surfaceChanged()` is typically not called.

---

## 5. YouTube / YouTube TV App Behavior on Cobalt

1. **Static Fullscreen / Fixed-Bounds Punch-Out:**
   On Cobalt (Living Room client for `youtube.com/tv` / YouTube TV), when a video plays via punch-out (`kSbPlayerOutputModePunchOut`), `SbPlayerCreate` and `SbPlayerSetBounds` configure the punch-out rectangle to match the full viewport/screen dimensions (or fixed miniplayer bounds) and keep it static.
2. **No Continuous Layout Animation:**
   Unlike mobile/tablet apps where dragging a picture-in-picture player continuously animates and resizes the `SurfaceView` on every frame, the YouTube Living Room app on Cobalt does not smoothly animate or scale punch-out video bounds mid-stream across frames.
3. **No Harm on Cobalt:**
   Because YouTube / YouTube TV on Cobalt does not continuously resize punch-out video bounds during active playback:
   - `surfaceChanged()` normally triggers only **once** upon surface creation (`mSawInitialChange == false`).
   - If `surfaceChanged()` is ever called again (e.g., due to an OS-level layout pass or static dimension update), SurfaceFlinger handles static buffer resizing without interrupting `MediaCodec`.
   - Adding logging (`Log.i`) is purely informational and harmless. The warning (`Log.e("Video surface changed; decoding may break")`) serves purely as a cautionary check for edge-case hardware decoders that struggle with mid-stream resizing, and causes no disruption to YouTube / YouTube TV on Cobalt.

---

## 6. Two-Layer Punch-Out Stacking & Synchronization Architecture (`kSbPlayerOutputModePunchOut`)

In punch-out video output mode (`kSbPlayerOutputModePunchOut`), video frames completely bypass Chromium/Viz's OpenGL/Skia graphics compositor. Rendering and visibility are split across two synchronized layers:

### 1. Top Layer (Web UI Canvas & Transparent Hole)
* **Quad Creation:** When Blink lays out the `<video>` element, Chromium's media pipeline (`VideoResourceUpdater`) creates a [`VideoHoleDrawQuad`](file:///usr/local/google/home/kjyoun/cobalt.main.2/src/media/renderers/video_resource_updater.cc#L576) (`DrawQuad::Material::kVideoHole`) corresponding to the `<video>` element's DOM bounding rectangle.
* **Hole Punching:** During the frame compositing pass, Viz's underlay/overlay strategy in [`OverlayStrategyUnderlayStarboard::CommitCandidate()`](file:///usr/local/google/home/kjyoun/cobalt.main.2/src/components/viz/service/display/starboard/overlay_strategy_underlay_starboard.cc#L165-L182) replaces the `VideoHoleDrawQuad` with a solid transparent rectangle (`alpha = 0`, using `SkBlendMode::kSrcOver` or `SkBlendMode::kDstOut`) directly on the top Web UI canvas (`dev.cobalt.coat/dev.cobalt.app.MainActivity#0` at `Z = 0`). This ensures all web buttons, text, and backgrounds paint normally while leaving a 100% transparent opening where the video tag sits.

### 2. Bottom Layer (`VideoSurfaceView` Geometry Alignment)
* **Synchronization Chain:** Immediately upon committing the underlay candidate, `CommitCandidate()` invokes `SetVideoGeometry(display_rect, ...)` -> [`VideoGeometrySetterService::SetVideoGeometry()`](file:///usr/local/google/home/kjyoun/cobalt.main.2/src/cobalt/media/service/video_geometry_setter_service.cc#L76-L87) -> [`StarboardRenderer::OnVideoGeometryChange(output_rect)`](file:///usr/local/google/home/kjyoun/cobalt.main.2/src/media/starboard/starboard_renderer.cc#L504-L520) -> `player_bridge_->SetBounds()` -> [`SbPlayerSetBounds(z, x, y, width, height)`](file:///usr/local/google/home/kjyoun/cobalt.main.2/src/starboard/android/shared/player_set_bounds.cc#L27-L42) -> JNI [`CobaltActivity.setVideoSurfaceBounds(pos=(x,y), size=widthxheight)`](file:///usr/local/google/home/kjyoun/cobalt.main.2/src/cobalt/android/apk/app/src/main/java/dev/cobalt/coat/CobaltActivity.java#L659-L692).
* **View Positioning:** On the Android UI thread (`Runnable`), Java sets `layoutParams.width = width; layoutParams.height = height;` and updates the view margins on `mVideoSurfaceView`. This moves and resizes the bottom hardware video surface (`SurfaceView`) so it sits directly underneath the transparent hole cut on the top Web UI canvas.

### 3. Compositor 60fps Ticking vs. Deduplication
* **On-Demand Damage Scheduler:** When the Web UI has dirty pixels (`SurfaceDamageRect` from active animations, layout transitions, or fading controls), Viz wakes up and composites `AggregatedRenderPass` frames at 60fps (`~16.6ms` intervals), triggering `CommitCandidate()` and `SetVideoGeometry()` on every frame. Once UI animations settle, `damage_rect` becomes empty and the compositor goes completely idle (`0 FPS`) to conserve GPU/CPU power.
* **Deduplication:** Even when `CommitCandidate()` ticks 60 times per second during UI animations, `StarboardRenderer::OnVideoGeometryChange(const gfx::Rect& output_rect)` checks `if (output_rect_ == output_rect) return;`. This deduplication drops unchanged geometry calls, preventing 60fps JNI spam and keeping the bottom `VideoSurfaceView` completely stable mid-stream.

---

## 7. SurfaceFlinger Analysis: Orphaned UI `SurfaceView` Layers at `Z = -1` Blocking Full-Screen Video

### 1. The Symptom (`repro_thumbnail_playback.sh`)
During rapid lifecycle transitions—such as executing `KEYCODE_HOME` followed immediately (`sleep 0.05`) by `am start` to resume the app, waiting for thumbnail playback (`720x404` at `156,243`), and then pressing `DPAD_CENTER` (`16:49:41.149`) to launch full-screen video—the user observes that **full-screen 1080p video playback is only visible through the tiny `(156,243) 720x404` thumbnail opening**, while the rest of the screen (`1920x1080`) is blocked/covered.

### 2. Ruling Out Possibility B (Viz / Blink Hole Punching Failure)
End-to-end logging proves that when `DPAD_CENTER` is pressed, Chromium/Viz correctly cuts the full-screen transparent hole on the primary Web UI canvas (`Z = 0`) and aligns the bottom video surface:
```text
16:49:41.957 ... KJ: VideoResourceUpdater -> creating VideoHoleDrawQuad (PunchOut hole) for rect=0,0 1920x1080, visible_rect=0,0 1920x1080
16:49:41.959 ... KJ: OverlayStrategyUnderlayStarboard::CommitCandidate -> setting video geometry rect=0,0 1920x1080 and replacing VideoHoleDrawQuad with transparent solid color (cutting top UI hole)
16:49:41.961 ... KJ: SbPlayerSetBounds(z=1, rect=0,0 1920x1080) -> calling StarboardBridge::SetVideoSurfaceBounds()
16:49:41.968 ... KJ: setVideoSurfaceBounds Runnable -> id=7815e85, new_pos=(0,0), new_size=1920x1080
```
Thus, the primary Web UI canvas (`MainActivity#0` at `Z = 0`) is 100% transparent across `0,0 1920x1080`, and the active hardware video surface (`id=7815e85`) is correctly positioned at `pos=(0,0), size=1920x1080`.

### 3. The Root Cause (Possibility A): Asynchronous `SurfaceControl` Buffer Destruction
In Android, calling `ViewGroup.removeView(oldVideoSurfaceView)` inside [`CobaltActivity.createNewSurfaceView()`](file:///usr/local/google/home/kjyoun/cobalt.main.2/src/cobalt/android/apk/app/src/main/java/dev/cobalt/coat/CobaltActivity.java#L701-L715) detaches the Java `View` immediately from the UI tree. **However, `WindowManagerService` and `SurfaceFlinger` tear down the underlying native `SurfaceControl` (`BufferQueueLayer`) asynchronously over IPC.**

When the reproduction script forces rapid `HOME` -> `Resume` transitions and `createNewSurfaceView()` churns through multiple `SurfaceView` instances in quick succession, older detached `SurfaceView` buffers are unlinked from the Java view hierarchy but remain temporarily alive inside `SurfaceFlinger`.

### 4. Live `dumpsys SurfaceFlinger` Layer Stack Proof
Inspecting `adb shell dumpsys SurfaceFlinger` on the live device during this symptom reveals the exact stacking order from Top (`Z = 0`) to Bottom (`Z = -3`):

| Layer Stack | Layer Name | Relative Z | Pixel Format | Active Buffer / State | Role |
| :--- | :--- | :---: | :---: | :---: | :--- |
| **Top** | `dev.cobalt.coat/...MainActivity#0` | `Z = 0` | `RGBA_8888` | `1920x1080, RGBA_8888` | **Main Web UI Canvas** (Full-screen 1080p transparent hole cut by Viz). |
| **Middle (Orphaned)** | `SurfaceView - ...MainActivity#1`<br>`SurfaceView - ...MainActivity#3` | `rel -1`<br>`rel -1` | `RGBA_8888`<br>`RGBA_8888` | `1920x1080, RGBA_8888`<br>`1920x1080, RGBA_8888` | **⚠️ CULPRITS (Orphaned `SurfaceView` layers):** Leftover `SurfaceView` layers whose Java views were detached by `removeView()`, but remain pending asynchronous teardown in SurfaceFlinger. |
| **Active Video** | `SurfaceView - ...MainActivity#4` | `rel -2` | `0x000011 (YUV)` | `1920x1080, format 0x11`<br>`queued-frames=3` | **🎬 Active Hardware Video Surface (`id=7815e85`):** `MediaCodec` is actively decoding and rendering full-screen 1080p frames (`queued-frames=3`). |
| **Bottom** | `SurfaceView - ...MainActivity#0` | `rel -3` | `0x000011 (YUV)` | `pos=(156,243), size=(854,480)` | Oldest initial thumbnail view buffer at bottom. |

### 5. Summary of Mechanism
Because `SurfaceView #1` and `#3` (`rel -1`) have `RGBA_8888` pixel format and sit stacked physically **above** the active 1080p video surface (`SurfaceView #4` at `rel -2`), any opaque/black `RGBA_8888` pixels on `#1` and `#3` cover up the YUV video buffers on `#4`. 

Since those orphaned `Z = -1` layers still retain the old `720x404` thumbnail opening cut out of their opaque background from earlier in the session, looking through the top window (`Z = 0`) hits the leftover orphaned `SurfaceView` layers (`Z = -1`), which only let light through their `720x404` thumbnail opening down to the active 1080p video playing at `Z = -2`.

---

  ### Why Z = 0 Didn't Show the Full-Screen Hole: The Idle Timing Gap

  1. Independent Layers: MainActivity#0 (Z = 0) is managed by Chromium's GPU/Viz thread (SkiaRenderer), while SurfaceView #1 / #3 (Z = -1) are
  managed by Android's WindowManager. The existence of an orphan at Z = -1 never prevents Viz from drawing onto Z = 0.
  2. The Real Culprit for Z = 0 (0 FPS Idle Latency):
  When you clicked DPAD_CENTER (16:49:41.959), Viz composited the 1920x1080 transparent hole in its internal memory.
  However, if the HTML/CSS player didn't trigger any subsequent animations right after that exact frame, Viz immediately went idle (0 FPS)
  before that new 1920x1080 transparent buffer latched onto SurfaceFlinger's screen display!

  ### Summary of the Two Distinct Issues:

  • Issue 1 (Z = -1 Orphan): Caused by asynchronous native SurfaceControl destruction during rapid HOME → Resume / createNewSurfaceView()
  transitions.
  • Issue 2 (Z = 0 Stale Hole): Caused by the Viz compositor going idle (0 FPS) before its newly rendered 1080p transparent frame can be
  latched and displayed by SurfaceFlinger.

---

  1. The Visual Evidence on the Monitor:
  When you press DPAD_CENTER after HOME && am start, the physical TV screen visually shows that the UI layer (Z = 0) only has the thumbnail
  hole punching (720x404) and the Home Screen around it.
  2. The Log Evidence:
  Right when DPAD_CENTER is pressed (17:51:19.964), the log shows that CommitCandidate processed a full-screen hole (Quad[9] rect=0,0
  1920x1080), SkiaRenderer::SwapBuffers(1920x1080) ran, and DidReceiveSwapBuffersAck returned swap result=0. And after .964, zero further
  compositor swaps (DrawAndSwap) occurred in the log.
  3. The Lifecycle Condition:
  This contradiction between what the log attempted (1920x1080 hole swap) and what the UI layer visually shows (720x404 thumbnail hole) only
  happens after rapid relaunch (HOME && am start) leaves an orphaned VideoSurfaceView (SurfaceView #1/#3 at rel -1) attached in SurfaceFlinger.

---

Why does the presence of an orphaned VideoSurfaceView (rel -1) cause the UI layer (Z = 0) to visually only have the 720x404 thumbnail hole
  punched in it on the monitor,
---

  ──────
  ### 1. Topmost Layer: Main Web UI Canvas (Z = 0)

  • Layer Name: BufferQueueLayer (dev.cobalt.coat/dev.cobalt.app.MainActivity#0)
  • Z-Order: z = 0 (layerStack = 0, zOrderRelativeOf = none)
  • Position & Size: pos = (0, 0), size = (1920, 1080), crop = [0, 0, 1920, 1080]
  • Pixel Format & Opaque Flag: defaultPixelFormat = RGBA_8888, isOpaque = 0 (Partially Transparent)
  • Hole Size (Region Data):
      • TransparentRegion (count = 1): [0, 0, 1920, 1080] (Note: SurfaceFlinger reports the buffer's full transparent region bounding box or
      Skia's full-screen clear region).
      • Visual Appearance on Monitor: Visually shows the Home Screen (Recommended shelf and Search bar) with only a 720x404 thumbnail hole cut
      around the left card (pos: ~156,243).
  • Orphan Status: ACTIVE (Not an orphan). Attached to parent ActivityRecord (dev.cobalt.coat/dev.cobalt.app.MainActivity#0).
  ──────
  ### 2. Middle Layer: Leftover / Detached SurfaceView (rel -1)

  • Layer Name: BufferQueueLayer (SurfaceView - dev.cobalt.coat/dev.cobalt.app.MainActivity#1)
  • Z-Order: z = -1 (zOrderRelativeOf = dev.cobalt.coat/dev.cobalt.app.MainActivity#0)
  • Position & Size: pos = (0, 0), size = (1920, 1080), crop = [0, 0, 1920, 1080]
  • Pixel Format & Opaque Flag: defaultPixelFormat = RGBA_8888, isOpaque = 0 (Partially Transparent)
  • Visible & Transparent Regions:
      • VisibleRegion (count = 1): [0, 0, 1920, 1080]
      • TransparentRegion (count = 0): None reported in SurfaceFlinger metadata (queued-frames = 0).
  • Orphan Status: ORPHANED (rel -1). Its Java VideoSurfaceView (#1) was detached (removeView()) when createNewSurfaceView() replaced it, but
  this native BufferQueueLayer remains unreleased and stacked directly above rel -2.
  ──────
  ### 3. Bottom Layer: Active Hardware Video Surface (rel -2)

  • Layer Name: BufferQueueLayer (SurfaceView - dev.cobalt.coat/dev.cobalt.app.MainActivity#2)
  • Z-Order: z = -2 (zOrderRelativeOf = dev.cobalt.coat/dev.cobalt.app.MainActivity#0)
  • Position & Size: pos = (0, 0), size = (1920, 1080), crop = [0, 0, 1920, 1080]
  • Pixel Format & Opaque Flag: defaultPixelFormat = Unknown 0x000011 (HAL_PIXEL_FORMAT_YCrCb_420_SP), isOpaque = 1 (100% Opaque YUV Video)
  • Visible Region: VisibleRegion (count = 1): [0, 0, 1920, 1080] (queued-frames = 3)
  • Hole Status: No Hole. Solid 1080p hardware video plane playing the active video across the entire screen bounding box (0, 0, 1920, 1080).
  • Orphan Status: ACTIVE (Not an orphan). This is the live mVideoSurfaceView receiving decoded frames (queued-frames = 3) from MediaCodec.
  ──────
  ### Summary Table of Live Surface Hierarchy

   Layer Name / Index         | Z-Order | Size & Position | Format / Opaque Status   | Visual / Hole Status                    | Orphan Status
  ----------------------------|---------|-----------------|--------------------------|-----------------------------------------|---------------
   MainActivity#0 (UI Canvas) | Z = 0   | (0,0) 1920x1080 | RGBA_8888, isOpaque=0    | Visually displays Home Screen with      | Active
                              |         |                 |                          | 720x404 thumbnail hole                  |
   SurfaceView #1             | rel -1  | (0,0) 1920x1080 | RGBA_8888, isOpaque=0    | Unreleased buffer sitting between UI    | Orphaned
                              |         |                 |                          | (Z=0) and Video (Z=-2)                  |
   SurfaceView #2             | rel -2  | (0,0) 1920x1080 | YCrCb_420_SP, isOpaque=1 | Solid 1080p video playing across entire | Active
                              |         |                 |                          | screen                                  |
---

  ### Chronological Timing in C25 (Strictly Synchronous & Sequential)

  In C25, the timeline is strictly ordered on the player thread before touching the Java UI tree:

    [Decoder Thread] 1. ReleaseVideoSurface()
                         └─> Locks mutex, sets g_video_surface_holder = NULL, stops using window.
    [Decoder Thread] 2. ClearVideoWindow(force_reset)
                         └─> Locks mutex (no contention).
                             ├─> If !force_reset: calls ClearNativeWindow() (paints black + eglSwapBuffers).
                             └─> If force_reset: calls JNI resetVideoSurface() -> posts Runnable to UI thread.
    [Java UI Thread] 3. createNewSurfaceView()
                         └─> Executes frameLayout.removeView(videoSurfaceView) & addView(newView).
                             └─> Triggers surfaceDestroyed() -> nativeOnVideoSurfaceChanged(NULL).
                                 └─> ANativeWindow_release(g_native_video_window) runs cleanly.

  Key C25 Timing Guarantee:
  ReleaseVideoSurface() (Step 1) ALWAYS completes before ClearVideoWindow() (Step 2), which ALWAYS completes before Java's
  createNewSurfaceView() (Step 3). Because the decoder completely dropped its hold on the surface before Java calls removeView(), the native
  window releases cleanly without SurfaceFlinger hanging onto the old BufferQueueLayer.
  ──────
  ### Chronological Timing in C27 (Asynchronous & Decoupled via Viz/Mojo)

  In C27 (Chromium m138), surface management is decoupled between the player thread, the Viz compositor/Mojo (VideoGeometrySetterService), and
  the Java UI thread:

    [Mojo/UI/Player] 1. CleanUpVideoWindow(force_clear=false)
                         └─> Immediately calls JNI resetVideoSurface() without waiting for MediaCodec
                             to release or teardown its native window handles!
    [Java UI Thread] 2. createNewSurfaceView()
                         └─> Executes frameLayout.removeView(mVideoSurfaceView) RIGHT HERE while native
                             C++ (g_native_video_window) and MediaCodec STILL hold active ANativeWindow handles!
    [Java UI Thread] 3. JNI_VideoSurfaceView_OnVideoSurfaceChanged() (and SurfaceDestroyNotifier)
                         └─> Runs asynchronously AFTER removeView() when Android fires surfaceDestroyed().
                             └─> ANativeWindow_release(g_native_video_window) finally runs right here (too late).

--

  ### Mystery 1: Why does the Top UI only punch the 720x404 thumbnail hole when the log proves it attempted to punch a 1920x1080 full-screen   
  hole?

  • The Contradiction: The log clearly proves that when DPAD_CENTER is pressed, Chromium's Viz compositor generates Quad[9] (kVideoHole) 
  rect=0,0 1920x1080, executes SkiaRenderer::SwapBuffers(1920x1080), and receives DidReceiveSwapBuffersAck -> swap result=0.
  • The Mystery: Why does the top UI layer (ContentViewRenderView / Z = 0) physically ignore or fail to display that 1920x1080 swapped buffer
  on the screen, leaving the stale 720x404 thumbnail hole frame frozen on the monitor?
  ──────
  ### Mystery 2: Why does WindowManagerService put the orphan video surface above the active video surface (rel -1 vs rel -2)?

  • The Contradiction: Both the old (#1) and new (#2) VideoSurfaceView instances are created with the exact same window attributes
  (TYPE_APPLICATION_MEDIA) and added to the exact same FrameLayout index (0).
  • The Mystery: Why does Android 11's WindowManagerService / SurfaceFlinger elevate or wedge the old detached video surface (#1) at rel -1,
  above the new active video surface (#2 at rel -2)?
  - Technically, orphan video surface should not be a problem, since it is transparent. Apparently it is. I can full screen playing on new vieo
  surface under orphan video surface.

--


  ### Status of Our Investigation (What We Know vs What We Don't)

  #### 1. What We Have Definitively Proven (The Facts & Mechanics):

  • The Contradiction is Real: The log proves Chromium/Viz commits Quad[9] (kVideoHole) rect=0,0 1920x1080, executes
  SkiaRenderer::SwapBuffers(1920x1080), and receives DidReceiveSwapBuffersAck -> swap result=0. Yet the physical screen proves
  ContentViewRenderView (Z = 0) remains frozen displaying only the 720x404 thumbnail hole surrounded by the Home Screen.
  • The Layer Churn & Orphan Identity: During rapid relaunch (HOME && am start), createNewSurfaceView() detaches VideoSurfaceView v1
  (SurfaceView #0) and creates VideoSurfaceView v2 (SurfaceView #3). When removeView() detaches #0, Android 11 (API 30) resets its pending
  SurfaceControl attributes to (RGBA_8888, isOpaque=0, Z = -1).
  • Why C25 Was Immune: C25 rendered directly to the root Activity window via takeSurface(this) (Z = 0). Because C25 had no child
  ContentViewRenderView (SurfaceView #1) inside FrameLayout, it never had sibling SurfaceView window churn or an RGBA_8888, Z = -1 orphan.
  ──────
  #### 2. What Still Remains Unsolved (The Open Mystery):

  Why does ContentViewRenderView (Z = 0 / rel -1) physically reject, drop, or fail to latch the 1920x1080 hole-punch frame on Android 11?

  Specifically, we still do not know why none of our three targeted interventions succeeded in getting the top UI to display the 1920x1080 hole
  punch:

  1. Z-Tier Isolation (setZOrderMediaOverlay(true)): Placing ContentViewRenderView explicitly on rel -1 and VideoSurfaceView on rel -2 did not
  unfreeze the 720x404 hole punch on the top UI.
  2. Surface Teardown (SKIPPING PreserveChildSurfaceControls()): Disabling Chromium's b/157439199 surface preservation in
  content_view_render_view.cc did not unfreeze the 720x404 hole punch on the top UI.
  3. OS Window Refresh (refreshWebSurfaceHole on surfaceChanged): Forcing ContentViewRenderView (setFormat(OPAQUE -> TRANSLUCENT)) to execute a
  joint window reallocation exactly right before SkiaRenderer::SwapBuffers(1920x1080) ran did not unfreeze the 720x404 hole punch on the top UI.

  So your check is spot on: while we have eliminated our false assumptions and identified every layer in dumpsys SurfaceFlinger, the root cause
  of why the web UI compositor (ContentViewRenderView on Android 11) gets permanently stuck on its 720x404 thumbnail hole frame upon resuming
  from HOME remains unsolved.

---

  If we map your terms to the Z-order tree (using Z = 0 for the root window, and relative negative Z-orders for children):

  ### 1. In C25:

  • UI Layer: Rendered directly onto the Root Activity Window (Z = 0).
  • Child Surfaces: There is no child SurfaceView at rel -1. The only child is the video surface at rel -2.
  ──────
  ### 2. In C27:

  • Root Window (Z = 0): Made completely transparent (it acts as a clear container).
  • UI Layer: Rendered on a child SurfaceView (ContentViewRenderView) placed at rel -1 (which corresponds to your "Z1" tier).
  • Video Layer: Rendered on a child SurfaceView (VideoSurfaceView) placed at rel -2 (the tier below the UI).

  This architectural shift is precisely what introduced the bug: by moving the Web UI from the root window (Z = 0) down into a child
  SurfaceView at rel -1 ("Z1"), C27 created a stacking hierarchy where an orphaned Web UI layer could get trapped at rel -1 and block the video
  underneath it.


