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

#include "starboard/android/shared/player_decoder_configuration_internal.h"

#include <pthread.h>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/thread.h"

namespace starboard::android::shared {

namespace {

pthread_once_t s_once_flag_for_decoder_config = PTHREAD_ONCE_INIT;
pthread_key_t s_thread_local_key_for_initial_max_frames_in_decoder = 0;
pthread_key_t s_thread_local_key_for_max_pending_input_frames = 0;

}  // namespace

void InitThreadLocalKeyForDecoderConfig() {
  int res = pthread_key_create(
      &s_thread_local_key_for_initial_max_frames_in_decoder, nullptr);
  SB_DCHECK_EQ(res, 0);
  res = pthread_key_create(&s_thread_local_key_for_max_pending_input_frames,
                           nullptr);
  SB_DCHECK_EQ(res, 0);
}

void EnsureThreadLocalKeyInitedForDecoderConfig() {
  pthread_once(&s_once_flag_for_decoder_config,
               InitThreadLocalKeyForDecoderConfig);
}

std::optional<int> GetVideoInitialMaxFramesInDecoderForCurrentThread() {
  EnsureThreadLocalKeyInitedForDecoderConfig();
  void* ptr =
      pthread_getspecific(s_thread_local_key_for_initial_max_frames_in_decoder);
  if (ptr == nullptr) {
    return std::nullopt;
  }
  // Subtract 1 to retrieve the original value, complementing `Set...`.
  return static_cast<int>(reinterpret_cast<uintptr_t>(ptr) - 1);
}

void SetVideoInitialMaxFramesInDecoderForCurrentThread(
    int initial_max_frames_in_decoder) {
  if (initial_max_frames_in_decoder < 0) {
    SB_LOG(WARNING) << "Invalid initial_max_frames_in_decoder: "
                    << initial_max_frames_in_decoder;
    return;
  }
  EnsureThreadLocalKeyInitedForDecoderConfig();
  // Add 1 to the value to distinguish 0 from a nullptr (not set).
  pthread_setspecific(s_thread_local_key_for_initial_max_frames_in_decoder,
                      reinterpret_cast<void*>(static_cast<uintptr_t>(
                          initial_max_frames_in_decoder + 1)));
}

std::optional<int> GetVideoMaxPendingInputFramesForCurrentThread() {
  EnsureThreadLocalKeyInitedForDecoderConfig();
  void* ptr =
      pthread_getspecific(s_thread_local_key_for_max_pending_input_frames);
  if (ptr == nullptr) {
    return std::nullopt;
  }
  // Subtract 1 to retrieve the original value, complementing `Set...`.
  return static_cast<int>(reinterpret_cast<uintptr_t>(ptr) - 1);
}

void SetVideoMaxPendingInputFramesForCurrentThread(
    int max_pending_input_frames) {
  if (max_pending_input_frames < 0) {
    SB_LOG(WARNING) << "Invalid max_pending_input_frames: "
                    << max_pending_input_frames;
    return;
  }
  EnsureThreadLocalKeyInitedForDecoderConfig();
  // Add 1 to the value to distinguish 0 from a nullptr (not set).
  pthread_setspecific(s_thread_local_key_for_max_pending_input_frames,
                      reinterpret_cast<void*>(static_cast<uintptr_t>(
                          max_pending_input_frames + 1)));
}

}  // namespace starboard::android::shared
