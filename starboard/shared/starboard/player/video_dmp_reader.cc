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
                                 const void* sample_buffer) {}

SbPlayerSampleInfo ConvertToPlayerSampleInfo(
    const VideoDmpReader::AudioAccessUnit& audio_unit) {
  SbPlayerSampleInfo sample_info = {};
  sample_info.buffer = audio_unit.data().data();
  sample_info.buffer_size = static_cast<int>(audio_unit.data().size());
  sample_info.timestamp = audio_unit.timestamp();
  sample_info.drm_info = audio_unit.drm_sample_info();
  sample_info.type = kSbMediaTypeAudio;
  sample_info.audio_sample_info = audio_unit.audio_sample_info();
  return sample_info;
}

SbPlayerSampleInfo ConvertToPlayerSampleInfo(
    const VideoDmpReader::VideoAccessUnit& video_unit) {
  SbPlayerSampleInfo sample_info = {};
  sample_info.buffer = video_unit.data().data();
  sample_info.buffer_size = static_cast<int>(video_unit.data().size());
  sample_info.timestamp = video_unit.timestamp();
  sample_info.drm_info = video_unit.drm_sample_info();
  sample_info.type = kSbMediaTypeVideo;
  sample_info.video_sample_info = video_unit.video_sample_info();
  return sample_info;
}

}  // namespace

using std::placeholders::_1;
using std::placeholders::_2;

bool VideoDmpReader::Registry::GetDmpInfo(const char* filename,
                                          DmpInfo* dmp_info) const {
  SB_DCHECK(filename);
  SB_DCHECK(dmp_info);

  ScopedLock scoped_lock(mutex_);
  auto iter = dmp_infos_.find(filename);
  if (iter == dmp_infos_.end()) {
    return false;
  }
  *dmp_info = iter->second;
  return true;
}

void VideoDmpReader::Registry::Register(const char* filename,
                                        const DmpInfo& dmp_info) {
  SB_DCHECK(filename);

  ScopedLock scoped_lock(mutex_);
  SB_DCHECK(dmp_infos_.find(filename) == dmp_infos_.end());
  dmp_infos_[filename] = dmp_info;
}

VideoDmpReader::VideoDmpReader(
    const char* filename,
    ReadOnDemandOptions read_on_demand_options /*= kDisableReadOnDemand*/)
    : file_reader_(filename, 1024 * 1024),
      read_cb_(std::bind(&FileCacheReader::Read, &file_reader_, _1, _2)),
      allow_read_on_demand_(read_on_demand_options == kEnableReadOnDemand) {
  bool already_cached = GetRegistry()->GetDmpInfo(filename, &dmp_info_);

  if (already_cached && allow_read_on_demand_) {
    // This is necessary as the current implementation assumes that the address
    // of any access units are never changed during the life time of the object,
    // and keep using it without explicit reference.
    audio_access_units_.reserve(dmp_info_.audio_access_units_size);
    video_access_units_.reserve(dmp_info_.video_access_units_size);
    return;
  }

  Parse();

  if (!already_cached) {
    GetRegistry()->Register(filename, dmp_info_);
  }
}

VideoDmpReader::~VideoDmpReader() {}

std::string VideoDmpReader::audio_mime_type() const {
  if (dmp_info_.audio_codec == kSbMediaAudioCodecNone) {
    return "";
  }
  std::stringstream ss;
  switch (dmp_info_.audio_codec) {
    case kSbMediaAudioCodecAac:
      ss << "audio/mp4; codecs=\"mp4a.40.2\";";
      break;
    case kSbMediaAudioCodecOpus:
      ss << "audio/webm; codecs=\"opus\";";
      break;
    case kSbMediaAudioCodecAc3:
      ss << "audio/mp4; codecs=\"ac-3\";";
      break;
    case kSbMediaAudioCodecEac3:
      ss << "audio/mp4; codecs=\"ec-3\";";
      break;
    default:
      SB_NOTREACHED();
  }
  ss << " channels=" << dmp_info_.audio_sample_info.number_of_channels;
  return ss.str();
}

