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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_CODEC_UTIL_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_CODEC_UTIL_H_

#include "starboard/media.h"
#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

SbMediaAudioCodec GetAudioCodecFromString(const char* codec);
SbMediaVideoCodec GetVideoCodecFromString(const char* codec);

// This function parses an h264 codec in the form of {avc1|avc3}.PPCCLL as
// specificed by https://tools.ietf.org/html/rfc6381#section-3.3.
//
// Note that the leading codec is not necessarily to be "avc1" or "avc3" per
// spec but this function only parses "avc1" and "avc3".  This function returns
// false when |codec| doesn't contain a valid codec string.
bool ParseH264Info(const char* codec, int* width, int* height, int* fps);

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_CODEC_UTIL_H_
