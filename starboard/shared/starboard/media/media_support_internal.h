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
// Setting any of the parameters to |0| indicates that they shouldn't be
// considered.
//
// |video_codec|: The video codec used in the media content.
// |frame_width|: The frame width of the media content.
// |frame_height|: The frame height of the media content.
// |bitrate|: The bitrate of the media content.
// |fps|: The number of frames per second in the media content.
SB_EXPORT bool SbMediaIsVideoSupported(SbMediaVideoCodec video_codec,
                                       int frame_width,
                                       int frame_height,
                                       int64_t bitrate,
                                       int fps);

SB_EXPORT bool IsVp9HwDecoderSupported();
SB_EXPORT bool IsVp9GPUDecoderSupported();

// Indicates whether this platform supports |audio_codec| at |bitrate|.
// If |audio_codec| is not supported under any condition, this function
// returns |false|.
//
// |audio_codec|: The media's audio codec (|SbMediaAudioCodec|).
// |bitrate|: The media's bitrate.
SB_EXPORT bool SbMediaIsAudioSupported(SbMediaAudioCodec audio_codec,
                                       int64_t bitrate);

// Indicates whether this platform supports |transfer_id| as a transfer
// characteristics.  If |transfer_id| is not supported under any condition, this
// function returns |false|.
//
// |transfer_id|: The id of transfer charateristics listed in SbMediaTransferId.
SB_EXPORT bool SbMediaIsTransferCharacteristicsSupported(
    SbMediaTransferId transfer_id);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_SUPPORT_INTERNAL_H_
