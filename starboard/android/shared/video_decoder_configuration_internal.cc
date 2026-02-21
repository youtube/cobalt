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

VideoDecoderExperimentalFeatures& GetOrCreateExperimentalFeatures() {
  EnsureThreadLocalKeyInited();
  void* ptr = pthread_getspecific(g_experimental_features_key);
  if (ptr) {
    return *static_cast<VideoDecoderExperimentalFeatures*>(ptr);
  }
  VideoDecoderExperimentalFeatures* features =
      new VideoDecoderExperimentalFeatures();
  pthread_setspecific(g_experimental_features_key, features);
  return *features;
}

// Helper to convert C-style pointers back to std::optional after receiving them
// from the Starboard extension API boundary.
std::optional<int> ToOptional(const int* ptr) {
  if (ptr) {
    return *ptr;
  }
  return std::nullopt;
}

}  // namespace

VideoDecoderExperimentalFeatures GetExperimentalFeaturesForCurrentThread() {
  EnsureThreadLocalKeyInited();
  void* ptr = pthread_getspecific(g_experimental_features_key);
  SB_CHECK(ptr) << __func__
                << ": No experimental features set for this thread. The "
                   "setter was either not called yet (e.g., due to a race "
                   "condition) or this function is being called on the "
                   "wrong thread.";
  return *static_cast<VideoDecoderExperimentalFeatures*>(ptr);
}

void SetExperimentalFeaturesForCurrentThread(
    const StarboardVideoDecoderExperimentalFeatures* experimental_features) {
  // Ensure the thread-local object is created to signal that the setter has
  // been called on this thread. This satisfies the initialization check in the
  // getter, even if experimental_features is null and no specific values are
  // being set.
  VideoDecoderExperimentalFeatures& features =
      GetOrCreateExperimentalFeatures();

  if (!experimental_features) {
    return;
  }

  features.initial_max_frames_in_decoder =
      ToOptional(experimental_features->initial_max_frames_in_decoder);
  features.max_pending_input_frames =
      ToOptional(experimental_features->max_pending_input_frames);
  features.video_decoder_poll_interval_ms =
      ToOptional(experimental_features->video_decoder_poll_interval_ms);
}

}  // namespace starboard::android::shared
