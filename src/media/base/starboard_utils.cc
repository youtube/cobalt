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

#include "media/base/starboard_utils.h"

#include "base/logging.h"

using base::Time;
using base::TimeDelta;

namespace media {

SbMediaAudioCodec MediaAudioCodecToSbMediaAudioCodec(AudioCodec codec) {
  if (codec == kCodecAAC) {
    return kSbMediaAudioCodecAac;
  } else if (codec == kCodecVorbis) {
    return kSbMediaAudioCodecVorbis;
  } else if (codec == kCodecOpus) {
    return kSbMediaAudioCodecOpus;
  }
  DLOG(ERROR) << "Unsupported audio codec " << codec;
  return kSbMediaAudioCodecNone;
}

SbMediaVideoCodec MediaVideoCodecToSbMediaVideoCodec(VideoCodec codec) {
  if (codec == kCodecH264) {
    return kSbMediaVideoCodecH264;
  } else if (codec == kCodecVC1) {
    return kSbMediaVideoCodecVc1;
  } else if (codec == kCodecMPEG2) {
    return kSbMediaVideoCodecMpeg2;
  } else if (codec == kCodecTheora) {
    return kSbMediaVideoCodecTheora;
  } else if (codec == kCodecVP8) {
    return kSbMediaVideoCodecVp8;
  } else if (codec == kCodecVP9) {
    return kSbMediaVideoCodecVp9;
  }
  DLOG(ERROR) << "Unsupported video codec " << codec;
  return kSbMediaVideoCodecNone;
}

TimeDelta SbMediaTimeToTimeDelta(SbMediaTime timestamp) {
  return TimeDelta::FromMicroseconds(timestamp * Time::kMicrosecondsPerSecond /
                                     kSbMediaTimeSecond);
}

SbMediaTime TimeDeltaToSbMediaTime(TimeDelta timedelta) {
  return timedelta.InMicroseconds() * kSbMediaTimeSecond /
         Time::kMicrosecondsPerSecond;
}

DemuxerStream::Type SbMediaTypeToDemuxerStreamType(SbMediaType type) {
  if (type == kSbMediaTypeAudio) {
    return DemuxerStream::AUDIO;
  }
  DCHECK_EQ(type, kSbMediaTypeVideo);
  return DemuxerStream::VIDEO;
}

SbMediaType DemuxerStreamTypeToSbMediaType(DemuxerStream::Type type) {
  if (type == DemuxerStream::AUDIO) {
    return kSbMediaTypeAudio;
  }
  DCHECK_EQ(type, DemuxerStream::VIDEO);
  return kSbMediaTypeVideo;
}

void FillDrmSampleInfo(const scoped_refptr<DecoderBuffer>& buffer,
                       SbDrmSampleInfo* drm_info,
                       SbDrmSubSampleMapping* subsample_mapping) {
  DCHECK(drm_info);
  DCHECK(subsample_mapping);

  const DecryptConfig* config = buffer->GetDecryptConfig();
  if (!config || config->iv().empty() || config->key_id().empty()) {
    drm_info->initialization_vector_size = 0;
    drm_info->identifier_size = 0;
    drm_info->subsample_count = 0;
    drm_info->subsample_mapping = NULL;
    return;
  }

  DCHECK_LE(config->iv().size(), sizeof(drm_info->initialization_vector));
  DCHECK_LE(config->key_id().size(), sizeof(drm_info->identifier));

  if (config->iv().size() > sizeof(drm_info->initialization_vector) ||
      config->key_id().size() > sizeof(drm_info->identifier)) {
    drm_info->initialization_vector_size = 0;
    drm_info->identifier_size = 0;
    drm_info->subsample_count = 0;
    drm_info->subsample_mapping = NULL;
    return;
  }

  memcpy(drm_info->initialization_vector, &config->iv()[0],
         config->iv().size());
  drm_info->initialization_vector_size = config->iv().size();
  memcpy(drm_info->identifier, &config->key_id()[0], config->key_id().size());
  drm_info->identifier_size = config->key_id().size();
  drm_info->subsample_count = config->subsamples().size();

  if (drm_info->subsample_count > 0) {
    COMPILE_ASSERT(sizeof(SbDrmSubSampleMapping) == sizeof(SubsampleEntry),
                   SubSampleEntrySizesMatch);
    drm_info->subsample_mapping =
        reinterpret_cast<const SbDrmSubSampleMapping*>(
            &config->subsamples()[0]);
  } else {
    drm_info->subsample_count = 1;
    drm_info->subsample_mapping = subsample_mapping;
    subsample_mapping->clear_byte_count = 0;
    subsample_mapping->encrypted_byte_count = buffer->GetDataSize();
  }
}

}  // namespace media
