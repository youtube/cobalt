// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/shared/starboard/media/codec_util.h"

#include "starboard/character.h"
#include "starboard/log.h"
#include "starboard/string.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

namespace {

int hex_to_int(char hex) {
  if (hex >= '0' && hex <= '9') {
    return hex - '0';
  }
  if (hex >= 'A' && hex <= 'F') {
    return hex - 'A' + 10;
  }
  if (hex >= 'a' && hex <= 'f') {
    return hex - 'a' + 10;
  }
  SB_NOTREACHED();
  return 0;
}

}  // namespace

SbMediaAudioCodec GetAudioCodecFromString(const char* codec) {
  if (SbStringCompare(codec, "mp4a.40.", 8) == 0) {
    return kSbMediaAudioCodecAac;
  }
  if (SbStringCompare(codec, "opus", 4) == 0) {
    return kSbMediaAudioCodecOpus;
  }
  if (SbStringCompare(codec, "vorbis", 6) == 0) {
    return kSbMediaAudioCodecVorbis;
  }
  return kSbMediaAudioCodecNone;
}

SbMediaVideoCodec GetVideoCodecFromString(const char* codec) {
  if (SbStringCompare(codec, "avc1.", 5) == 0 ||
      SbStringCompare(codec, "avc3.", 5) == 0) {
    return kSbMediaVideoCodecH264;
  }
  if (SbStringCompare(codec, "hev1.", 5) == 0 ||
      SbStringCompare(codec, "hvc1.", 5) == 0) {
    return kSbMediaVideoCodecH265;
  }
  if (SbStringCompare(codec, "vp8", 3) == 0) {
    return kSbMediaVideoCodecVp8;
  }
  if (SbStringCompare(codec, "vp9", 3) == 0) {
    return kSbMediaVideoCodecVp9;
  }
  return kSbMediaVideoCodecNone;
}

bool ParseH264Info(const char* codec, int* width, int* height, int* fps) {
  SB_DCHECK(width);
  SB_DCHECK(height);
  SB_DCHECK(fps);

  if (GetVideoCodecFromString(codec) != kSbMediaVideoCodecH264) {
    return false;
  }

  if (SbStringGetLength(codec) != 11 || !SbCharacterIsHexDigit(codec[9]) ||
      !SbCharacterIsHexDigit(codec[10])) {
    return false;
  }

  // According to https://en.wikipedia.org/wiki/H.264/MPEG-4_AVC#Levels, the
  // only thing that determinates resolution and fps is level.  Profile and
  // constraint set only affect bitrate and features.
  //
  // Note that a level can map to more than one resolution/fps combinations.
  // So the returned resolution/fps is hand-picked to best fit for common cases.
  // For example, level 4.2 indicates 1280 x 720 at 145.1 fps or 1920 x 1080 at
  // 64.0 fps or 2048 x 1080 @ 60.9 fps.  In this case, the function returns
  // 1920 x 1080 at 60 fps.
  int level = hex_to_int(codec[9]) * 16 + hex_to_int(codec[10]);

  // Level 3.1 is the minimum to support 720p at 30 fps.  We assume all devices
  // can support it.
  if (level <= 31) {
    *width = 1280;
    *height = 720;
    *fps = 30;
    return true;
  } else if (level <= 32) {
    *width = 1280;
    *height = 720;
    *fps = 60;
    return true;
  } else if (level <= 40) {
    *width = 1920;
    *height = 1080;
    *fps = 30;
    return true;
  } else if (level <= 41) {
    *width = 1920;
    *height = 1080;
    *fps = 30;
    return true;
  } else if (level <= 42) {
    *width = 1920;
    *height = 1080;
    *fps = 60;
    return true;
  } else if (level <= 50) {
    *width = 2560;
    *height = 1440;
    *fps = 30;
    return true;
  } else if (level <= 51) {
    *width = 3840;
    *height = 2160;
    *fps = 30;
    return true;
  }

  // Level >= 52
  *width = 3840;
  *height = 2160;
  *fps = 60;
  return true;
}

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
