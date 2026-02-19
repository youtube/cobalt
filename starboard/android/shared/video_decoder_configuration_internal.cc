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

pthread_once_t g_once_control = PTHREAD_ONCE_INIT;
pthread_key_t g_experimental_features_key = 0;

void FreeExperimentalFeatures(void* ptr) {
  delete static_cast<VideoDecoderExperimentalFeatures*>(ptr);
}

void InitializeKey() {
  int res = pthread_key_create(&g_experimental_features_key,
                               FreeExperimentalFeatures);
  SB_CHECK_EQ(res, 0);
}

void EnsureThreadLocalKeyInited() {
  pthread_once(&g_once_control, InitializeKey);
}

}  // namespace

VideoDecoderExperimentalFeatures GetExperimentalFeaturesForCurrentThread() {
  EnsureThreadLocalKeyInited();
  void* ptr = pthread_getspecific(g_experimental_features_key);
  if (ptr) {
    return *static_cast<VideoDecoderExperimentalFeatures*>(ptr);
  }
  return {};
}

void SetExperimentalFeaturesForCurrentThread(
    const StarboardVideoDecoderExperimentalFeatures* experimental_features) {
  if (!experimental_features) {
    return;
  }
  EnsureThreadLocalKeyInited();

  void* ptr = pthread_getspecific(g_experimental_features_key);
  VideoDecoderExperimentalFeatures* features;
  if (ptr) {
    features = static_cast<VideoDecoderExperimentalFeatures*>(ptr);
  } else {
    features = new VideoDecoderExperimentalFeatures();
    pthread_setspecific(g_experimental_features_key, features);
  }

  if (experimental_features->initial_max_frames_in_decoder.is_set) {
    features->initial_max_frames_in_decoder =
        experimental_features->initial_max_frames_in_decoder.value;
  }

  if (experimental_features->max_pending_input_frames.is_set) {
    features->max_pending_input_frames =
        experimental_features->max_pending_input_frames.value;
  }

  if (experimental_features->video_decoder_poll_interval_ms.is_set) {
    features->video_decoder_poll_interval_ms =
        experimental_features->video_decoder_poll_interval_ms.value;
  }

  if (experimental_features->initial_preroll_count.is_set) {
    features->initial_preroll_count =
        experimental_features->initial_preroll_count.value;
  }
}

}  // namespace starboard::android::shared
