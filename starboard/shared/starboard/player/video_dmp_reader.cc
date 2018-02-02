// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "starboard/shared/starboard/player/video_dmp_reader.h"

#include <functional>

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace video_dmp {

namespace {

template <typename AccessUnit>
int64_t CalculateAverageBitrate(const std::vector<AccessUnit>& access_units) {
  if (access_units.empty()) {
    return 0;
  }

  if (access_units.size() == 1) {
    // A guestimated bitrate of 1k.
    return 1024;
  }

  SbMediaTime duration =
      access_units.back().timestamp() - access_units.front().timestamp();

  SB_DCHECK(duration > 0);

  int64_t total_bitrate = 0;
  for (auto& au : access_units) {
    total_bitrate += au.data().size();
  }

  return total_bitrate * 8 * kSbMediaTimeSecond / duration;
}

static void DeallocateSampleFunc(SbPlayer player,
                                 void* context,
                                 const void* sample_buffer) {
  SB_UNREFERENCED_PARAMETER(player);
  SB_UNREFERENCED_PARAMETER(context);
  SB_UNREFERENCED_PARAMETER(sample_buffer);
}

}  // namespace

using std::placeholders::_1;
using std::placeholders::_2;

VideoDmpReader::VideoDmpReader(const char* filename)
    : reverse_byte_order_(false),
      read_cb_(std::bind(&VideoDmpReader::ReadFromFile, this, _1, _2)) {
  SB_CHECK(SbFileCanOpen(filename, kSbFileOpenOnly | kSbFileRead));
  file_ = SbFileOpen(filename, kSbFileOpenOnly | kSbFileRead, NULL, NULL);
  SB_DCHECK(SbFileIsValid(file_));

  Parse();

  SbFileClose(file_);
}

VideoDmpReader::~VideoDmpReader() {}

scoped_refptr<InputBuffer> VideoDmpReader::GetAudioInputBuffer(
    size_t index) const {
  SB_DCHECK(index < audio_access_units_.size());
  const AudioAccessUnit& au = audio_access_units_[index];
  return new InputBuffer(kSbMediaTypeAudio, DeallocateSampleFunc, NULL, NULL,
                         au.data().data(), static_cast<int>(au.data().size()),
                         au.timestamp(), NULL, NULL);
}

scoped_refptr<InputBuffer> VideoDmpReader::GetVideoInputBuffer(
    size_t index) const {
  SB_DCHECK(index < video_access_units_.size());
  const VideoAccessUnit& au = video_access_units_[index];
  return new InputBuffer(kSbMediaTypeVideo, DeallocateSampleFunc, NULL, NULL,
                         au.data().data(), static_cast<int>(au.data().size()),
                         au.timestamp(), &au.video_sample_info(), NULL);
}

void VideoDmpReader::Parse() {
  reverse_byte_order_ = false;
  uint32_t byte_order_mark;
  Read(read_cb_, reverse_byte_order_, &byte_order_mark);
  if (byte_order_mark != kByteOrderMark) {
    std::reverse(reinterpret_cast<uint8_t*>(&byte_order_mark),
                 reinterpret_cast<uint8_t*>(&byte_order_mark + 1));
    SB_DCHECK(byte_order_mark == kByteOrderMark);
    if (byte_order_mark != kByteOrderMark) {
      SB_LOG(ERROR) << "Invalid BOM" << byte_order_mark;
      return;
    }
    reverse_byte_order_ = true;
  }
  for (;;) {
    uint32_t type;
    int bytes_read = ReadFromFile(&type, sizeof(type));
    if (bytes_read <= 0) {
      break;
    }
    if (reverse_byte_order_) {
      std::reverse(reinterpret_cast<uint8_t*>(&type),
                   reinterpret_cast<uint8_t*>(&type + 1));
    }
    switch (type) {
      case kRecordTypeAudioConfig:
        Read(read_cb_, reverse_byte_order_, &audio_codec_);
        if (audio_codec_ != kSbMediaAudioCodecNone) {
          Read(read_cb_, reverse_byte_order_, &audio_header_);
        }
        break;
      case kRecordTypeVideoConfig:
        Read(read_cb_, reverse_byte_order_, &video_codec_);
        break;
      case kRecordTypeAudioAccessUnit:
        audio_access_units_.push_back(ReadAudioAccessUnit());
        break;
      case kRecordTypeVideoAccessUnit:
        video_access_units_.push_back(ReadVideoAccessUnit());
        break;
      default:
        SB_NOTREACHED() << type;
        break;
    }
  }
  audio_bitrate_ = CalculateAverageBitrate(audio_access_units_);
  video_bitrate_ = CalculateAverageBitrate(video_access_units_);

  // Guestimate the video fps.
  if (video_access_units_.size() > 1) {
    SbMediaTime first_pts = video_access_units_.front().timestamp();
    SbMediaTime second_pts = video_access_units_.back().timestamp();
    for (const auto& au : video_access_units_) {
      if (au.timestamp() != first_pts && au.timestamp() < second_pts) {
        second_pts = au.timestamp();
      }
    }
    SB_DCHECK(first_pts < second_pts);
    video_fps_ = kSbMediaTimeSecond / (second_pts - first_pts);
  }
}

VideoDmpReader::AudioAccessUnit VideoDmpReader::ReadAudioAccessUnit() {
  SbMediaTime timestamp;
  Read(read_cb_, reverse_byte_order_, &timestamp);

  bool drm_sample_info_present;
  Read(read_cb_, reverse_byte_order_, &drm_sample_info_present);

  SbDrmSampleInfoWithSubSampleMapping drm_sample_info;
  if (drm_sample_info_present) {
    Read(read_cb_, reverse_byte_order_, &drm_sample_info);
  }

  uint32_t size;
  Read(read_cb_, reverse_byte_order_, &size);
  std::vector<uint8_t> data(size);
  Read(read_cb_, data.data(), size);

  return AudioAccessUnit(timestamp,
                         drm_sample_info_present ? &drm_sample_info : NULL,
                         std::move(data));
}

VideoDmpReader::VideoAccessUnit VideoDmpReader::ReadVideoAccessUnit() {
  SbMediaTime timestamp;
  Read(read_cb_, reverse_byte_order_, &timestamp);

  bool drm_sample_info_present;
  Read(read_cb_, reverse_byte_order_, &drm_sample_info_present);

  SbDrmSampleInfoWithSubSampleMapping drm_sample_info;
  if (drm_sample_info_present) {
    Read(read_cb_, reverse_byte_order_, &drm_sample_info);
  }

  uint32_t size;
  Read(read_cb_, reverse_byte_order_, &size);
  std::vector<uint8_t> data(size);
  Read(read_cb_, data.data(), size);

  SbMediaVideoSampleInfoWithOptionalColorMetadata video_sample_info;
  Read(read_cb_, reverse_byte_order_, &video_sample_info);

  return VideoAccessUnit(timestamp,
                         drm_sample_info_present ? &drm_sample_info : NULL,
                         std::move(data), video_sample_info);
}

int VideoDmpReader::ReadFromFile(void* buffer, int bytes_to_read) {
  return SbFileRead(file_, static_cast<char*>(buffer), bytes_to_read);
}

}  // namespace video_dmp
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
