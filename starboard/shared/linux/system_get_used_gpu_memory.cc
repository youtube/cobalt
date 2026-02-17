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

#include <stdio.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/system.h"

namespace {
// Parses /sys/kernel/debug/dma_buf/bufinfo to find DMA-BUFs associated with the
// current process.
// Note: This usually requires root or debugfs to be mounted and accessible.
int64_t GetDmaBufUsage() {
  const char* kBufInfoPath = "/sys/kernel/debug/dma_buf/bufinfo";
  starboard::ScopedFile file(kBufInfoPath, O_RDONLY);
  if (!file.IsValid()) {
    return 0;
  }

  const int kBufferSize = 32 * 1024;
  std::vector<char> buffer(kBufferSize);
  int bytes_read = file.ReadAll(buffer.data(), kBufferSize - 1);
  if (bytes_read <= 0) {
    return 0;
  }
  buffer[bytes_read] = '\0';

  int64_t total_size = 0;
  char* line = buffer.data();
  char* next_line = nullptr;
  pid_t my_pid = getpid();
  char pid_str[32];
  snprintf(pid_str, sizeof(pid_str), "pid: %d", my_pid);

  int64_t current_buf_size = 0;
  while (line && *line) {
    next_line = strchr(line, '
');
    if (next_line) {
      *next_line = '\0';
      next_line++;
    }

    // Look for "size: <number>"
    const char* size_ptr = strstr(line, "size: ");
    if (size_ptr) {
      current_buf_size = 0;
      size_ptr += 6;
      while (*size_ptr >= '0' && *size_ptr <= '9') {
        current_buf_size = current_buf_size * 10 + (*size_ptr - '0');
        size_ptr++;
      }
    }

    // If we find our PID in the "Attached Processes" section (which follows the
    // size line), we add the current_buf_size to total.
    // Note: This is a simplistic parser and might need refinement for different
    // kernel versions.
    if (strstr(line, pid_str)) {
      total_size += current_buf_size;
      // Reset current_buf_size so we don't count it multiple times if multiple
      // threads/PIDs are listed (though we only care about our PID).
      current_buf_size = 0;
    }

    line = next_line;
  }

  return total_size;
}
}  // namespace

int64_t SbSystemGetUsedGPUMemory() {
  return GetDmaBufUsage();
}
