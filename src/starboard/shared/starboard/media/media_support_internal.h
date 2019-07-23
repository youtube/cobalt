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

#include "starboard/media.h"
#include "starboard/shared/internal_only.h"

#ifdef __cplusplus
extern "C" {
#endif

// Indicates whether a given combination of (|frame_width| x |frame_height|)
// frames at |bitrate| and |fps| is supported on this platform with
// |video_codec|. If |video_codec| is not supported under any condition, this
// function returns |false|.
//
// |video_codec|: The video codec used in the media content.
// |profile|: The profile in the context of |video_codec|.  It should be set to
//            -1 when it is unknown or not applicable.
// |level|: The level in the context of |video_codec|.  It should be set to -1
//          when it is unknown or not applicable.
// |bit_depth|: The color bit depth of the video.  It should be set to 8 for SDR
//              videos, and set to 10 or 12 for HDR videos.
// |primary_id|: The colour primaries of the video.  See definition for
//               |SbMediaPrimaryId| in `media.h` for more details.  It should be
//               set to |kSbMediaPrimaryIdUnspecified| when it is unknown.
// |transfer_id|: The transfer characteristics of the video.  See definition
//                for |SbMediaTransferId| in `media.h` for more details.  It
//                should be set to |kSbMediaTransferIdUnspecified| when it is
//                unknown.
// |matrix_id|: The matrix coefficients of the video.  See definition for
//                |SbMediaMatrixId| in `media.h` for more details.  It should be
//                set to |kSbMediaMatrixIdUnspecified| when it is unknown.
// |frame_width|: The frame width of the media content.  When set to 0, it
//                indicates that the frame width shouldn't be considered.
// |frame_height|: The frame height of the media content.  When set to 0, it
//                indicates that the frame height shouldn't be considered.
// |bitrate|: The bitrate of the media content.  When set to 0, it indicates
//            that the bitrate shouldn't be considered.
// |fps|: The number of frames per second in the media content.  When set to 0,
//        it indicates that the fps shouldn't be considered.
// |decode_to_texture_required|: Whether or not the resulting video frames can
//                               be decoded and used as textures by the GPU.
SB_EXPORT bool SbMediaIsVideoSupported(SbMediaVideoCodec video_codec,
#if SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
                                       int profile,
                                       int level,
                                       int bit_depth,
                                       SbMediaPrimaryId primary_id,
                                       SbMediaTransferId transfer_id,
                                       SbMediaMatrixId matrix_id,
#endif  // SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
                                       int frame_width,
                                       int frame_height,
                                       int64_t bitrate,
                                       int fps
#if SB_API_VERSION >= 10
                                       ,
                                       bool decode_to_texture_required
#endif  // SB_API_VERSION >= 10
                                       );

// Indicates whether this platform supports |audio_codec| at |bitrate|.
// If |audio_codec| is not supported under any condition, this function
// returns |false|.
//
// |audio_codec|: The media's audio codec (|SbMediaAudioCodec|).
// |bitrate|: The media's bitrate.
SB_EXPORT bool SbMediaIsAudioSupported(SbMediaAudioCodec audio_codec,
                                       int64_t bitrate);

#if !SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
// Indicates whether this platform supports |transfer_id| as a transfer
// characteristics.  If |transfer_id| is not supported under any condition, this
// function returns |false|.
//
// |transfer_id|: The id of transfer charateristics listed in SbMediaTransferId.
SB_EXPORT bool SbMediaIsTransferCharacteristicsSupported(
    SbMediaTransferId transfer_id);
#endif  // !SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_SUPPORT_INTERNAL_H_
