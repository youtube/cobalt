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
}  // namespace

DmpDemuxer::DmpDemuxer(const MediaTypeToDmpReaderMap& dmp_readers)
    : dmp_readers_(dmp_readers) {
  Init();
}

SbMediaAudioCodec DmpDemuxer::GetAudioCodec() const {
  SB_DCHECK(HasMediaTrack(kSbMediaTypeAudio));

  const auto& dmp_reader = dmp_readers_.find(kSbMediaTypeAudio);
  return dmp_reader->second->audio_codec();
}

SbMediaVideoCodec DmpDemuxer::GetVideoCodec() const {
  SB_DCHECK(HasMediaTrack(kSbMediaTypeVideo));

  const auto& dmp_reader = dmp_readers_.find(kSbMediaTypeVideo);
  return dmp_reader->second->video_codec();
}

void DmpDemuxer::GetReadRange(SbTime* start, SbTime* end) const {
  if (start) {
    *start = read_range_.first;
  }
  if (end) {
    *end = read_range_.second;
  }
}

bool DmpDemuxer::PeekSample(SbMediaType type, SbPlayerSampleInfo* sample_info) {
  SB_DCHECK(HasMediaTrack(type));

  if (sample_index_[type] >= num_of_samples_[type]) {
    return false;
  }
  *sample_info =
      dmp_readers_[type]->GetPlayerSampleInfo(type, sample_index_[type]);
  if (sample_info->timestamp >= read_range_.second) {
    if (type == kSbMediaTypeVideo) {
      int index = sample_index_[type] + 1;
      while (index < num_of_samples_[type]) {
        SbPlayerSampleInfo info =
            dmp_readers_[type]->GetPlayerSampleInfo(type, index);
        if (info.timestamp < read_range_.second) {
          return true;
        }
        if (info.video_sample_info.is_key_frame) {
          break;
        }
        ++index;
      }
    }
    return false;
  } else {
    return true;
  }
}

bool DmpDemuxer::ReadSample(SbMediaType type, SbPlayerSampleInfo* sample_info) {
  if (!PeekSample(type, sample_info)) {
    return false;
  }
  ++sample_index_[type];
  return true;
}

bool DmpDemuxer::Seek(SbTime time_to_seek_to) {
  SB_DCHECK(time_to_seek_to >= 0);

  // The legal interval is [0, duration).
  if (time_to_seek_to >= duration_) {
    return false;
  }
  for (const auto& iter : dmp_readers_) {
    SbMediaType media_type = iter.first;
    SB_DCHECK(media_type == kSbMediaTypeAudio ||
              media_type == kSbMediaTypeVideo);
    const auto& dmp_reader = iter.second;
    // The first key is always a key frame which is guaranteed in init().
    int current_key_index = 0;
    int num_of_buffers = media_type == kSbMediaTypeVideo
                             ? dmp_reader->number_of_video_buffers()
                             : dmp_reader->number_of_audio_buffers();
    for (int index = 1; index < num_of_buffers; ++index) {
      SbPlayerSampleInfo sample_info =
          dmp_reader->GetPlayerSampleInfo(media_type, index);
      if (sample_info.timestamp >= time_to_seek_to) {
        break;
      }
      if (media_type == kSbMediaTypeAudio ||
          sample_info.video_sample_info.is_key_frame) {
        current_key_index = index;
      }
    }
    SB_DCHECK(current_key_index >= 0 &&
              current_key_index < num_of_samples_[media_type]);
    sample_index_[media_type] = current_key_index;
  }

  read_range_.first = time_to_seek_to;
  read_range_.second = duration_;
  return true;
}

const AudioStreamInfo* DmpDemuxer::GetAudioStreamInfo() const {
  SB_DCHECK(HasMediaTrack(kSbMediaTypeAudio));

  const auto& dmp_reader = dmp_readers_.find(kSbMediaTypeAudio);
  return &dmp_reader->second->audio_stream_info();
}

int DmpDemuxer::GetCurrentIndex(SbMediaType type) const {
  SB_DCHECK(HasMediaTrack(type));

  const auto& iter = sample_index_.find(type);
  SB_DCHECK(iter != sample_index_.end());

  return iter->second;
}

VideoDmpReader* DmpDemuxer::GetDmpReader(SbMediaType type) const {
  SB_DCHECK(HasMediaTrack(type));

  return dmp_readers_.find(type)->second;
}

void DmpDemuxer::Init() {
  SB_DCHECK(!dmp_readers_.empty());

  for (const auto& iter : dmp_readers_) {
    sample_index_[iter.first] = 0;

    SbMediaType media_type = iter.first;
    const auto& dmp_reader = iter.second;

    // Only supports the first frame being designated as the key frame, and its
    // presentation timestamp (PTS) must be set to 0..
    SbPlayerSampleInfo sample = dmp_reader->GetPlayerSampleInfo(media_type, 0);
    SB_DCHECK(media_type == kSbMediaTypeAudio ||
              sample.video_sample_info.is_key_frame);
    SB_DCHECK(sample.timestamp == 0);

    if (media_type == kSbMediaTypeVideo) {
      SB_DCHECK(dmp_reader->video_duration() > 0);
      SB_DCHECK(dmp_reader->number_of_video_buffers() > 0);

      num_of_samples_[kSbMediaTypeVideo] =
          dmp_reader->number_of_video_buffers();
      duration_ = std::min(duration_, dmp_reader->video_duration());
    } else if (media_type == kSbMediaTypeAudio) {
      SB_DCHECK(dmp_reader->number_of_audio_buffers() > 0);

      num_of_samples_[kSbMediaTypeAudio] =
          dmp_reader->number_of_audio_buffers();
      // TODO(b/275377840): remove it after update heaac.dmp.
      if (dmp_reader->audio_duration() == 0) {
        SbPlayerSampleInfo sample_info = dmp_reader->GetPlayerSampleInfo(
            kSbMediaTypeAudio, num_of_samples_[kSbMediaTypeAudio] - 1);
        duration_ = std::min(duration_, sample_info.timestamp + 1);
      } else {
        SB_DCHECK(dmp_reader->audio_duration() > 0);
        duration_ = std::min(duration_, dmp_reader->audio_duration());
      }
    }
  }
  SB_DCHECK(duration_ > 0);
  read_range_.first = 0;
  read_range_.second = duration_;
}

// Range is in [start, end).
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
