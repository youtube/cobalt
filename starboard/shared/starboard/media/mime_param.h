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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_MIME_PARAM_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_MIME_PARAM_H_

#include <cstddef>
#include <iosfwd>
#include <string_view>

#include "build/build_config.h"

namespace starboard {

// Represents a MIME type parameter name.
class MimeParam {
 public:
  template <size_t N>
  constexpr explicit MimeParam(const char (&name)[N]) : name_(name, N - 1) {}

  friend std::ostream& operator<<(std::ostream& os, const MimeParam& param);

  constexpr bool operator==(const MimeParam& other) const {
    return name_ == other.name_;
  }
  constexpr bool operator!=(const MimeParam& other) const {
    return name_ != other.name_;
  }

  bool EqualsCaseInsensitive(std::string_view other) const;

 private:
  std::string_view name_;
};

// MIME parameters recognized and used by Cobalt. Cobalt mainly uses these to
// receive additional information from the JavaScript layer.
//
// Historically, MIME parameters were used to run playback experiments. Modern
// player experiments should use H5VCC settings instead, as long as it is not
// per playback setting.

// General MIME parameter.
inline constexpr MimeParam kMimeParamCodecs{"codecs"};

// Format capability MIME parameters defined in the web player (capability.ts).

// keep-sorted start
inline constexpr MimeParam kMimeParamBitrate{"bitrate"};
inline constexpr MimeParam kMimeParamChannels{"channels"};
inline constexpr MimeParam kMimeParamCryptoblockformat{"cryptoblockformat"};
inline constexpr MimeParam kMimeParamDecodeToTexture{"decode-to-texture"};
inline constexpr MimeParam kMimeParamEotf{"eotf"};
inline constexpr MimeParam kMimeParamExperimental{"experimental"};
inline constexpr MimeParam kMimeParamFramerate{"framerate"};
inline constexpr MimeParam kMimeParamHeight{"height"};
inline constexpr MimeParam kMimeParamTunnelMode{"tunnelmode"};
inline constexpr MimeParam kMimeParamWidth{"width"};
// keep-sorted end

// Cobalt-specific MIME parameters that are not used by the web player.

// Attribute passed within the |key_system| string of
// SbMediaCanPlayMimeAndKeySystem() (starboard/media.h) to convey the EME
// encryption scheme (e.g., "cenc", "cbcs"). Used by NPLB tests
// (media_can_play_mime_and_key_system_test.cc) and platform ports since the C
// API does not take a separate encryption scheme argument.
inline constexpr MimeParam kMimeParamEncryptionScheme{"encryptionscheme"};

#if BUILDFLAG(IS_ANDROID)
// Playback experiment MIME parameters defined in the web player (manifest.ts).

// keep-sorted start
inline constexpr MimeParam kMimeParamEnableFlushDuringSeek{
    "enableflushduringseek"};
inline constexpr MimeParam kMimeParamEnableResetAudioDecoder{
    "enableresetaudiodecoder"};
// keep-sorted end

// Cobalt-specific MIME parameters that are not used by the web player.

// Decode-To-Texture(used by WebGL video shaders) may use this param.
// TODO: b/490474392 - Move this param accordingly, when it is actually used.
// Main tracking bug: b/490474392, DRM exploration: b/494037632
inline constexpr MimeParam kMimeParamSoftwareDecoder{"softwaredecoder"};
#endif

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MIME_PARAM_H_
