// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/dmp_demuxer.h"

#include <algorithm>

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace video_dmp {

namespace {
using shared::starboard::media::AudioStreamInfo;
}

DmpDemuxer::DmpDemuxer(DmpDemuxer& demuxer, SbMediaType type) {
  SB_DCHECK(demuxer.HasMediaTrack(type));

  dmp_readers_[type] = demuxer.GetDmpReader(type);
  Init();
}

DmpDemuxer::DmpDemuxer(MediaTypeToDmpReaderMap& dmp_readers)
    : dmp_readers_(dmp_readers) {
  Init();
}

SbMediaAudioCodec DmpDemuxer::GetAudioCodecType() const {
  if (!HasMediaTrack(kSbMediaTypeAudio)) {
    return kSbMediaAudioCodecNone;
  }
  const auto& dmp_reader = dmp_readers_.find(kSbMediaTypeAudio);
  return dmp_reader->second->audio_codec();
}

SbMediaVideoCodec DmpDemuxer::GetVideoCodecType() const {
  if (!HasMediaTrack(kSbMediaTypeVideo)) {
    return kSbMediaVideoCodecNone;
  }
  const auto& dmp_reader = dmp_readers_.find(kSbMediaTypeVideo);
  return dmp_reader->second->video_codec();
}

bool DmpDemuxer::PeekSample(SbMediaType type, SbPlayerSampleInfo& sample_info) {
  SB_DCHECK(HasMediaTrack(type));

  if (sample_index_[type] >= num_of_samples_[type]) {
    return false;
  }
  sample_info =
      dmp_readers_[type]->GetPlayerSampleInfo(type, sample_index_[type]);

  if (sample_info.timestamp > read_range_.second) {
    return false;
  } else {
    return true;
  }
}

bool DmpDemuxer::ReadSample(SbMediaType type, SbPlayerSampleInfo& sample_info) {
  if (!PeekSample(type, sample_info)) {
    return false;
  }
  ++sample_index_[type];
  return true;
}

bool DmpDemuxer::Seek(SbTime time_to_seek_to) {
  SB_DCHECK(time_to_seek_to >= 0);

  if (time_to_seek_to > duration_) {
    return false;
  }
  for (const auto& dmp_reader : dmp_readers_) {
    int index;
    if (dmp_reader.first == kSbMediaTypeVideo) {
      int current_key_index = -1;
      for (index = 0; index < (dmp_reader.second)->number_of_video_buffers();
           ++index) {
        SbPlayerSampleInfo sample =
            (dmp_reader.second)->GetPlayerSampleInfo(kSbMediaTypeVideo, index);
        if (sample.video_sample_info.is_key_frame) {
          current_key_index = index;
        }
        if (sample.timestamp >= time_to_seek_to) {
          break;
        }
      }
      SB_DCHECK(current_key_index >= 0 &&
                current_key_index < num_of_samples_[kSbMediaTypeVideo]);
      sample_index_[kSbMediaTypeVideo] = current_key_index;
    } else if (dmp_reader.first == kSbMediaTypeAudio) {
      for (index = 0; index < (dmp_reader.second)->number_of_audio_buffers();
           ++index) {
        SbPlayerSampleInfo sample =
            (dmp_reader.second)->GetPlayerSampleInfo(kSbMediaTypeAudio, index);
        if (sample.timestamp >= time_to_seek_to) {
          break;
        }
      }
      // Check seek to last audio.
      sample_index_[kSbMediaTypeAudio] = index;
    }
  }
  read_range_.first = time_to_seek_to;
  read_range_.second = duration_;
  return true;
}

const AudioStreamInfo* DmpDemuxer::GetAudioStreamInfo() const {
  if (!HasMediaTrack(kSbMediaTypeAudio)) {
    return nullptr;
  }
  const auto& dmp_reader = dmp_readers_.find(kSbMediaTypeAudio);
  return &dmp_reader->second->audio_stream_info();
}

int DmpDemuxer::GetCurrentIndex(SbMediaType type) const {
  SB_DCHECK(HasMediaTrack(type));

  const auto& sample_index = sample_index_.find(type);
  SB_DCHECK(sample_index != sample_index_.end());

  return sample_index->second;
}

VideoDmpReader* DmpDemuxer::GetDmpReader(SbMediaType type) const {
  SB_DCHECK(dmp_readers_.count(type) > 0);

  const auto reader = dmp_readers_.find(type);
  return reader->second;
}

void DmpDemuxer::Init() {
  SB_DCHECK(!dmp_readers_.empty());

  for (const auto& dmp_reader : dmp_readers_) {
    sample_index_[dmp_reader.first] = 0;

    // Consider solely audio and video inputs exclusively.
    if (dmp_reader.first == kSbMediaTypeVideo) {
      SB_DCHECK((dmp_reader.second)->video_duration() > 0);
      SB_DCHECK((dmp_reader.second)->number_of_video_buffers() > 0);

      num_of_samples_[kSbMediaTypeVideo] =
          (dmp_reader.second)->number_of_video_buffers();
      duration_ = std::min(duration_, (dmp_reader.second)->video_duration());
    } else if (dmp_reader.first == kSbMediaTypeAudio) {
      // Once the heaac.dmp test file has been updated, recover the check below.
      // SB_DCHECK((dmp_reader.second)->audio_duration() > 0);
      SB_DCHECK((dmp_reader.second)->number_of_audio_buffers() > 0);

      num_of_samples_[kSbMediaTypeAudio] =
          (dmp_reader.second)->number_of_audio_buffers();
      duration_ = std::min(duration_, (dmp_reader.second)->audio_duration());
      // The resolve the issue that the duration of heaac.dmp is 0.
      // Once the heaac.dmp test file has been updated, remove it.
      if (duration_ == 0) {
        SbPlayerSampleInfo sample_info =
            (dmp_reader.second)
                ->GetPlayerSampleInfo(kSbMediaTypeAudio,
                                      num_of_samples_[kSbMediaTypeAudio] - 1);
        duration_ = sample_info.timestamp + 1;
      }
    }
  }
  SB_DCHECK(duration_ > 0);
  read_range_.first = 0;
  read_range_.second = duration_;
}

bool DmpDemuxer::SetReadRange(SbTime start, SbTime end) {
  SB_DCHECK(end > start);
  SB_DCHECK(end <= duration_);
  SB_DCHECK(start >= 0);

  if (!Seek(start)) {
    return false;
  }
  read_range_.first = start;
  read_range_.second = end;
  return true;
}

}  // namespace video_dmp
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
