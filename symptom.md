# Android 11 Rapid Relaunch / DPAD Navigation Video Occlusion (`symptom.md`)

### Target Environment
* **Platform:** Android 11 (`API 30`)

### Repro step

```bash
# Minimum script to repro the problem.

adb shell am start -S -n dev.cobalt.coat/dev.cobalt.app.MainActivity

sleep 20
# Displays Home screen (Home UI scene #1), playing thumbnail video

adb shell "input keyevent KEYCODE_HOME && sleep 0.05 && am start -n dev.cobalt.coat/dev.cobalt.app.MainActivity"

sleep 5
# Now app is on home screen, playing thumbnail video.
# Move list one video right (changed to Home UI scene #2)
adb shell input keyevent KEYCODE_DPAD_RIGHT

sleep 10
# play a video
adb shell input keyevent KEYCODE_DPAD_CENTER
```

Expected:
- Video should start in full screen.

Actual:
- Home UI scene #1 is displayed, instead of the full-screen video.
- But, through the thumbnail hole, a part of the full-screen video is visible (playing the video selected at the end of the script).
- The UI behaves exactly as if full-screen video is playing (DPAD left/right inputs highlight hidden thumbnail options, DPAD down brings up the player progress bar).

Note:
- This issue is reproducible on Android 11, but not on Android 14.
- This issue is reproducible on C27 (current main, based on Chromium m138) and C26 (based on Chromium m114), but not on C25 (based on the fork of old Chromium).

--------

### Surface Layout Layer Stack (Buggy State)

The following table lists the active layers associated with Cobalt, ordered from **frontmost (top)** to **backmost (bottom)** in Z-order.

| Z-Order / Stacking | Layer Name | Screen Rect (WxH @ X,Y) | Buffer Size | Format | Composition | Notes / Interpretation |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Z = 0** (Frontmost) | `dev.cobalt.coat/dev.cobalt.app.MainActivity#0` | 1920x1080 @ (0,0) | 1920x1080 | RGBA_8888 | DEVICE | **Main UI Window** (container for the root View hierarchy). |
| **rel -1** | `SurfaceView - dev.cobalt.coat/dev.cobalt.app.MainActivity#3` | 1920x1080 @ (0,0) | 1920x1080 | RGBA_8888 | DEVICE | **Orphaned Web UI Surface** (lingering from first launch; renders *Home UI scene #1* with the `720x404` hole, blocking the screen). |
| **rel -1** | `SurfaceView - dev.cobalt.coat/dev.cobalt.app.MainActivity#1` | 1920x1080 @ (0,0) | 1920x1080 | RGBA_8888 | DEVICE | **Active Web UI Surface** (has the full-screen `1920x1080` transparent hole). |
| **rel -2** | `SurfaceView - dev.cobalt.coat/dev.cobalt.app.MainActivity#2` | 1920x1080 @ (0,0) | 1920x1080 | YUV (NV21, fmt 17) | DEVICE | **Active Video Surface** (rendering the active fullscreen YUV video). |
| **rel -2** (Backmost) | `SurfaceView - dev.cobalt.coat/dev.cobalt.app.MainActivity#0` | 720x404 @ (156, 243) | 854x480 | YUV (NV21, fmt 17) | DEVICE | **Orphaned Video Surface** (lingering from the first launch; transparent container). |

---

### Visual Blending Diagrams (How the Occlusion Happens)

#### 1. Thumbnail Mode (The Bug is Invisible)
The active UI layer is stacked on top of the orphaned layer. Because it contains opaque Home Screen graphics everywhere outside the `720x404` thumbnail box, it completely covers and hides the orphaned layer underneath it.

```text
[ USER'S EYE ]
      │
      ▼
┌───────────────────────────────────────────┐
│ Active UI (Z = -1)                        │  <-- Opaque Home Screen (O) with 720p Hole (H)
│  [ O  O  O  O  O  O  O  O  O  O  O  O ]   │
│  [ O  O  O  H  H  H  H  O  O  O  O  O ]   │  
└─────────────┬───────────┬─────────────────┘
              │           │ (Blocked elsewhere by Opaque UI)
              ▼           x
      (Light passes)
┌───────────────────────────────────────────┐
│ Orphaned UI (Z = -1)                      │  <-- Hidden from user's view underneath
└───────────────────────────────────────────┘
              │
              ▼
┌───────────────────────────────────────────┐
│ Active Video (Z = -2)                     │  <-- Video is only visible through the hole
└───────────────────────────────────────────┘
```

#### 2. Fullscreen Mode (The Bug Explodes Into View)
The active UI transitions to fullscreen playback and sets its entire layout to transparent (`alpha = 0.0`). The active layer now acts as a clear window, exposing the orphaned layer directly underneath it. Since the orphaned layer still has the opaque *Home UI scene #1* painted on it, it masks the fullscreen video to only the `720x404` thumbnail box.

```text
[ USER'S EYE ]
      │
      ▼
┌───────────────────────────────────────────┐
│ Active UI (Z = -1)                        │  <-- 100% Transparent (T) (Fullscreen Hole)
│  [ T  T  T  T  T  T  T  T  T  T  T  T ]   │
└─────────────┬─────────────────────────────┘
              │ (Light passes through everywhere)
              ▼
┌───────────────────────────────────────────┐
│ Orphaned UI (Z = -1)                      │  <-- Renders Stale Home UI (O) with 720p Hole (H)
│  [ O  O  O  O  O  O  O  O  O  O  O  O ]   │  <-- REVEALED! Blocks video outside the hole.
│  [ O  O  O  H  H  H  H  O  O  O  O  O ]   │
└─────────────┬───────────┬─────────────────┘
              │           │ (Blocked by Orphan's Opaque UI)
              ▼           x
      (Light passes)
┌───────────────────────────────────────────┐
│ Active Video (Z = -2)                     │  <-- Fullscreen video plays underneath,
└───────────────────────────────────────────┘      but is blocked outside H.
```
Current hypothesis

- On Android 11, rapid relaunch (FG -> BG -> FG) leave orphared UI surface
- At first, this orphan UI surface is not visible since it is put behind the new UI surface
- But, on full screen playback. new UI surface becomes transparent. then orphaned UI surface occules full screen video playback on Video surface


