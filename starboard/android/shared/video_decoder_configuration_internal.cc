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

std::optional<int> GetVideoInitialMaxFramesInDecoderForCurrentThread() {
  EnsureThreadLocalKeyInitedForDecoderConfig();
  return ReadIntFromThreadLocalStorage(g_initial_max_frames_key);
}

void SetVideoInitialMaxFramesInDecoderForCurrentThread(
    int initial_max_frames_in_decoder) {
  if (initial_max_frames_in_decoder < 0) {
    SB_LOG(WARNING) << "Invalid initial_max_frames_in_decoder: "
                    << initial_max_frames_in_decoder;
    return;
  }
  EnsureThreadLocalKeyInitedForDecoderConfig();
  WriteIntToThreadLocalStorage(g_initial_max_frames_key,
                               initial_max_frames_in_decoder);
}

std::optional<int> GetVideoMaxPendingInputFramesForCurrentThread() {
  EnsureThreadLocalKeyInitedForDecoderConfig();
  return ReadIntFromThreadLocalStorage(g_max_pending_frames_key);
}

void SetVideoMaxPendingInputFramesForCurrentThread(
    int max_pending_input_frames) {
  if (max_pending_input_frames < 0) {
    SB_LOG(WARNING) << "Invalid max_pending_input_frames: "
                    << max_pending_input_frames;
    return;
  }
  EnsureThreadLocalKeyInitedForDecoderConfig();
  WriteIntToThreadLocalStorage(g_max_pending_frames_key,
                               max_pending_input_frames);
}

std::optional<int> GetVideoDecoderPollIntervalMsForCurrentThread() {
  EnsureThreadLocalKeyInitedForDecoderConfig();
  return ReadIntFromThreadLocalStorage(g_video_decoder_poll_interval_key);
}

void SetVideoDecoderPollIntervalMsForCurrentThread(
    int video_decoder_poll_interval_ms) {
  if (video_decoder_poll_interval_ms <= 0) {
    SB_LOG(WARNING) << "Invalid video_decoder_poll_interval_ms: "
                    << video_decoder_poll_interval_ms;
    return;
  }
  EnsureThreadLocalKeyInitedForDecoderConfig();
  WriteIntToThreadLocalStorage(g_video_decoder_poll_interval_key,
                               video_decoder_poll_interval_ms);
}

}  // namespace starboard::android::shared
