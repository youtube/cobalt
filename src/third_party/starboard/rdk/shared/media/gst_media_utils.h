//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
#ifndef THIRD_PARTY_STARBOARD_RDK_SHARED_MEDIA_GST_MEDIA_UTILS_H_
#define THIRD_PARTY_STARBOARD_RDK_SHARED_MEDIA_GST_MEDIA_UTILS_H_

#include <string>
#include <vector>

#include "starboard/media.h"

typedef struct _GstCaps GstCaps;

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {
namespace media {

bool GstRegistryHasElementForMediaType(SbMediaVideoCodec codec);
bool GstRegistryHasElementForMediaType(SbMediaAudioCodec codec);
GstCaps* CodecToGstCaps(SbMediaAudioCodec codec, const SbMediaAudioStreamInfo* info = nullptr);
GstCaps* CodecToGstCaps(SbMediaVideoCodec codec);

}  // namespace media
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party

#endif  // THIRD_PARTY_STARBOARD_RDK_SHARED_MEDIA_GST_MEDIA_UTILS_H_
