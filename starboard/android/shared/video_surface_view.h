#ifndef STARBOARD_ANDROID_SHARED_VIDEO_SURFACE_VIEW_H_
#define STARBOARD_ANDROID_SHARED_VIDEO_SURFACE_VIEW_H_

namespace starboard::android::shared {

// Get surface_view via SetVideoSurfaceViewForCurrentThread(),
// it returns 0 if s_thread_local_key is invalid.
void* GetSurfaceViewForCurrentThread();

// This is used to set the SurfaceView for the subsequently created
// SbPlayer on the current calling thread.
void SetVideoSurfaceViewForCurrentThread(void* surface_view);

}  // namespace starboard::android::shared

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_SURFACE_VIEW_H_
