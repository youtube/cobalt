//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
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

#include "starboard/system.h"

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"

#include <cerrno>

int64_t SbSystemGetUsedGPUMemory() {
  starboard::ScopedFile status_file(
    "/sys/fs/cgroup/gpu/gpu.usage_in_bytes",
    O_RDONLY);

  if (status_file.IsValid()) {
    const int kBufferSize = 512;
    char buffer[kBufferSize];
    int bytes_read = status_file.ReadAll(buffer, kBufferSize);
    if (bytes_read == kBufferSize) {
      bytes_read = kBufferSize - 1;
    }
    buffer[bytes_read] = '\0';
    int64_t usage_in_bytes = strtoll(buffer, nullptr, 10);
    if (usage_in_bytes > 0 && errno != ERANGE)
      return usage_in_bytes;
  }

  return 0;
}
