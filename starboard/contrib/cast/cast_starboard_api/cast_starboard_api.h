// Copyright 2022 Google LLC. All rights reserved.

#ifndef STARBOARD_CAST_CAST_STARBOARD_API_CAST_STARBOARD_API_H_
#define STARBOARD_CAST_CAST_STARBOARD_API_CAST_STARBOARD_API_H_

#include <starboard/drm.h>
#include <starboard/egl.h>
#include <starboard/event.h>
#include <starboard/gles.h>
#include <starboard/media.h>
#include <starboard/player.h>
#include <starboard/window.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initializes the Starboard thread and event loop. After this function is
// called, the Starboard APIs included above are expected to be available.
//
// Optional command line arguments are passed through |argc| and |argv|.
// The |callback| is analogous to SbEventHandle and must receive SbEvents.
//
// Must be called prior to the other library functions. Not guaranteed to be
// thread-safe; other library functions should not be called until this returns.
SB_EXPORT int CastStarboardApiInitialize(int argc,
                                         char** argv,
                                         void (*callback)(const SbEvent*));

// Finalizes the library in the provided |context|.
//
// Must not be called prior to the other library functions. Not guaranteed to be
// thread-safe; this function should not be called until all other library
// functions have returned.
SB_EXPORT void CastStarboardApiFinalize();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_CAST_CAST_STARBOARD_API_CAST_STARBOARD_API_H_