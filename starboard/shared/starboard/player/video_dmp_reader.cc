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
#include <iomanip>

#include "starboard/common/check_op.h"
#include "starboard/shared/starboard/media/iamf_util.h"

namespace starboard {

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

  int64_t duration =
      access_units.back().timestamp() - access_units.front().timestamp();

  SB_DCHECK_GT(duration, 0);

  int64_t total_bitrate = 0;
  for (auto& au : access_units) {
    total_bitrate += au.data().size();
  }

  return total_bitrate * 8 * 1'000'000LL / duration;
}

SbPlayerSampleInfo ConvertToPlayerSampleInfo(
    const VideoDmpReader::AudioAccessUnit& audio_unit) {
  SbPlayerSampleInfo sample_info = {};
  sample_info.buffer = audio_unit.data().data();
  sample_info.buffer_size = static_cast<int>(audio_unit.data().size());
  sample_info.timestamp = audio_unit.timestamp();
  sample_info.drm_info = audio_unit.drm_sample_info();
  sample_info.type = kSbMediaTypeAudio;
  audio_unit.audio_sample_info().ConvertTo(&sample_info.audio_sample_info);
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
  video_unit.video_sample_info().ConvertTo(&sample_info.video_sample_info);
  return sample_info;
}

}  // namespace

using std::placeholders::_1;
using std::placeholders::_2;

bool VideoDmpReader::Registry::GetDmpInfo(const std::string& filename,
                                          DmpInfo* dmp_info) const {
  SB_DCHECK(!filename.empty());
  SB_DCHECK(dmp_info);

  std::lock_guard scoped_lock(mutex_);
  auto iter = dmp_infos_.find(filename);
  if (iter == dmp_infos_.end()) {
    return false;
  }
  *dmp_info = iter->second;
  return true;
}

void VideoDmpReader::Registry::Register(const std::string& filename,
                                        const DmpInfo& dmp_info) {
  SB_DCHECK(!filename.empty());

  std::lock_guard scoped_lock(mutex_);
  SB_DCHECK(dmp_infos_.find(filename) == dmp_infos_.end());
  dmp_infos_[filename] = dmp_info;
}

VideoDmpReader::VideoDmpReader(
    const char* filename,
    ReadOnDemandOptions read_on_demand_options /*= kDisableReadOnDemand*/)
    : allow_read_on_demand_(read_on_demand_options == kEnableReadOnDemand),
      file_reader_(filename, 1024 * 1024),
      read_cb_(std::bind(&FileCacheReader::Read, &file_reader_, _1, _2)) {
  bool already_cached =
      GetRegistry()->GetDmpInfo(file_reader_.GetAbsolutePathName(), &dmp_info_);

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
    GetRegistry()->Register(file_reader_.GetAbsolutePathName(), dmp_info_);
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
    case kSbMediaAudioCodecVorbis:
      ss << "audio/webm; codecs=\"vorbis\";";
      break;
    case kSbMediaAudioCodecMp3:
      ss << "audio/mpeg; codecs=\"mp3\";";
      break;
    case kSbMediaAudioCodecFlac:
      ss << "audio/ogg; codecs=\"flac\";";
      break;
    case kSbMediaAudioCodecPcm:
      ss << "audio/wav; codecs=\"1\";";
      break;
    case kSbMediaAudioCodecIamf:
      SB_CHECK(dmp_info_.iamf_primary_profile.has_value());
      // Only Opus IAMF substreams are currently supported.
      ss << "audio/mp4; codecs=\"iamf.";
      ss << std::setw(3) << std::setfill('0') << std::hex
         << static_cast<int>(*dmp_info_.iamf_primary_profile) << "."
         << std::setw(3) << std::setfill('0') << std::hex
         << static_cast<int>(dmp_info_.iamf_additional_profile.value_or(0))
         << ".Opus\";";
      break;
    default:
      SB_NOTREACHED() << "Unsupported audio codec: " << dmp_info_.audio_codec;
  }

  ss << " channels="
     << dmp_info_.audio_sample_info.stream_info.number_of_channels;
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
    case kSbMediaVideoCodecVp8:
      ss << "video/webm; codecs=\"vp8\";";
      break;
    case kSbMediaVideoCodecH265:
      ss << "video/mp4; codecs=\"hev1.1.6.L123.B0\";";
      break;
    default:
      SB_NOTREACHED() << "Unsupported video codec: " << dmp_info_.video_codec;
  }
  if (number_of_video_buffers() > 0) {
    const auto& video_stream_info = this->video_stream_info();
    ss << "width=" << video_stream_info.frame_size.width
       << "; height=" << video_stream_info.frame_size.height << ";";
  }
  ss << " framerate=" << dmp_info_.video_fps;
  return ss.str();
}

