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

#include "starboard/shared/starboard/player/video_dmp_writer.h"

#include <map>
#include <sstream>
#include <string>

#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/string.h"
#include "starboard/once.h"
#include "starboard/shared/starboard/application.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace video_dmp {

namespace {

using std::placeholders::_1;
using std::placeholders::_2;

class PlayerToWriterMap {
 public:
  PlayerToWriterMap()
      : dump_video_data_(Application::Get()->GetCommandLine()->HasSwitch(
            "dump_video_data")) {}
  bool dump_video_data() const { return dump_video_data_; }
  void Register(SbPlayer player) {
    ScopedLock scoped_lock(mutex_);
    SB_DCHECK(map_.find(player) == map_.end());
    map_[player] = new VideoDmpWriter;
  }
  void Unregister(SbPlayer player) {
    ScopedLock scoped_lock(mutex_);
    auto iter = map_.find(player);
    SB_DCHECK(iter != map_.end());
    delete iter->second;
    map_.erase(iter);
  }
  VideoDmpWriter* Get(SbPlayer player) {
    ScopedLock scoped_lock(mutex_);
    auto iter = map_.find(player);
    SB_DCHECK(iter != map_.end());
    return iter->second;
  }

 private:
  Mutex mutex_;
  bool dump_video_data_;
  std::map<SbPlayer, VideoDmpWriter*> map_;
};

SB_ONCE_INITIALIZE_FUNCTION(PlayerToWriterMap, GetOrCreatePlayerToWriterMap);

}  // namespace

VideoDmpWriter::VideoDmpWriter() : file_(kSbFileInvalid) {
  int index = 0;
  std::string file_name;
  while (!SbFileIsValid(file_)) {
    std::stringstream ss;
    ss << "video_" << index << ".dmp";
    file_name = ss.str();

    bool created = false;
    file_ = SbFileOpen(file_name.c_str(), kSbFileCreateOnly | kSbFileWrite,
                       &created, NULL);
    ++index;
  }
  SB_LOG(INFO) << "Dump video content to " << file_name;

  write_cb_ = std::bind(&VideoDmpWriter::WriteToFile, this, _1, _2);

  Write(write_cb_, kByteOrderMark);
  Write(write_cb_, kSupportedWriterVersion);
}

VideoDmpWriter::~VideoDmpWriter() {
  SbFileClose(file_);
}

// static
void VideoDmpWriter::OnPlayerCreate(
    SbPlayer player,
    SbMediaAudioCodec audio_codec,
    SbMediaVideoCodec video_codec,
    SbDrmSystem drm_system,
    const SbMediaAudioSampleInfo* audio_sample_info) {
  // TODO: Allow dump of drm initialization data

  PlayerToWriterMap* map = GetOrCreatePlayerToWriterMap();
  if (!map->dump_video_data()) {
    return;
  }
  map->Register(player);
  VideoDmpWriter* dmp_writer = map->Get(player);
  dmp_writer->DumpConfigs(video_codec, audio_codec, audio_sample_info);
}

// static
void VideoDmpWriter::OnPlayerWriteSample(
    SbPlayer player,
    const scoped_refptr<InputBuffer>& input_buffer) {
  PlayerToWriterMap* map = GetOrCreatePlayerToWriterMap();
  if (!map->dump_video_data()) {
    return;
  }
  map->Get(player)->DumpAccessUnit(input_buffer);
}

// static
void VideoDmpWriter::OnPlayerDestroy(SbPlayer player) {
  PlayerToWriterMap* map = GetOrCreatePlayerToWriterMap();
  if (!map->dump_video_data()) {
    return;
  }
  map->Unregister(player);
}

void VideoDmpWriter::DumpConfigs(
    SbMediaVideoCodec video_codec,
    SbMediaAudioCodec audio_codec,
    const SbMediaAudioSampleInfo* audio_sample_info) {
  Write(write_cb_, kRecordTypeAudioConfig);
  Write(write_cb_, audio_codec);
  if (audio_codec != kSbMediaAudioCodecNone) {
    SB_DCHECK(audio_sample_info);
    Write(write_cb_, audio_codec, *audio_sample_info);
  }

  Write(write_cb_, kRecordTypeVideoConfig);
  Write(write_cb_, video_codec);
}

void VideoDmpWriter::DumpAccessUnit(
    const scoped_refptr<InputBuffer>& input_buffer) {
  const SbMediaType& sample_type = input_buffer->sample_type();
  const void* sample_buffer = static_cast<const void*>(input_buffer->data());
  const int& sample_buffer_size = input_buffer->size();
  const SbTime& sample_timestamp = input_buffer->timestamp();
  const SbDrmSampleInfo* drm_sample_info = input_buffer->drm_info();

  if (sample_type == kSbMediaTypeAudio) {
    Write(write_cb_, kRecordTypeAudioAccessUnit);
  } else {
    SB_DCHECK(sample_type == kSbMediaTypeVideo);
    Write(write_cb_, kRecordTypeVideoAccessUnit);
  }

  Write(write_cb_, sample_timestamp);

  if (drm_sample_info && drm_sample_info->identifier_size == 16 &&
      (drm_sample_info->initialization_vector_size == 8 ||
       drm_sample_info->initialization_vector_size == 16)) {
    Write(write_cb_, true);
    Write(write_cb_, *drm_sample_info);
  } else {
    Write(write_cb_, false);
  }
  Write(write_cb_, static_cast<uint32_t>(sample_buffer_size));
  Write(write_cb_, sample_buffer, static_cast<size_t>(sample_buffer_size));

  if (sample_type == kSbMediaTypeAudio) {
    const SbMediaAudioSampleInfo& audio_sample_info =
        input_buffer->audio_sample_info();
    Write(write_cb_, audio_sample_info.codec, audio_sample_info);
  } else {
    SB_DCHECK(sample_type == kSbMediaTypeVideo);
    const SbMediaVideoSampleInfo& video_sample_info =
        input_buffer->video_sample_info();
    Write(write_cb_, video_sample_info.codec, video_sample_info);
  }
}

int VideoDmpWriter::WriteToFile(const void* buffer, int size) {
  return SbFileWrite(file_, static_cast<const char*>(buffer), size);
}

}  // namespace video_dmp
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
