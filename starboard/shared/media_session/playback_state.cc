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

#include "starboard/shared/media_session/playback_state.h"

#include <cstring>

#include "cobalt/extension/media_session.h"
#include "starboard/common/log.h"
#include "starboard/string.h"
#include "starboard/system.h"

namespace starboard {
namespace shared {
namespace media_session {
namespace {

CobaltExtensionMediaSessionPlaybackState
PlaybackStateToMediaSessionPlaybackState(PlaybackState state) {
  CobaltExtensionMediaSessionPlaybackState result;
  switch (state) {
    case kPlaying:
      result = kCobaltExtensionMediaSessionPlaying;
      break;
    case kPaused:
      result = kCobaltExtensionMediaSessionPaused;
      break;
    case kNone:
      result = kCobaltExtensionMediaSessionNone;
      break;
    default:
      SB_NOTREACHED() << "Unsupported PlaybackState " << state;
      result = static_cast<CobaltExtensionMediaSessionPlaybackState>(-1);
  }
  return result;
}

const CobaltExtensionMediaSessionApi* g_extension;
}  // namespace

void UpdateActiveSessionPlatformPlaybackState(PlaybackState state) {
  if (g_extension == NULL) {
    g_extension = static_cast<const CobaltExtensionMediaSessionApi*>(
        SbSystemGetExtension(kCobaltExtensionMediaSessionName));
  }
  if (g_extension &&
      strcmp(g_extension->name, kCobaltExtensionMediaSessionName) ==
          0 &&
      g_extension->version >= 1) {
    CobaltExtensionMediaSessionPlaybackState ext_state =
        PlaybackStateToMediaSessionPlaybackState(state);
    g_extension->UpdateActiveSessionPlatformPlaybackState(ext_state);
  }
}
}  // namespace media_session
}  // namespace shared
}  // namespace starboard
