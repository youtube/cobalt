// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/media_agency.h"
#include "sys/system_properties.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"

namespace starboard {
namespace android {
namespace shared {

MediaAgency* MediaAgency::sInstance = NULL;
starboard::Mutex MediaAgency::s_mutex_;

void MediaAgency::RegisterPlayerClient(
    int tunneling_audio_session_id,
    void* video_decoder,
    const PlaybackStatusCB& playback_status_cb) {
  starboard::ScopedLock lock(mutex_);
  ClientInfo info = {
      -1,  // seek_to_time_
      playback_status_cb,
      NULL,  // audio_renderer_sink_android_
      video_decoder,
  };

  client_map_[tunneling_audio_session_id] = info;
}

int MediaAgency::UnregisterPlayerClient(int tunneling_audio_session_id) {
  starboard::ScopedLock lock(mutex_);
  auto iter = client_map_.find(tunneling_audio_session_id);
  if (iter != client_map_.end()) {
    client_map_.erase(iter);
    return 0;
  }
  return -1;
}

void MediaAgency::UpdatePlayerClient(int tunneling_audio_session_id,
                                     void* audio_renderer_sink_android) {
  starboard::ScopedLock lock(mutex_);
  // FIXME: check if tunneling_audio_session_id exist
  client_map_[tunneling_audio_session_id].audio_renderer_sink_android_ =
      audio_renderer_sink_android;
}

int MediaAgency::GetAudioConfigByAudioSinkContext(
    void* audio_renderer_sink_android,
    SbTime* seek_time) {
  starboard::ScopedLock lock(mutex_);
  for (std::pair<int32_t, ClientInfo> element : client_map_) {
    if (element.second.audio_renderer_sink_android_ ==
        audio_renderer_sink_android) {
      if (seek_time) {
        *seek_time = element.second.seek_to_time_;
      }
      return element.first;
    }
  }
  return -1;
}

void MediaAgency::SetPlaybackStatus(int tunneling_audio_session_id,
                                    SbTime seek_to_time) {
  starboard::ScopedLock lock(mutex_);
  auto iter = client_map_.find(tunneling_audio_session_id);
  if (iter == client_map_.end()) {
    return;
  }

  int action = kSbPlaybackStatusUpdateActionNone;
  if (iter->second.seek_to_time_ != seek_to_time) {
    action |= kSbPlaybackStatusUpdateSeekTime;
  }

  // TODO:handling pending action to set playback rate to video pipeline
  if (iter->second.playback_status_cb_ != nullptr &&
      action != kSbPlaybackStatusUpdateActionNone) {
    iter->second.playback_status_cb_(action, seek_to_time);
  }

  iter->second.seek_to_time_ = seek_to_time;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
