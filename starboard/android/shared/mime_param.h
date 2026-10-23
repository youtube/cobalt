// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_MIME_PARAM_H_
#define STARBOARD_ANDROID_SHARED_MIME_PARAM_H_

#include "starboard/shared/starboard/media/mime_param.h"

namespace starboard {

// Format capability MIME parameter defined in the web player (capability.ts).
// Used on Android TV to enable tunneled playback (API level 34+).
inline constexpr MimeParam kMimeParamTunnelMode{"tunnelmode"};

// Playback experiment MIME parameters defined in the web player (manifest.ts).

// keep-sorted start
inline constexpr MimeParam kMimeParamEnableFlushDuringSeek{
    "enableflushduringseek"};
inline constexpr MimeParam kMimeParamEnableResetAudioDecoder{
    "enableresetaudiodecoder"};
// keep-sorted end

// Cobalt-specific MIME parameters that are not used by the web player.

// Decode-To-Texture (used by WebGL video shaders) may use this param.
// TODO: b/490474392 - Move this param accordingly, when it is actually used.
// Main tracking bug: b/490474392, DRM exploration: b/494037632
inline constexpr MimeParam kMimeParamSoftwareDecoder{"softwaredecoder"};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MIME_PARAM_H_
