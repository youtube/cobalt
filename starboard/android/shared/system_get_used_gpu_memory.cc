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

#include <optional>

#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/system.h"
#include "third_party/jni_zero/jni_zero.h"

// Reports the "summary.graphics" value of ActivityManager's memory info for
// this process, read through CobaltMemoryInfoBridge as Android TV does in
// cobalt/browser/performance/performance_impl.cc. ActivityManager throttles
// getProcessMemoryInfo(), so calls made in quick succession can return the
// same value.
int64_t SbSystemGetUsedGPUMemory() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  std::optional<int> graphics_kb =
      starboard::StarboardBridge::GetInstance()->GetUsedGraphicsMemoryKb(env);
  if (!graphics_kb) {
    return 0;
  }
  return static_cast<int64_t>(*graphics_kb) * 1024;
}
