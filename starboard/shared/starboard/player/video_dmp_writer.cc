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

#include "starboard/shared/starboard/player/video_dmp_writer.h"

#include <map>
#include <sstream>
#include <string>

#include "starboard/log.h"
#include "starboard/mutex.h"
#include "starboard/once.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/string.h"

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
}

VideoDmpWriter::~VideoDmpWriter() {
  SbFileClose(file_);
}

// static
void VideoDmpWriter::OnPlayerCreate(SbPlayer player,
                                    SbMediaVideoCodec video_codec,
                                    SbMediaAudioCodec audio_codec,
                                    SbDrmSystem drm_system,
                                    const SbMediaAudioHeader* audio_header) {
  // TODO: Allow dump of drm initialization data
  SB_UNREFERENCED_PARAMETER(drm_system);

  PlayerToWriterMap* map = GetOrCreatePlayerToWriterMap();
  if (!map->dump_video_data()) {
    return;
  }
  map->Register(player);
  map->Get(player)->DumpConfigs(video_codec, audio_codec, audio_header);
}

// static
void VideoDmpWriter::OnPlayerWriteSample(
    SbPlayer player,
    SbMediaType sample_type,
    const void* const* sample_buffers,
    const int* sample_buffer_sizes,
    int number_of_sample_buffers,
    SbTime sample_timestamp,
    const SbMediaVideoSampleInfo* video_sample_info,
    const SbDrmSampleInfo* drm_sample_info) {
  PlayerToWriterMap* map = GetOrCreatePlayerToWriterMap();
  if (!map->dump_video_data()) {
    return;
  }
  map->Get(player)->DumpAccessUnit(sample_type, sample_buffers,
                                   sample_buffer_sizes,
                                   number_of_sample_buffers, sample_timestamp,
                                   video_sample_info, drm_sample_info);
}

// static
void VideoDmpWriter::OnPlayerDestroy(SbPlayer player) {
  PlayerToWriterMap* map = GetOrCreatePlayerToWriterMap();
  if (!map->dump_video_data()) {
    return;
  }
  map->Unregister(player);
}

void VideoDmpWriter::DumpConfigs(SbMediaVideoCodec video_codec,
                                 SbMediaAudioCodec audio_codec,
                                 const SbMediaAudioHeader* audio_header) {
  Write(write_cb_, kRecordTypeAudioConfig);
  Write(write_cb_, audio_codec);
  if (audio_codec != kSbMediaAudioCodecNone) {
    SB_DCHECK(audio_header);
    Write(write_cb_, *audio_header);
  }

  Write(write_cb_, kRecordTypeVideoConfig);
  Write(write_cb_, video_codec);
}

void VideoDmpWriter::DumpAccessUnit(
    SbMediaType sample_type,
    const void* const* sample_buffers,
    const int* sample_buffer_sizes,
    int number_of_sample_buffers,
    SbTime sample_timestamp,
    const SbMediaVideoSampleInfo* video_sample_info,
    const SbDrmSampleInfo* drm_sample_info) {
  SB_DCHECK(number_of_sample_buffers == 1);

  if (sample_type == kSbMediaTypeAudio) {
    Write(write_cb_, kRecordTypeAudioAccessUnit);
  } else if (sample_type == kSbMediaTypeVideo) {
    Write(write_cb_, kRecordTypeVideoAccessUnit);
  } else {
    SB_NOTREACHED() << sample_type;
  }

  // TODO: make this write SbTime.
  Write(write_cb_, SB_TIME_TO_SB_MEDIA_TIME(sample_timestamp));

  if (drm_sample_info && drm_sample_info->identifier_size == 16 &&
      (drm_sample_info->initialization_vector_size == 8 ||
       drm_sample_info->initialization_vector_size == 16)) {
    Write(write_cb_, true);
    Write(write_cb_, *drm_sample_info);
  } else {
    Write(write_cb_, false);
  }
  Write(write_cb_, static_cast<uint32_t>(sample_buffer_sizes[0]));
  Write(write_cb_, sample_buffers[0],
        static_cast<size_t>(sample_buffer_sizes[0]));

  if (sample_type == kSbMediaTypeVideo) {
    SB_DCHECK(video_sample_info);
    Write(write_cb_, *video_sample_info);
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
