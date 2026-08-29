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
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"

namespace {

int64_t SearchForMemoryValue(const char* search_key, const char* buffer) {
  const char* found = strstr(buffer, search_key);
  if (!found) {
    SB_LOG(ERROR) << "Could not find '" << search_key << "' in smaps_rollup.";
    return 0;
  }

  const char* digits = strpbrk(found + strlen(search_key), "0123456789");
  if (!digits) {
    SB_LOG(ERROR) << "Could not find any digits for '" << search_key
                  << "' in smaps_rollup.";
    return 0;
  }

  int64_t memory_value_in_kilobytes = 0;
  if (sscanf(digits, "%" PRId64, &memory_value_in_kilobytes) != 1) {
    SB_LOG(ERROR) << "Could not parse integer value for '" << search_key
                  << "' in smaps_rollup.";
    return 0;
  }

  return memory_value_in_kilobytes * 1024;
}

}  // namespace

int64_t SbSystemGetUsedCPUMemory() {
  // On RDK devices, /proc/self/smaps_rollup is optimized to provide accurate
  // memory footprint calculations avoiding double counting UMA GPU memory buffers
  // that can be misreported by /proc/self/status VmRSS.
  starboard::ScopedFile status_file("/proc/self/smaps_rollup", O_RDONLY);
  if (!status_file.IsValid()) {
    SB_LOG(ERROR)
        << "Error opening /proc/self/smaps_rollup in order to query self memory usage.";
    return 0;
  }

  const int kBufferSize = 4096;
  char buffer[kBufferSize];
  int bytes_read = status_file.ReadAll(buffer, kBufferSize - 1);
  if (bytes_read < 0) {
    bytes_read = 0;
  }
  buffer[bytes_read] = '\0';

  // Use Rss footprint from smaps_rollup which is robust on RDK.
  return SearchForMemoryValue("Rss:", buffer);
}
