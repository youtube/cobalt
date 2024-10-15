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

#include "starboard/loader_app/pending_restart.h"

#include <atomic>

namespace starboard {
namespace loader_app {

namespace {
std::atomic<int32_t> g_pending_restart{0};
}  // namespace

bool IsPendingRestart() {
  return g_pending_restart.load(std::memory_order_relaxed) == 1;
}

void SetPendingRestart(bool value) {
  g_pending_restart.store(value ? 1 : 0, std::memory_order_relaxed);
}

}  // namespace loader_app
}  // namespace starboard
