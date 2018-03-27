// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/media/base/audio_decoder_config.h"
#include "cobalt/media/base/decoder_buffer.h"
#include "cobalt/media/base/demuxer_stream.h"
#include "cobalt/media/base/video_decoder_config.h"
#include "cobalt/media/formats/webm/webm_colour_parser.h"
#include "starboard/drm.h"
#include "starboard/media.h"

namespace cobalt {
namespace media {

SbMediaAudioCodec MediaAudioCodecToSbMediaAudioCodec(AudioCodec codec);
SbMediaVideoCodec MediaVideoCodecToSbMediaVideoCodec(VideoCodec codec);

SbMediaAudioHeader MediaAudioConfigToSbMediaAudioHeader(
    const AudioDecoderConfig& audio_decoder_config);

DemuxerStream::Type SbMediaTypeToDemuxerStreamType(SbMediaType type);
SbMediaType DemuxerStreamTypeToSbMediaType(DemuxerStream::Type type);

void FillDrmSampleInfo(const scoped_refptr<DecoderBuffer>& buffer,
                       SbDrmSampleInfo* drm_info,
                       SbDrmSubSampleMapping* subsample_mapping);

SbMediaColorMetadata MediaToSbMediaColorMetadata(
    const WebMColorMetadata& webm_color_metadata);

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_STARBOARD_UTILS_H_
