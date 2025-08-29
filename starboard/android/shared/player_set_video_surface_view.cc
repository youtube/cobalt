#include "starboard/android/shared/player_set_video_surface_view.h"

#include "starboard/android/shared/video_surface_view.h"
#include "starboard/extension/player_set_video_surface_view.h"

namespace starboard::android::shared {

namespace {

// Definitions of any functions included as components in the extension
// are added here.
void SetVideoSurfaceViewForCurrentThread(void* surface_view) {
  starboard::android::shared::SetVideoSurfaceViewForCurrentThread(surface_view);
}

const StarboardExtensionPlayerSetVideoSurfaceViewApi
    kPlayerSetVideoSurfaceView = {
        kStarboardExtensionPlayerSetVideoSurfaceViewName,
        1,  // API version that's implemented.
        &SetVideoSurfaceViewForCurrentThread};

}  // namespace

const void* GetPlayerSetVideoSurfaceViewApi() {
  return &kPlayerSetVideoSurfaceView;
}

}  // namespace starboard::android::shared