std::string VideoDmpReader::video_mime_type() {
  if (dmp_info_.video_codec == kSbMediaVideoCodecNone) {
    return "";
  }
  // TODO: Support HDR
  std::stringstream ss;
  switch (dmp_info_.video_codec) {
    case kSbMediaVideoCodecH264:
      ss << "video/mp4; codecs=\"avc1.4d402a\";";
      break;
    case kSbMediaVideoCodecVp9:
      ss << "video/webm; codecs=\"vp9\";";
      break;
    case kSbMediaVideoCodecAv1:
      ss << "video/mp4; codecs=\"av01.0.08M.08\";";
      break;
    default:
      SB_NOTREACHED();
  }
  if (number_of_video_buffers() > 0) {
    const auto& video_sample_info =
        GetPlayerSampleInfo(kSbMediaTypeVideo, 0).video_sample_info;
    ss << "width=" << video_sample_info.frame_width
       << "; height=" << video_sample_info.frame_height << ";";
  }
  ss << " framerate=" << dmp_info_.video_fps;
  return ss.str();
}

SbPlayerSampleInfo VideoDmpReader::GetPlayerSampleInfo(SbMediaType type,
                                                       size_t index) {
  EnsureSampleLoaded(type, index);

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

const SbMediaAudioSampleInfo& VideoDmpReader::GetAudioSampleInfo(size_t index) {
  EnsureSampleLoaded(kSbMediaTypeAudio, index);

  SB_DCHECK(index < audio_access_units_.size());
  const AudioAccessUnit& au = audio_access_units_[index];
  return au.audio_sample_info();
}

void VideoDmpReader::ParseHeader(uint32_t* dmp_writer_version) {
  SB_DCHECK(dmp_writer_version);
  SB_DCHECK(!reverse_byte_order_.has_engaged());

  int64_t file_size = file_reader_.GetSize();
  SB_CHECK(file_size >= 0);

  reverse_byte_order_ = false;
  uint32_t byte_order_mark;
  Read(read_cb_, reverse_byte_order_.value(), &byte_order_mark);
  if (byte_order_mark != kByteOrderMark) {
    std::reverse(reinterpret_cast<uint8_t*>(&byte_order_mark),
                 reinterpret_cast<uint8_t*>(&byte_order_mark + 1));
    SB_CHECK(byte_order_mark == kByteOrderMark);
    reverse_byte_order_ = true;
  }

  Read(read_cb_, reverse_byte_order_.value(), dmp_writer_version);
}

bool VideoDmpReader::ParseOneRecord() {
  uint32_t type;
  int bytes_read = read_cb_(&type, sizeof(type));
  if (bytes_read != sizeof(type)) {
    // Read an invalid number of bytes (corrupt file), or we read zero bytes
    // (end of file).  Return false to signal the end of reading.
    return false;
  }
  if (reverse_byte_order_.value()) {
    std::reverse(reinterpret_cast<uint8_t*>(&type),
                 reinterpret_cast<uint8_t*>(&type + 1));
  }
  switch (type) {
    case kRecordTypeAudioConfig:
      Read(read_cb_, reverse_byte_order_.value(), &dmp_info_.audio_codec);
      if (dmp_info_.audio_codec != kSbMediaAudioCodecNone) {
        Read(read_cb_, reverse_byte_order_.value(),
             &dmp_info_.audio_sample_info);
      }
      break;
    case kRecordTypeVideoConfig:
      Read(read_cb_, reverse_byte_order_.value(), &dmp_info_.video_codec);
      break;
    case kRecordTypeAudioAccessUnit:
      audio_access_units_.push_back(ReadAudioAccessUnit());
      break;
    case kRecordTypeVideoAccessUnit:
      video_access_units_.push_back(ReadVideoAccessUnit());
      break;
    default:
      SB_NOTREACHED() << type;
      return false;
  }

  return true;
}

void VideoDmpReader::Parse() {
  SB_DCHECK(!reverse_byte_order_.has_engaged());

  uint32_t dmp_writer_version = 0;
  ParseHeader(&dmp_writer_version);
  if (dmp_writer_version != kSupportedWriterVersion) {
    SB_LOG(ERROR) << "Unsupported input dmp file(" << dmp_writer_version
                  << "). Please regenerate dmp files with"
                  << " right dmp writer. Currently support version "
                  << kSupportedWriterVersion << ".";
    return;
  }

  while (ParseOneRecord()) {
  }

  dmp_info_.audio_access_units_size = audio_access_units_.size();
  dmp_info_.audio_bitrate = CalculateAverageBitrate(audio_access_units_);
  dmp_info_.video_access_units_size = video_access_units_.size();
  dmp_info_.video_bitrate = CalculateAverageBitrate(video_access_units_);

  // Guestimate the audio duration.
  if (audio_access_units_.size() > 1) {
    auto frame_duration =
        audio_access_units_[audio_access_units_.size() - 1].timestamp() -
        audio_access_units_[audio_access_units_.size() - 2].timestamp();
    dmp_info_.audio_duration =
        audio_access_units_.back().timestamp() + frame_duration;
  }
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
    SbTime frame_duration = second_timestamp - first_timestamp;
    dmp_info_.video_fps = kSbTimeSecond / frame_duration;

    SbTime last_frame_timestamp = video_access_units_.back().timestamp();
    for (auto it = video_access_units_.rbegin();
         it != video_access_units_.rend(); it++) {
      if (it->timestamp() > last_frame_timestamp) {
        last_frame_timestamp = it->timestamp();
      }
      if (it->video_sample_info().is_key_frame) {
        break;
      }
    }
    dmp_info_.video_duration = last_frame_timestamp + frame_duration;
  }
}

