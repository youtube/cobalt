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

#ifndef COBALT_MEDIA_BASE_STARBOARD_UTILS_H_
#define COBALT_MEDIA_BASE_STARBOARD_UTILS_H_

#include "starboard/drm.h"
#include "starboard/media.h"
#include "third_party/chromium/media/base/audio_codecs.h"
#include "third_party/chromium/media/base/audio_decoder_config.h"
#include "third_party/chromium/media/base/decoder_buffer.h"
#include "third_party/chromium/media/base/demuxer_stream.h"
#include "third_party/chromium/media/base/video_codecs.h"
#include "third_party/chromium/media/base/video_decoder_config.h"
#include "third_party/chromium/media/cobalt/third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/chromium/media/cobalt/ui/gfx/hdr_metadata.h"

namespace cobalt {
namespace media {

SbMediaAudioCodec MediaAudioCodecToSbMediaAudioCodec(::media::AudioCodec codec);
SbMediaVideoCodec MediaVideoCodecToSbMediaVideoCodec(::media::VideoCodec codec);

SbMediaAudioSampleInfo MediaAudioConfigToSbMediaAudioSampleInfo(
    const ::media::AudioDecoderConfig& audio_decoder_config,
    const char* mime_type);

::media::DemuxerStream::Type SbMediaTypeToDemuxerStreamType(SbMediaType type);
SbMediaType DemuxerStreamTypeToSbMediaType(::media::DemuxerStream::Type type);

void FillDrmSampleInfo(const scoped_refptr<::media::DecoderBuffer>& buffer,
                       SbDrmSampleInfo* drm_info,
                       SbDrmSubSampleMapping* subsample_mapping);

SbMediaColorMetadata MediaToSbMediaColorMetadata(
    const ::media::VideoColorSpace& color_space,
    const absl::optional<gfx::HDRMetadata>& hdr_metadata);

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_STARBOARD_UTILS_H_
