#ifndef STARBOARD_EXTENSION_PLAYER_SET_VIDEO_SURFACE_VIEW_H_
#define STARBOARD_EXTENSION_PLAYER_SET_VIDEO_SURFACE_VIEW_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kStarboardExtensionPlayerSetVideoSurfaceViewName \
  "dev.starboard.extension.PlayerSetVideoSurfaceView"

typedef struct StarboardExtensionPlayerSetVideoSurfaceViewApi {
  // Name should be the string
  // |kStarboardExtensionPlayerSetVideoSurfaceViewName|. This helps to validate
  // that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // This is used to set the SurfaceView for the subsequently created
  // SbPlayer on the current calling thread.
  void (*SetVideoSurfaceViewForCurrentThread)(void* surface_view);
} StarboardExtensionPlayerSetVideoSurfaceViewApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_FOO_H_
