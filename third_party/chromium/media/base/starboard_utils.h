// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef MEDIA_BASE_STARBOARD_UTILS_H_
#define MEDIA_BASE_STARBOARD_UTILS_H_

#if !defined(STARBOARD)
#error "This file only works with Cobalt/Starboard."
#endif  // !defined(STARBOARD)

#include "starboard/drm.h"
#include "starboard/media.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

SbMediaAudioCodec MediaAudioCodecToSbMediaAudioCodec(AudioCodec codec);
SbMediaVideoCodec MediaVideoCodecToSbMediaVideoCodec(VideoCodec codec);

SbMediaAudioStreamInfo MediaAudioConfigToSbMediaAudioStreamInfo(
    const AudioDecoderConfig& audio_decoder_config,
    const char* mime_type);

DemuxerStream::Type SbMediaTypeToDemuxerStreamType(SbMediaType type);
SbMediaType DemuxerStreamTypeToSbMediaType(DemuxerStream::Type type);

void FillDrmSampleInfo(const scoped_refptr<DecoderBuffer>& buffer,
                       SbDrmSampleInfo* drm_info,
                       SbDrmSubSampleMapping* subsample_mapping);

SbMediaColorMetadata MediaToSbMediaColorMetadata(
    const VideoColorSpace& color_space,
    const absl::optional<gfx::HDRMetadata>& hdr_metadata,
    const std::string& mime_type);

int GetSbMediaVideoBufferBudget(const VideoDecoderConfig* video_config,
                                const std::string& mime_type);

// Extract the value of "codecs" parameter from |mime_type|. It will return
// "avc1.42E01E" for `video/mp4; codecs="avc1.42E01E"`.
// Note that this function assumes that the input is always valid and does
// minimum validation.
// This function is exposed as a public function so it can be tested.
// TODO(b/232559177): Unify the implementations once `MimeType` is moved to
//                    "starboard/common".
std::string ExtractCodecs(const std::string& mime_type);

}  // namespace media

#endif  // MEDIA_BASE_STARBOARD_UTILS_H_
