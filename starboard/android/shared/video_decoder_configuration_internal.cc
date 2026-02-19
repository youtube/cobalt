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
pthread_key_t g_initial_max_frames_key = 0;
pthread_key_t g_max_pending_frames_key = 0;
pthread_key_t g_video_decoder_poll_interval_key = 0;

void WriteIntToThreadLocalStorage(pthread_key_t key, int value) {
  uintptr_t ptr_val = static_cast<uintptr_t>(value);
  // Sets sentinel value to differentiate 0 vs nullptr.
  ptr_val <<= 1;
  ptr_val |= 0x01;
  pthread_setspecific(key, reinterpret_cast<void*>(ptr_val));
}

std::optional<int> ReadIntFromThreadLocalStorage(pthread_key_t key) {
  void* ptr = pthread_getspecific(key);
  if (ptr == nullptr) {
    return std::nullopt;
  }
  uintptr_t ptr_val = reinterpret_cast<uintptr_t>(ptr);
  // Removes sentinel value.
  ptr_val >>= 1;
  return static_cast<int>(ptr_val);
}

}  // namespace

void InitializeKeys() {
  int res =
      pthread_key_create(&g_initial_max_frames_key, /*destructor=*/nullptr);
  SB_CHECK_EQ(res, 0);
  res = pthread_key_create(&g_max_pending_frames_key,
                           /*destructor=*/nullptr);
  SB_CHECK_EQ(res, 0);
  res = pthread_key_create(&g_video_decoder_poll_interval_key,
                           /*destructor=*/nullptr);
  SB_CHECK_EQ(res, 0);
}

void EnsureThreadLocalKeyInitedForDecoderConfig() {
  pthread_once(&g_once_control, InitializeKeys);
}

VideoDecoderExperimentalFeatures GetExperimentalFeaturesForCurrentThread() {
  EnsureThreadLocalKeyInitedForDecoderConfig();
  VideoDecoderExperimentalFeatures experimental_features;
  experimental_features.initial_max_frames_in_decoder =
      ReadIntFromThreadLocalStorage(g_initial_max_frames_key);
  experimental_features.max_pending_input_frames =
      ReadIntFromThreadLocalStorage(g_max_pending_frames_key);
  experimental_features.video_decoder_poll_interval_ms =
      ReadIntFromThreadLocalStorage(g_video_decoder_poll_interval_key);
  return experimental_features;
}

void SetExperimentalFeaturesForCurrentThread(
    const StarboardVideoDecoderExperimentalFeatures* experimental_features) {
  if (!experimental_features) {
    return;
  }
  EnsureThreadLocalKeyInitedForDecoderConfig();

  if (experimental_features->initial_max_frames_in_decoder.is_set) {
    int value = experimental_features->initial_max_frames_in_decoder.value;
    if (value < 0) {
      SB_LOG(WARNING) << "Invalid initial_max_frames_in_decoder: " << value;
    } else {
      WriteIntToThreadLocalStorage(g_initial_max_frames_key, value);
    }
  }

  if (experimental_features->max_pending_input_frames.is_set) {
    int value = experimental_features->max_pending_input_frames.value;
    if (value < 0) {
      SB_LOG(WARNING) << "Invalid max_pending_input_frames: " << value;
    } else {
      WriteIntToThreadLocalStorage(g_max_pending_frames_key, value);
    }
  }

  if (experimental_features->video_decoder_poll_interval_ms.is_set) {
    int value = experimental_features->video_decoder_poll_interval_ms.value;
    if (value <= 0) {
      SB_LOG(WARNING) << "Invalid video_decoder_poll_interval_ms: " << value;
    } else {
      WriteIntToThreadLocalStorage(g_video_decoder_poll_interval_key, value);
    }
  }
}

}  // namespace starboard::android::shared
