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

#ifndef STARBOARD_ANDROID_SHARED_MEDIA_CODEC_MEMORY_METRICS_H_
#define STARBOARD_ANDROID_SHARED_MEDIA_CODEC_MEMORY_METRICS_H_

#include <stdint.h>

namespace starboard {

// Returns the global memory usage of all MediaCodec output buffers.
int64_t GetGlobalOutputMemoryUsage();

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_CODEC_MEMORY_METRICS_H_
