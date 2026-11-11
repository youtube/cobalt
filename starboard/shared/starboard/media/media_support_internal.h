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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_SUPPORT_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_SUPPORT_INTERNAL_H_

#include <cstdint>

#include "starboard/common/media.h"
#include "starboard/common/size.h"
#include "starboard/configuration.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/mime_type.h"

namespace starboard {

// Indicates whether this platform supports decoding |video_codec| and
// |audio_codec| along with decrypting using |key_system|. If |video_codec| is
// |kSbMediaVideoCodecNone| or if |audio_codec| is |kSbMediaAudioCodecNone|,
// this function should return |true| as long as |key_system| is supported on
// the platform to decode any supported input formats.
//
// |video_codec|: The |SbMediaVideoCodec| being checked for platform
//                compatibility.
// |audio_codec|: The |SbMediaAudioCodec| being checked for platform
//                compatibility.
// |key_system|: The key system being checked for platform compatibility.
bool MediaIsKeySystemSupported(SbMediaVideoCodec video_codec,
                               SbMediaAudioCodec audio_codec,
                               const char* key_system);

// Describes the video stream checked by MediaIsVideoSupported().
struct VideoSupportQuery {
  // The video codec and the information parsed from the codec string.  The
  // video codec is |codec_info.codec|, which must not be
  // |kSbMediaVideoCodecNone|.  Other fields in |codec_info| keep their default
  // values when they are unknown or not applicable:
  // |profile|: The profile in the context of the video codec.  -1 when it is
  //            unknown or not applicable.
  // |level|: The level in the context of the video codec.  -1 when it is
  //          unknown or not applicable.
  // |bit_depth|: The color bit depth of the video.  It should be set to 8 for
  //              SDR videos, and set to 10 or 12 for HDR videos.
  // |primary_id|: The colour primaries of the video.  See definition for
  //               |SbMediaPrimaryId| in `media.h` for more details.
  //               |kSbMediaPrimaryIdUnspecified| when it is unknown.
  // |transfer_id|: The transfer characteristics of the video.  See definition
  //                for |SbMediaTransferId| in `media.h` for more details.
  //                |kSbMediaTransferIdUnspecified| when it is unknown.
  // |matrix_id|: The matrix coefficients of the video.  See definition for
  //              |SbMediaMatrixId| in `media.h` for more details.
  //              |kSbMediaMatrixIdUnspecified| when it is unknown.
  ParsedVideoCodec codec_info;
  // The frame size of the media content.  When its width or height is set to
  // 0, it indicates that the frame width or height shouldn't be considered.
  Size frame_size;
  // The bitrate of the media content.  When set to 0, it indicates that the
  // bitrate shouldn't be considered.
  int64_t bitrate = 0;
  // The number of frames per second in the media content.  When set to 0, it
  // indicates that the fps shouldn't be considered.
  int fps = 0;
  // Whether or not the resulting video frames can be decoded and used as
  // textures by the GPU.
  bool decode_to_texture_required = false;
};

// Indicates whether the video stream described by |query| is supported on this
// platform.  If |query.codec_info.codec| is not supported under any condition,
// this function returns |false|.
//
// |mime_type|: The parsed mime type passed to the corresponding interface.
//              Note that |mime_type| can be NULL.
// |query|: The video stream to check.  See |VideoSupportQuery| for details.
bool MediaIsVideoSupported(const MimeType* mime_type,
                           const VideoSupportQuery& query);

// Indicates whether this platform supports |audio_codec| at |bitrate|.
// If |audio_codec| is not supported under any condition, this function
// returns |false|.
//
// |audio_codec|: The media's audio codec (|SbMediaAudioCodec|).
// |mime_type|: The parsed mime type passed to the corresponding interface.
//              Note that |mime_type| can be NULL.
// |bitrate|: The media's bitrate.
bool MediaIsAudioSupported(SbMediaAudioCodec audio_codec,
                           const MimeType* mime_type,
                           int64_t bitrate);

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_SUPPORT_INTERNAL_H_