SbPlayerSampleInfo VideoDmpReader::GetPlayerSampleInfo(SbMediaType type,
                                                       size_t index) {
  EnsureSampleLoaded(type, index);

  switch (type) {
    case kSbMediaTypeAudio: {
      SB_DCHECK_LT(index, audio_access_units_.size());
      const AudioAccessUnit& audio_au = audio_access_units_[index];
      return ConvertToPlayerSampleInfo(audio_au);
    }
    case kSbMediaTypeVideo: {
      SB_DCHECK_LT(index, video_access_units_.size());
      const VideoAccessUnit& video_au = video_access_units_[index];
      return ConvertToPlayerSampleInfo(video_au);
    }
  }
  SB_NOTREACHED() << "Unhandled SbMediaType";
  return SbPlayerSampleInfo();
}

const AudioSampleInfo& VideoDmpReader::GetAudioSampleInfo(size_t index) {
  EnsureSampleLoaded(kSbMediaTypeAudio, index);

  SB_DCHECK_LT(index, audio_access_units_.size());
  const AudioAccessUnit& au = audio_access_units_[index];
  return au.audio_sample_info();
}

void VideoDmpReader::ParseHeader(uint32_t* dmp_writer_version) {
  SB_DCHECK(dmp_writer_version);
  SB_DCHECK(!reverse_byte_order_.has_value());

  int64_t file_size = file_reader_.GetSize();
  SB_CHECK_GE(file_size, 0);

  reverse_byte_order_ = false;
  uint32_t byte_order_mark;
  Read(read_cb_, reverse_byte_order_.value(), &byte_order_mark);
  if (byte_order_mark != kByteOrderMark) {
    std::reverse(reinterpret_cast<uint8_t*>(&byte_order_mark),
                 reinterpret_cast<uint8_t*>(&byte_order_mark + 1));
    SB_CHECK_EQ(byte_order_mark, kByteOrderMark);
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
        SB_DCHECK_EQ(dmp_info_.audio_codec,
                     dmp_info_.audio_sample_info.stream_info.codec);
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
  SB_DCHECK(!reverse_byte_order_.has_value());

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

  if (dmp_info_.audio_codec == kSbMediaAudioCodecIamf &&
      !dmp_info_.iamf_primary_profile.has_value()) {
    auto result =
        IamfMimeUtil::ParseIamfSequenceHeaderObu(audio_access_units_[0].data());
    SB_CHECK(result) << result.error();
    dmp_info_.iamf_primary_profile = result->primary_profile;
    dmp_info_.iamf_additional_profile = result->additional_profile;
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
    int64_t first_timestamp = video_access_units_.front().timestamp();
    int64_t second_timestamp = video_access_units_.back().timestamp();
    for (const auto& au : video_access_units_) {
      if (au.timestamp() != first_timestamp &&
          au.timestamp() < second_timestamp) {
        second_timestamp = au.timestamp();
      }
    }
    SB_DCHECK_LT(first_timestamp, second_timestamp);
    int64_t frame_duration = second_timestamp - first_timestamp;
    dmp_info_.video_fps = 1'000'000LL / frame_duration;

    int64_t last_frame_timestamp = video_access_units_.back().timestamp();
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
  if (!reverse_byte_order_.has_value()) {
    uint32_t dmp_writer_version = 0;
    ParseHeader(&dmp_writer_version);
    SB_DCHECK_EQ(dmp_writer_version, kSupportedWriterVersion);
  }

  if (type == kSbMediaTypeAudio) {
    while (index >= audio_access_units_.size() && ParseOneRecord()) {
    }
    SB_CHECK_LT(index, audio_access_units_.size());
  } else {
    SB_DCHECK_EQ(type, kSbMediaTypeVideo);
    while (index >= video_access_units_.size() && ParseOneRecord()) {
    }
    SB_CHECK_LT(index, video_access_units_.size());
  }
}

VideoDmpReader::AudioAccessUnit VideoDmpReader::ReadAudioAccessUnit() {
  int64_t timestamp;
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

  AudioSampleInfo audio_sample_info;
  Read(read_cb_, reverse_byte_order_.value(), &audio_sample_info);

  return AudioAccessUnit(timestamp,
                         drm_sample_info_present ? &drm_sample_info : NULL,
                         std::move(data), std::move(audio_sample_info));
}

VideoDmpReader::VideoAccessUnit VideoDmpReader::ReadVideoAccessUnit() {
  int64_t timestamp;
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

  VideoSampleInfo video_sample_info;
  Read(read_cb_, reverse_byte_order_.value(), &video_sample_info);

  return VideoAccessUnit(timestamp,
                         drm_sample_info_present ? &drm_sample_info : NULL,
                         std::move(data), std::move(video_sample_info));
}

// static
VideoDmpReader::Registry* VideoDmpReader::GetRegistry() {
  static Registry s_registry;
  return &s_registry;
}

}  // namespace starboard
