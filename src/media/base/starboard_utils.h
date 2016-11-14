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

#ifndef MEDIA_BASE_STARBOARD_UTILS_H_
#define MEDIA_BASE_STARBOARD_UTILS_H_

#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder_config.h"
#include "starboard/drm.h"
#include "starboard/media.h"

namespace media {

SbMediaAudioCodec MediaAudioCodecToSbMediaAudioCodec(AudioCodec codec);
SbMediaVideoCodec MediaVideoCodecToSbMediaVideoCodec(VideoCodec codec);

base::TimeDelta SbMediaTimeToTimeDelta(SbMediaTime timestamp);
SbMediaTime TimeDeltaToSbMediaTime(base::TimeDelta timedelta);

DemuxerStream::Type SbMediaTypeToDemuxerStreamType(SbMediaType type);
SbMediaType DemuxerStreamTypeToSbMediaType(DemuxerStream::Type type);

void FillDrmSampleInfo(const scoped_refptr<DecoderBuffer>& buffer,
                       SbDrmSampleInfo* drm_info,
                       SbDrmSubSampleMapping* subsample_mapping);

}  // namespace media

#endif  // MEDIA_BASE_STARBOARD_UTILS_H_
