
### Side-by-Side Surface Layout Comparison (dumpsys SurfaceFlinger)

             Z-Order Tier           | C25 Layout (~/cobalt25/src)      | C27 Layout (~/cobalt.main.2/src)  | Key Differentiating Impact
  ----------------------------------|----------------------------------|-----------------------------------|-----------------------------------
       Z = 0(Topmost UI Canvas)     | MainActivity#0• Format:          | MainActivity#0• Format: RGBA_8888 | Identical top-level transparent
                                    | RGBA_8888 (isOpaque=0)• Size:    | (isOpaque=0)• Size: 1920x1080•    | hole (1920x1080) on both
                                    | 1920x1080• Role: Root Activity   | Role: Root Activity window.       | versions.
                                    | window (takeSurface).            | Chromium's Viz compositor paints  |
                                    | Skia/Starboard UI paints         | the primary web UI here with the  |
                                    | directly onto this single root   | full 1920x1080 transparent hole   |
                                    | window with the full 1920x1080   | cut in it.                        |
                                    | transparent hole cut in it.      |                                   |
    Z = -1 / rel -1(Media Overlay   | 🚫 NONE (Zero layers exist at Z  | ⚠️ SurfaceView - MainActivity#1   | The exact cause of the bug in
		Tier)               | = -1)Because C25 has no child    | (ORPHANED)• Format: RGBA_8888     | C27: C27 has an RGBA_8888 web
                                    | web SurfaceView                  | (isOpaque=0)• Size: 1920x1080•    | SurfaceView trapped at rel -1
                                    | (ContentViewRenderView), this Z- | Role: Lingering detached child    | directly over the video, blocking
                                    | order tier is 100% empty.        | web SurfaceView                   | everything except its 720x404
                                    |                                  | (ContentViewRenderView) wedged    | thumbnail hole. C25 has no layer
                                    |                                  | above rel -2. Holds the old       | here at all.
                                    |                                  | frozen 720x404 thumbnail hole.    |
     Z = -2 / rel -2(Active Video   | SurfaceView - MainActivity#1     | SurfaceView - MainActivity#2      | Both versions have their active
		Tier)               | (ACTIVE)• Format: 0x000011 (YUV, | (ACTIVE)• Format: 0x000011 (YUV,  | 1080p hardware video decoding to
                                    | isOpaque=1)• Size: 1920x1080     | isOpaque=1)• Size: 1920x1080      | rel -2.
                                    | (queued-frames=2)• Role: Active  | (queued-frames=2)• Role: Active   |
                                    | hardware VideoSurfaceView        | hardware VideoSurfaceView playing |
                                    | playing 1080p video directly     | 1080p video beneath rel -1 and Z  |
                                    | beneath the Z = 0 UI canvas.     | = 0.                              |
    Z = -2 / rel -2(Inactive / Old  | SurfaceView - MainActivity#0     | SurfaceView - MainActivity#0      | Both versions retain old YUV
            Video Buffer)           | (INACTIVE)• Format: 0x000011     | (INACTIVE)• Format: 0x000011      | video buffers at Z = -2, which is
                                    | (YUV, isOpaque=1)• Size: 720x405 | (YUV, isOpaque=1)• Size: 720x404  | harmless because they sit
                                    | at pos=(156,243)• Role: Old      | at pos=(156,243)• Role: Old       | alongside or behind the active
                                    | detached VideoSurfaceView from   | detached VideoSurfaceView from    | video at rel -2.
                                    | before the transition. Sits      | before the transition. Sits at    |
                                    | safely at Z = -2 alongside the   | rel -2 / rel -3.                  |
                                    | active video.                    |                                   |
  ──────
  ### Why C25 Has No Problem while C27 Does:

  1. In C25 (takeSurface architecture):
  There is literally no layer between Z = 0 and Z = -2. When Skia/Starboard punches out the 1920x1080 transparent hole in MainActivity#0 (Z =
  0), light travels straight down to the 1080p hardware video plane (Z = -2) with zero obstructions.
  2. In C27 (ContentViewRenderView child SurfaceView architecture):
  Chromium renders the web UI into a child SurfaceView (ContentViewRenderView #1 at rel -1). During rapid relaunch view churn on Android 11,
  WindowManagerService leaves ContentViewRenderView #1 wedged at rel -1 holding its old 720x404 thumbnail hole. Even though Z = 0 punches a
  full 1920x1080 hole, looking through Z = 0 hits #1 (rel -1), which blocks the rest of the screen and only allows the 720x404 thumbnail
  opening to show through to #2 (rel -2).

