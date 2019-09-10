// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include <algorithm>
#include <functional>

#if SB_HAS(PLAYER_FILTER_TESTS)
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

  SbTime duration =
      access_units.back().timestamp() - access_units.front().timestamp();

  SB_DCHECK(duration > 0);

  int64_t total_bitrate = 0;
  for (auto& au : access_units) {
    total_bitrate += au.data().size();
  }

  return total_bitrate * 8 * kSbTimeSecond / duration;
}

static void DeallocateSampleFunc(SbPlayer player,
                                 void* context,
                                 const void* sample_buffer) {
  SB_UNREFERENCED_PARAMETER(player);
  SB_UNREFERENCED_PARAMETER(context);
  SB_UNREFERENCED_PARAMETER(sample_buffer);
}

SbPlayerSampleInfo ConvertToPlayerSampleInfo(
    const VideoDmpReader::AudioAccessUnit& audio_unit) {
  SbPlayerSampleInfo sample_info = {};
  sample_info.buffer = audio_unit.data().data();
  sample_info.buffer_size = static_cast<int>(audio_unit.data().size());
  sample_info.timestamp = audio_unit.timestamp();
  sample_info.drm_info = audio_unit.drm_sample_info();
#if SB_API_VERSION >= 11
  sample_info.type = kSbMediaTypeAudio;
  sample_info.audio_sample_info = audio_unit.audio_sample_info();
#else   // SB_API_VERSION >= 11
  sample_info.video_sample_info = NULL;
#endif  // SB_API_VERSION >= 11
  return sample_info;
}

SbPlayerSampleInfo ConvertToPlayerSampleInfo(
    const VideoDmpReader::VideoAccessUnit& video_unit) {
  SbPlayerSampleInfo sample_info = {};
  sample_info.buffer = video_unit.data().data();
  sample_info.buffer_size = static_cast<int>(video_unit.data().size());
  sample_info.timestamp = video_unit.timestamp();
  sample_info.drm_info = video_unit.drm_sample_info();
#if SB_API_VERSION >= 11
  sample_info.type = kSbMediaTypeVideo;
  sample_info.video_sample_info = video_unit.video_sample_info();
#else   // SB_API_VERSION >= 11
  sample_info.video_sample_info = &video_unit.video_sample_info();
#endif  // SB_API_VERSION >= 11
  return sample_info;
}

}  // namespace

using std::placeholders::_1;
using std::placeholders::_2;

VideoDmpReader::VideoDmpReader(const char* filename)
    : reverse_byte_order_(false) {
  ScopedFile file(filename, kSbFileOpenOnly | kSbFileRead);
  SB_CHECK(file.IsValid()) << "Failed to open " << filename;
  int64_t file_size = file.GetSize();
  SB_CHECK(file_size >= 0);
  read_cb_ = std::bind(&VideoDmpReader::ReadFromFile, this, &file, _1, _2);
  Parse();
}

VideoDmpReader::~VideoDmpReader() {}

SbPlayerSampleInfo VideoDmpReader::GetPlayerSampleInfo(SbMediaType type,
                                                       size_t index) const {
  switch (type) {
    case kSbMediaTypeAudio: {
      SB_DCHECK(index < audio_access_units_.size());
      const AudioAccessUnit& audio_au = audio_access_units_[index];
      return ConvertToPlayerSampleInfo(audio_au);
    }
    case kSbMediaTypeVideo: {
      SB_DCHECK(index < video_access_units_.size());
      const VideoAccessUnit& video_au = video_access_units_[index];
      return ConvertToPlayerSampleInfo(video_au);
    }
  }
  SB_NOTREACHED() << "Unhandled SbMediaType";
  return SbPlayerSampleInfo();
}

SbMediaAudioSampleInfo VideoDmpReader::GetAudioSampleInfo(size_t index) const {
  SB_DCHECK(index < audio_access_units_.size());
  const AudioAccessUnit& au = audio_access_units_[index];
  return au.audio_sample_info();
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
  uint32_t dmp_writer_version;
  Read(read_cb_, reverse_byte_order_, &dmp_writer_version);
  if (dmp_writer_version != kSupportWriterVersion) {
    SB_LOG(ERROR) << "Unsupported input dmp file(" << dmp_writer_version
                  << "). Please regenerate dmp files with"
                  << " right dmp writer. Currently support version "
                  << kSupportWriterVersion << ".";
    return;
  }
  for (;;) {
    uint32_t type;
    int bytes_read = read_cb_(&type, sizeof(type));
    if (bytes_read != sizeof(type)) {
      // Read an invalid number of bytes (corrupt file), or we read zero bytes
      // (end of file).
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
          Read(read_cb_, reverse_byte_order_, &audio_sample_info_);
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
    SbTime first_timestamp = video_access_units_.front().timestamp();
    SbTime second_timestamp = video_access_units_.back().timestamp();
    for (const auto& au : video_access_units_) {
      if (au.timestamp() != first_timestamp &&
          au.timestamp() < second_timestamp) {
        second_timestamp = au.timestamp();
      }
    }
    SB_DCHECK(first_timestamp < second_timestamp);
    video_fps_ = kSbTimeSecond / (second_timestamp - first_timestamp);
  }
}

VideoDmpReader::AudioAccessUnit VideoDmpReader::ReadAudioAccessUnit() {
  SbTime timestamp;
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

  SbMediaAudioSampleInfoWithConfig audio_sample_info;
  Read(read_cb_, reverse_byte_order_, &audio_sample_info);

  return AudioAccessUnit(timestamp,
                         drm_sample_info_present ? &drm_sample_info : NULL,
                         std::move(data), audio_sample_info);
}

VideoDmpReader::VideoAccessUnit VideoDmpReader::ReadVideoAccessUnit() {
  SbTime timestamp;
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

int VideoDmpReader::ReadFromFile(ScopedFile* file,
                                 void* buffer,
                                 int bytes_to_read) {
  return file->Read(static_cast<char*>(buffer), bytes_to_read);
}

}  // namespace video_dmp
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
#endif  // SB_HAS(PLAYER_FILTER_TESTS)
