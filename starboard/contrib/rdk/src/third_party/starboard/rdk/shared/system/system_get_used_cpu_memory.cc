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

// clang-format off
#include "starboard/system.h"
// clang-format on

#include <fcntl.h>
#include <stdlib.h>

#include "starboard/common/file.h"
#include "starboard/common/log.h"

int64_t SbSystemGetUsedCPUMemory() {
  starboard::ScopedFile status_file(
      "/sys/fs/cgroup/memory/memory.usage_in_bytes", O_RDONLY);
  if (status_file.IsValid()) {
    const int kBufferSize = 512;
    char buffer[kBufferSize];
    int bytes_read = status_file.ReadAll(buffer, kBufferSize);
    if (bytes_read == kBufferSize) {
      bytes_read = kBufferSize - 1;
    }
    if (bytes_read >= 0) {
      buffer[bytes_read] = '\0';
      int64_t val = strtoll(buffer, nullptr, 10);
      if (val > 0) {
        return val;
      }
    }
  }
  return 0;
}
