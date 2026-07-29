// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/video_max_video_input_size.h"

#include <algorithm>

#include "starboard/common/check_op.h"

namespace starboard {
namespace {

thread_local int g_max_video_input_size = 0;

}  // namespace

int GetMaxVideoInputSizeForCurrentThread() {
  return g_max_video_input_size;
}

void SetMaxVideoInputSizeForCurrentThread(int max_video_input_size) {
  SB_DCHECK_GE(max_video_input_size, 0);
  g_max_video_input_size = std::max(0, max_video_input_size);
}

}  // namespace starboard