void VideoDmpReader::EnsureSampleLoaded(SbMediaType type, size_t index) {
  if (!reverse_byte_order_.has_engaged()) {
    uint32_t dmp_writer_version = 0;
    ParseHeader(&dmp_writer_version);
    SB_DCHECK(dmp_writer_version == kSupportedWriterVersion);
  }

  if (type == kSbMediaTypeAudio) {
    while (index >= audio_access_units_.size() && ParseOneRecord()) {
    }
    SB_CHECK(index < audio_access_units_.size());
  } else {
    SB_DCHECK(type == kSbMediaTypeVideo);
    while (index >= video_access_units_.size() && ParseOneRecord()) {
    }
    SB_CHECK(index < video_access_units_.size());
  }
}

VideoDmpReader::AudioAccessUnit VideoDmpReader::ReadAudioAccessUnit() {
  SbTime timestamp;
  Read(read_cb_, reverse_byte_order_.value(), &timestamp);

  bool drm_sample_info_present;
  Read(read_cb_, reverse_byte_order_.value(), &drm_sample_info_present);

  SbDrmSampleInfoWithSubSampleMapping drm_sample_info;
  if (drm_sample_info_present) {
    Read(read_cb_, reverse_byte_order_.value(), &drm_sample_info);
  }

  uint32_t size;
  Read(read_cb_, reverse_byte_order_.value(), &size);
  std::vector<uint8_t> data(size);
  Read(read_cb_, data.data(), size);

  SbMediaAudioSampleInfoWithConfig audio_sample_info;
  Read(read_cb_, reverse_byte_order_.value(), &audio_sample_info);

  return AudioAccessUnit(timestamp,
                         drm_sample_info_present ? &drm_sample_info : NULL,
                         std::move(data), audio_sample_info);
}

VideoDmpReader::VideoAccessUnit VideoDmpReader::ReadVideoAccessUnit() {
  SbTime timestamp;
  Read(read_cb_, reverse_byte_order_.value(), &timestamp);

  bool drm_sample_info_present;
  Read(read_cb_, reverse_byte_order_.value(), &drm_sample_info_present);

  SbDrmSampleInfoWithSubSampleMapping drm_sample_info;
  if (drm_sample_info_present) {
    Read(read_cb_, reverse_byte_order_.value(), &drm_sample_info);
  }

  uint32_t size;
  Read(read_cb_, reverse_byte_order_.value(), &size);
  std::vector<uint8_t> data(size);
  Read(read_cb_, data.data(), size);

  SbMediaVideoSampleInfoWithOptionalColorMetadata video_sample_info;
  Read(read_cb_, reverse_byte_order_.value(), &video_sample_info);

  return VideoAccessUnit(timestamp,
                         drm_sample_info_present ? &drm_sample_info : NULL,
                         std::move(data), video_sample_info);
}

// static
VideoDmpReader::Registry* VideoDmpReader::GetRegistry() {
  static Registry s_registry;
  return &s_registry;
}

}  // namespace video_dmp
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
