// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/video_decoder_configuration_internal.h"

#include <pthread.h>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/thread.h"

namespace starboard::android::shared {

namespace {

struct VideoDecoderConfig {
  std::optional<int> initial_max_frames;
  std::optional<int> max_pending_input_frames;
  std::optional<int> initial_preroll_count;
  std::optional<int> poll_interval;
  std::optional<int> media_codec_reset_delay;
};

pthread_once_t g_once_control = PTHREAD_ONCE_INIT;
pthread_key_t g_video_decoder_config_key = 0;

void DestroyVideoDecoderConfig(void* ptr) {
  delete static_cast<VideoDecoderConfig*>(ptr);
}

void InitializeKeys() {
  int res = pthread_key_create(&g_video_decoder_config_key,
                               DestroyVideoDecoderConfig);
  SB_CHECK_EQ(res, 0);
}

void EnsureThreadLocalKeyInitedForDecoderConfig() {
  pthread_once(&g_once_control, InitializeKeys);
}

VideoDecoderConfig* GetOrCreateConfig() {
  EnsureThreadLocalKeyInitedForDecoderConfig();
  void* ptr = pthread_getspecific(g_video_decoder_config_key);
  if (ptr == nullptr) {
    ptr = new VideoDecoderConfig();
    pthread_setspecific(g_video_decoder_config_key, ptr);
  }
  return static_cast<VideoDecoderConfig*>(ptr);
}

}  // namespace

#define DEFINE_VIDEO_DECODER_CONFIG_ACCESSORS(name, member, validator) \
  std::optional<int> Get##name##ForCurrentThread() {                   \
    return GetOrCreateConfig()->member;                                \
  }                                                                    \
  void Set##name##ForCurrentThread(int value) {                        \
    if (!(validator(value))) {                                         \
      SB_LOG(WARNING) << "Invalid " #member ": " << value;             \
      return;                                                          \
    }                                                                  \
    GetOrCreateConfig()->member = value;                               \
  }

#define DEFINE_NON_NEGATIVE_ACCESSORS(name, member)   \
  DEFINE_VIDEO_DECODER_CONFIG_ACCESSORS(name, member, \
                                        [](int v) { return v >= 0; })

#define DEFINE_POSITIVE_ACCESSORS(name, member)       \
  DEFINE_VIDEO_DECODER_CONFIG_ACCESSORS(name, member, \
                                        [](int v) { return v > 0; })

DEFINE_NON_NEGATIVE_ACCESSORS(VideoInitialMaxFramesInDecoder,
                              initial_max_frames)
DEFINE_NON_NEGATIVE_ACCESSORS(VideoMaxPendingInputFrames,
                              max_pending_input_frames)
DEFINE_NON_NEGATIVE_ACCESSORS(VideoDecoderInitialPrerollCount,
                              initial_preroll_count)
DEFINE_POSITIVE_ACCESSORS(VideoDecoderPollIntervalMs, poll_interval)
DEFINE_NON_NEGATIVE_ACCESSORS(MediaCodecResetDelayMs, media_codec_reset_delay)

}  // namespace starboard::android::shared
