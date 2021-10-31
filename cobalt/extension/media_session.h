// Copyright 2020 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_EXTENSION_MEDIA_SESSION_H_
#define COBALT_EXTENSION_MEDIA_SESSION_H_

#include "starboard/configuration.h"
#include "starboard/time.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kCobaltExtensionMediaSessionName "dev.cobalt.extension.MediaSession"

typedef enum CobaltExtensionMediaSessionPlaybackState {
  kCobaltExtensionMediaSessionNone = 0,
  kCobaltExtensionMediaSessionPaused = 1,
  kCobaltExtensionMediaSessionPlaying = 2
} CobaltExtensionMediaSessionPlaybackState;

typedef enum CobaltExtensionMediaSessionAction {
  kCobaltExtensionMediaSessionActionPlay,
  kCobaltExtensionMediaSessionActionPause,
  kCobaltExtensionMediaSessionActionSeekbackward,
  kCobaltExtensionMediaSessionActionSeekforward,
  kCobaltExtensionMediaSessionActionPrevioustrack,
  kCobaltExtensionMediaSessionActionNexttrack,
  kCobaltExtensionMediaSessionActionSkipad,
  kCobaltExtensionMediaSessionActionStop,
  kCobaltExtensionMediaSessionActionSeekto,

  // Not part of spec, but used in Cobalt implementation.
  kCobaltExtensionMediaSessionActionNumActions,
} CobaltExtensionMediaSessionAction;

typedef struct CobaltExtensionMediaImage {
  // These fields are null-terminated strings copied over from IDL.
  const char* size;
  const char* src;
  const char* type;
} CobaltExtensionMediaImage;

typedef struct CobaltExtensionMediaMetadata {
  // These fields are null-terminated strings copied over from IDL.
  const char* album;
  const char* artist;
  const char* title;

  CobaltExtensionMediaImage* artwork;
  size_t artwork_count;
} CobaltExtensionMediaMetadata;

typedef struct CobaltExtensionMediaSessionActionDetails {
  CobaltExtensionMediaSessionAction action;

  // Seek time/offset are non-negative. Negative value signifies "unset".
  double seek_offset;
  double seek_time;

  bool fast_seek;
} CobaltExtensionMediaSessionActionDetails;

typedef void (*CobaltExtensionMediaSessionUpdatePlatformPlaybackStateCallback)(
    CobaltExtensionMediaSessionPlaybackState state, void* callback_context);
typedef void (*CobaltExtensionMediaSessionInvokeActionCallback)(
    CobaltExtensionMediaSessionActionDetails details, void* callback_context);

// This struct and all its members should only be used for piping data to each
// platform's implementation of OnMediaSessionStateChanged and they are only
// valid within the scope of that function. Any data inside must be copied if it
// will be referenced later.
typedef struct CobaltExtensionMediaSessionState {
  SbTimeMonotonic duration;
  CobaltExtensionMediaSessionPlaybackState actual_playback_state;
  bool available_actions[kCobaltExtensionMediaSessionActionNumActions];
  CobaltExtensionMediaMetadata* metadata;
  double actual_playback_rate;
  SbTimeMonotonic current_playback_position;
  bool has_position_state;
} CobaltExtensionMediaSessionState;

typedef struct CobaltExtensionMediaSessionApi {
  // Name should be the string kCobaltExtensionMediaSessionName.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // MediaSession API Wrapper.

  void (*OnMediaSessionStateChanged)(
      CobaltExtensionMediaSessionState session_state);

  // Register MediaSessionClient callbacks when the platform create a new
  // MediaSessionClient.
  void (*RegisterMediaSessionCallbacks)(
      void* callback_context,
      CobaltExtensionMediaSessionInvokeActionCallback invoke_action_callback,
      CobaltExtensionMediaSessionUpdatePlatformPlaybackStateCallback
          update_platform_playback_state_callback);

  // Destroy platform's MediaSessionClient after the Cobalt's
  // MediaSessionClient has been destroyed.
  void (*DestroyMediaSessionClientCallback)();

  // Starboard method for updating playback state.
  void (*UpdateActiveSessionPlatformPlaybackState)(
      CobaltExtensionMediaSessionPlaybackState state);
} CobaltExtensionMediaSessionApi;

inline void CobaltExtensionMediaSessionActionDetailsInit(
    CobaltExtensionMediaSessionActionDetails* details,
    CobaltExtensionMediaSessionAction action) {
  details->action = action;
  details->seek_offset = -1.0;
  details->seek_time = -1.0;
  details->fast_seek = false;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_EXTENSION_MEDIA_SESSION_H_
