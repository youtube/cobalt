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

#ifndef STARBOARD_ANDROID_SHARED_MEDIA_AGENCY_H_
#define STARBOARD_ANDROID_SHARED_MEDIA_AGENCY_H_

#include <map>

#include "starboard/common/condition_variable.h"
#include "starboard/common/optional.h"
#include "starboard/common/ref_counted.h"
#include "starboard/android/shared/video_decoder.h"
namespace starboard {
namespace android {
namespace shared {

class MediaAgency {
 public:
  typedef std::function<void(int, double, int64_t, int64_t)> PlaybackStatusCB;

  void RegisterPlayerClient(int audio_session_id,
                            void* video_decoder,
                            const PlaybackStatusCB& playback_status_cb);
  int UnregisterPlayerClient(int audio_session_id);

  void UpdatePlayerClient(int audio_session_id,
                          void* audio_renderer_sink_android,
                          void* VideoRenderAlgorithmImpl);

  void SetPlaybackStatus(int audio_session_id,
                         SbTime seek_to_time,
                         SbTime frames_sent_to_sink_in_time,
                         double playback_rate);

  static MediaAgency* GetInstance() {
    starboard::ScopedLock lock(s_mutex_);
    if (sInstance == NULL) {
      sInstance = new MediaAgency;
    }
    return sInstance;
  }

  int GetAudioConfigByAudioSinkContext(void* audio_renderer_sink_android,
                                       SbTime* seek_time = NULL);
  int GetAudioSessionIdByVideoDecoder(void* video_decoder);
  int DisableTunnelMode(int audio_session_id, bool disable);
  bool IsTunnelModeEnabled(void* VideoRenderAlgorithmImpl);
  int GetDroppedFramesTunnel(void* VideoRenderAlgorithmImpl);
  void SetDroppedFramesTunnel(int audio_session_id, int dropped_frames);

 protected:
  MediaAgency(){};
  ~MediaAgency(){};

 private:
  struct ClientInfo {
    SbTime seek_to_time_;
    double playback_rate_;
    PlaybackStatusCB playback_status_cb_;
    void* audio_renderer_sink_android_;
    void* media_algorithm_;
    void* video_decoder_;
    bool disable_tunnel_;
    int dropped_frames_;
  };

  starboard::Mutex mutex_;
  std::map<int32_t, ClientInfo> client_map_;

  static starboard::Mutex s_mutex_;
  static MediaAgency* sInstance;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_AGENCY_H_
