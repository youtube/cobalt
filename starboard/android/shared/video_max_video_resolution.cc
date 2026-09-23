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

#include "starboard/android/shared/video_max_video_resolution.h"

#include <mutex>
#include <utility>

namespace starboard {
namespace {

thread_local std::string g_thread_max_video_resolution;

std::mutex g_player_max_video_resolution_mutex;
std::string g_player_max_video_resolution;

}  // namespace

std::string GetMaxVideoResolutionForPlayer() {
  std::lock_guard<std::mutex> lock(g_player_max_video_resolution_mutex);
  return g_player_max_video_resolution;
}

void TransferMaxVideoResolutionForCurrentThreadToPlayer() {
  std::lock_guard<std::mutex> lock(g_player_max_video_resolution_mutex);
  g_player_max_video_resolution = std::move(g_thread_max_video_resolution);
  g_thread_max_video_resolution.clear();
}

void SetMaxVideoResolutionForCurrentThread(const char* max_video_resolution) {
  if (max_video_resolution) {
    g_thread_max_video_resolution = max_video_resolution;
  } else {
    g_thread_max_video_resolution.clear();
  }
}

}  // namespace starboard
