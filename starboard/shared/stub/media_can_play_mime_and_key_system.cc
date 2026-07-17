// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <atomic>

#include "starboard/media.h"

namespace {
std::atomic<SbMediaCanPlayMimeAndKeySystemFunc> g_can_play_func_for_testing{
    nullptr};
}  // namespace

void SbMediaSetCanPlayMimeAndKeySystemFuncForTesting(
    SbMediaCanPlayMimeAndKeySystemFunc func) {
  g_can_play_func_for_testing.store(func, std::memory_order_release);
}

SbMediaSupportType SbMediaCanPlayMimeAndKeySystem(const char* mime,
                                                  const char* key_system) {
  auto test_func = g_can_play_func_for_testing.load(std::memory_order_acquire);
  if (test_func) {
    return test_func(mime, key_system);
  }
  return kSbMediaSupportTypeNotSupported;
}
