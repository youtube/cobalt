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

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/system.h"

namespace {

std::string GetMemoryCgroupPath(const char* property) {
  std::string path;
  starboard::ScopedFile cgroup_file("/proc/self/cgroup", O_RDONLY);

  if (cgroup_file.IsValid()) {
    const int kBufferSize = 4096;
    char buffer[kBufferSize];
    int bytes_read = cgroup_file.ReadAll(buffer, kBufferSize - 1);

    if (bytes_read > 0) {
      buffer[bytes_read] = '\0';
      const char* memory_subsys = strstr(buffer, ":memory:");
      if (memory_subsys) {
        const char* line_end = strchr(memory_subsys, '\n');
        const char* path_start = strchr(memory_subsys + 8, '/');
        if (path_start && (!line_end || path_start < line_end)) {
          if (line_end) {
            path = std::string(path_start, line_end - path_start);
          } else {
            path = std::string(path_start);
          }
        }
      }
    }
  }

  return std::string("/sys/fs/cgroup/memory") + path + "/" + property;
}

}  // namespace

int64_t SbSystemGetUsedCPUMemory() {
  starboard::ScopedFile status_file(
      GetMemoryCgroupPath("memory.usage_in_bytes").c_str(), O_RDONLY);

  if (status_file.IsValid()) {
    const int kBufferSize = 512;
    char buffer[kBufferSize];
    int bytes_read = status_file.ReadAll(buffer, kBufferSize);
    if (bytes_read == kBufferSize) {
      bytes_read = kBufferSize - 1;
    }
    buffer[bytes_read] = '\0';
    int64_t val = strtoll(buffer, nullptr, 10);
    if (val > 0) {
      return val;
    }
  }
  return 0;
}
