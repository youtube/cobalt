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

#include "starboard/android/shared/configurate_seek.h"

#include <pthread.h>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/thread.h"

namespace starboard::android::shared {

namespace {

pthread_once_t s_once_flag_for_seek = PTHREAD_ONCE_INIT;
pthread_key_t s_thread_local_key_for_flush_decoder_during_reset = 0;
pthread_key_t s_thread_local_key_for_reset_audio_decoder = 0;

}  // namespace

void InitThreadLocalKeyForSeek() {
  int res = pthread_key_create(
      &s_thread_local_key_for_flush_decoder_during_reset, nullptr);
  SB_DCHECK_EQ(res, 0);
  res =
      pthread_key_create(&s_thread_local_key_for_reset_audio_decoder, nullptr);
  SB_DCHECK_EQ(res, 0);
}

void EnsureThreadLocalKeyInitedForSeek() {
  pthread_once(&s_once_flag_for_seek, InitThreadLocalKeyForSeek);
}

bool GetForceFlushDecoderDuringResetForCurrentThread() {
  EnsureThreadLocalKeyInitedForSeek();
  return pthread_getspecific(
             s_thread_local_key_for_flush_decoder_during_reset) != nullptr;
}

void SetForceFlushDecoderDuringResetForCurrentThread(
    bool flush_decoder_during_reset) {
  EnsureThreadLocalKeyInitedForSeek();
  pthread_setspecific(s_thread_local_key_for_flush_decoder_during_reset,
                      reinterpret_cast<void*>(
                          static_cast<uintptr_t>(flush_decoder_during_reset)));
}

bool GetForceResetAudioDecoderForCurrentThread() {
  EnsureThreadLocalKeyInitedForSeek();
  return pthread_getspecific(s_thread_local_key_for_reset_audio_decoder) !=
         nullptr;
}

void SetForceResetAudioDecoderForCurrentThread(bool reset_audio_decoder) {
  EnsureThreadLocalKeyInitedForSeek();
  pthread_setspecific(
      s_thread_local_key_for_reset_audio_decoder,
      reinterpret_cast<void*>(static_cast<uintptr_t>(reset_audio_decoder)));
}

}  // namespace starboard::android::shared
