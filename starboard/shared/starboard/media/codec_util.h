// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

// This function parses the video codec string and returns a codec.  All fields
// will be filled with information parsed from the codec string when possible,
// otherwise they will have the following default values:
//            profile: -1
//              level: -1
//          bit_depth: 8
//         primary_id: kSbMediaPrimaryIdUnspecified
//        transfer_id: kSbMediaTransferIdUnspecified
//          matrix_id: kSbMediaMatrixIdUnspecified
// It returns true when |codec| contains a well-formed codec string, otherwise
// it returns false.
bool ParseVideoCodec(const char* codec_string,
                     SbMediaVideoCodec* codec,
                     int* profile,
                     int* level,
                     int* bit_depth,
                     SbMediaPrimaryId* primary_id,
                     SbMediaTransferId* transfer_id,
                     SbMediaMatrixId* matrix_id);

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_CODEC_UTIL_H_
