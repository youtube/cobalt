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

#include "starboard/elf_loader/evergreen_info.h"

#include <atomic>
#include <string.h>

static EvergreenInfo g_evergreen_info;
static bool g_valid_info = false;
static std::atomic<int32_t> g_busy{0};

bool SetEvergreenInfo(const EvergreenInfo* evergreen_info) {
  // Set the busy flag or bail.
  int32_t busy_state_flag = 1;
  int32_t ready_state_flag = 0;
  if (g_busy.compare_exchange_weak(ready_state_flag, busy_state_flag,
      std::memory_order_release, std::memory_order_relaxed)) {
    // Bailing out is OK as the process crashed
    // before we launched the application and in that
    // case the evergreen information is not needed.
    return false;
  }
  if (evergreen_info != NULL && evergreen_info->base_address != 0 &&
      strlen(evergreen_info->file_path_buf) != 0) {
    g_evergreen_info = *evergreen_info;
    g_valid_info = true;
  } else {
    g_valid_info = false;
  }
  // Publish local memory changes to all threads.
  std::atomic_thread_fence(std::memory_order_seq_cst);

  // Clear the busy flag.
  g_busy.compare_exchange_weak(busy_state_flag, ready_state_flag,
      std::memory_order_release, std::memory_order_relaxed);

  return true;
}

bool GetEvergreenInfo(EvergreenInfo* evergreen_info) {
  if (evergreen_info == NULL) {
    return false;
  }

  int32_t busy_state_flag = 1;
  int32_t ready_state_flag = 0;

  // Set the busy flag or bail.
  if (g_busy.compare_exchange_weak(ready_state_flag, busy_state_flag,
      std::memory_order_release, std::memory_order_relaxed)) {
    return false;
  }

  // Make sure all memory changes are visible to the current thread.
  std::atomic_thread_fence(std::memory_order_seq_cst);
  if (!g_valid_info) {
    return false;
  }

  *evergreen_info = g_evergreen_info;

  // Clear the busy flag.
  g_busy.compare_exchange_weak(busy_state_flag, ready_state_flag,
    std::memory_order_release, std::memory_order_relaxed);
  return true;
}
