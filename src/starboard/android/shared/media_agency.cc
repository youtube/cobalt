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
    int audio_session_id,
    void* video_decoder,
    const PlaybackStatusCB& playback_status_cb) {
  starboard::ScopedLock lock(mutex_);
  ClientInfo info = {
      -1,  // seek_to_time_
      -1,  // playback_rate_
      playback_status_cb,
      NULL,  // audio_renderer_sink_android_
      NULL,  // media_algorithm_
      video_decoder,
      false,  // disable_tunnel_
      0       // dropped_frames_
  };

  client_map_[audio_session_id] = info;
}

int MediaAgency::UnregisterPlayerClient(int audio_session_id) {
  starboard::ScopedLock lock(mutex_);
  auto iter = client_map_.find(audio_session_id);
  if (iter != client_map_.end()) {
    client_map_.erase(iter);
    return 0;
  }
  return -1;
}

void MediaAgency::UpdatePlayerClient(int audio_session_id,
                                     void* audio_renderer_sink_android,
                                     void* VideoRenderAlgorithmImpl) {
  starboard::ScopedLock lock(mutex_);
  // FIXME: check if audio_session_id exist
  client_map_[audio_session_id].audio_renderer_sink_android_ =
      audio_renderer_sink_android;
  client_map_[audio_session_id].media_algorithm_ = VideoRenderAlgorithmImpl;
}

bool MediaAgency::IsTunnelModeEnabled(void* VideoRenderAlgorithmImpl) {
  starboard::ScopedLock lock(mutex_);
  for (std::pair<int32_t, ClientInfo> element : client_map_) {
    if (element.second.media_algorithm_ == VideoRenderAlgorithmImpl) {
      return !element.second.disable_tunnel_ && element.first != -1;
    }
  }
  return false;
}

int MediaAgency::GetDroppedFramesTunnel(void* VideoRenderAlgorithmImpl) {
  starboard::ScopedLock lock(mutex_);
  for (std::pair<int32_t, ClientInfo> element : client_map_) {
    if (element.second.media_algorithm_ == VideoRenderAlgorithmImpl) {
      if (!element.second.disable_tunnel_ && element.first != -1) {
        return element.second.dropped_frames_;
      }
    }
  }
  return 0;
}

void MediaAgency::SetDroppedFramesTunnel(int audio_session_id,
                                         int dropped_frames) {
  starboard::ScopedLock lock(mutex_);
  auto iter = client_map_.find(audio_session_id);
  if (iter != client_map_.end()) {
    iter->second.dropped_frames_ = dropped_frames;
  }
}

int MediaAgency::DisableTunnelMode(int audio_session_id, bool disable) {
  starboard::ScopedLock lock(mutex_);
  auto iter = client_map_.find(audio_session_id);
  if (iter != client_map_.end()) {
    iter->second.disable_tunnel_ = disable;
    int action = kSbPlaybackStatusCapabilityChange;

    if (iter->second.playback_status_cb_ != nullptr &&
        action != kSbPlaybackStatusUpdateActionNone) {
      iter->second.playback_status_cb_(action, 0, 0, 0);
    }
    return 0;
  }
  return -1;
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
      if (element.second.disable_tunnel_) {
        return -1;
      }
      return element.first;
    }
  }
  return -1;
}

int MediaAgency::GetAudioSessionIdByVideoDecoder(void* video_decoder) {
  starboard::ScopedLock lock(mutex_);
  for (std::pair<int32_t, ClientInfo> element : client_map_) {
    if (element.second.video_decoder_ == video_decoder) {
      if (element.second.disable_tunnel_) {
        return -1;
      }
      return element.first;
    }
  }
  return -1;
}

void MediaAgency::SetPlaybackStatus(int audio_session_id,
                                    SbTime seek_to_time,
                                    SbTime frames_sent_to_sink_in_time,
                                    double playback_rate) {
  starboard::ScopedLock lock(mutex_);
  auto iter = client_map_.find(audio_session_id);
  if (iter == client_map_.end()) {
    return;
  }

  SbTime playTime = seek_to_time + frames_sent_to_sink_in_time;
  int action = kSbPlaybackStatusUpdateActionNone;
  if (iter->second.playback_rate_ != playback_rate) {
    action |= kSbPlaybackStatusUpdatePlaybackRate;
  }

  if (iter->second.seek_to_time_ != seek_to_time) {
    action |= kSbPlaybackStatusUpdateSeekTime;
  }

  // TODO:handling pending action to set playback rate to video pipeline
  if (iter->second.playback_status_cb_ != nullptr &&
      action != kSbPlaybackStatusUpdateActionNone) {
    iter->second.playback_status_cb_(action, playback_rate, playTime,
                                     seek_to_time);
  }

  iter->second.seek_to_time_ = seek_to_time;
  iter->second.playback_rate_ = playback_rate;

  /* There are three approaches for tunnel mode playback rate
   Method 1: Adjust audio sync time by playback rate
          a, Adjust the audio stamp of sync header by playback rate,
             audio stamp = written_frames_in_time * playback rate
          b, Android HAL handle the audio timestamp specially since the
             timestamp is not in proportion to audio frames written.
             Note: Currently many chipset OEM only support 1x playback
             rate for tunnel mode. If audio timestamp does not match with
             written audio frames, it would report incorrect audio timestamp.
             Hence it causes A/V sync problem
          c, Maintain a table consist of audio timestamp and written frames,
             it is similar as AudioFrameTracker. We calculate the real played
             audio time stamp by interpolate the table with the consumed frames.
             Note: there is problem that we can't the elapsed audio timestamp
             like GetFutureFramesPlayedAdjustedToPlaybackRate(). Maybe we can
             send the playback rate to low level. Anyway it would be complicated
             since video pipeline and audio pipeline is different process

   Method 2: Adjust video timestamp by playback rate
          a, Modify the video input buffer timestamp when there is playback rate
             changed.
             Note: there is limitation because video and audio pipeline run in
             different thread. Maybe video input buffer is prior to playback
             rate changed so that it has no chance to modify timestamp
             we can modify output buffer timestamp. However tunnel mode do not
             notify output buffer to cobalt.

   Method 3: In Android Video HAL to maintain AudioFrameTracker. Currently patch
             use it
          a, send the playback rate and corresponding audio written frames to
             video HAL
          b, When do A/V sync, we adjust audio timestamp by this
             AudioFrameTracker that has playback rate information.

  */
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
