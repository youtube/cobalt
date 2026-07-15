
  ### Verification Verdict:

  • Cobalt 25 (Immune): Renders the Web UI directly onto the Root Activity Window (Z = 0) by taking ownership of the window surface using
  getWindow().takeSurface(this). It has no child SurfaceView at rel -1 for the UI. When the activity is destroyed, the OS robustly cleans up
  the root window and its surface, leaving no orphans.
  • Cobalt 27 (Vulnerable): Renders the UI on a sibling child SurfaceView (ContentViewRenderView) configured with setZOrderMediaOverlay(true),
  which forces it to rel -1. This introduces a separate surface lifecycle that leaks on Android 11 during rapid relaunch, creating the
  occluding orphan layer at Z = -1.

---

Making UI surface to transparent seems to be dead-end. making surface transparent async op. involving multiuple threads. Then we cannot
  guaratnee transparenty completed in time (onPause or onStop timeout).
