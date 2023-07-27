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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <unistd.h>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"

// We find the current amount of used memory on Linux by opening
// '/proc/self/status' and scan the file for its "VmRSS" and "VmSwap" entries.
// Essentially, we need to parse a buffer that has the following format:
//
// xxxxxx:       45327 kB
// yyyyyy:          23 kB
// VmRSS:        87432 kB
// zzzzzz:        3213 kB
// VmSwap:          15 kB
// ...
//
// And here, we would want to return the value 87432 * 1024 + 15 * 1024 (i.e.
// (VmRSS + VmSwap) * 1024).
// See http://man7.org/linux/man-pages/man5/proc.5.html for more details.

// Searches for a specific value we're interested in and return it.  Will
// modify |buffer| in order to do so quickly and easily.  Returns the memory
// value in bytes (not kilobytes as it is presented in /proc/self/status).
int64_t SearchForMemoryValue(
    const char* search_key, const char* buffer) {
  const char* found = strstr(buffer, search_key);
  if (!found) {
    SB_LOG(ERROR) << "Could not find '" << search_key << "' in "
                  << "/proc/self/status.";
    return 0;
  }

  while (*found != '\0' && (*found < '0' || *found > '9')) {
    ++found;
  }

  if (*found == '\0') {
    SB_LOG(ERROR) << "Unexpected end of string when searching for '"
                  << search_key << "' in /proc/self/status.";
    return 0;
  }

  // Convert the number string into an integer.
  int64_t memory_value_in_kilobytes = 0;
  while (*found >= '0' && *found <= '9') {
    memory_value_in_kilobytes = memory_value_in_kilobytes * 10 + (*found - '0');
    ++found;
  }

  return memory_value_in_kilobytes * 1024;
}

int64_t SbSystemGetUsedCPUMemory() {
  // Read our process' current physical memory usage from /proc/self/status.
  starboard::ScopedFile status_file("/proc/self/status",
                                    kSbFileOpenOnly | kSbFileRead);
  if (!status_file.IsValid()) {
    SB_LOG(ERROR)
        << "Error opening /proc/self/status in order to query self memory "
           "usage.";
    return 0;
  }

  // Read the entire file into memory.
  const int kBufferSize = 2048;
  char buffer[kBufferSize];
  int bytes_read = status_file.ReadAll(buffer, kBufferSize);

  // Ensure that this is a null-terminated string.
  if (bytes_read == kBufferSize) {
    bytes_read = kBufferSize - 1;
  }
  buffer[bytes_read] = '\0';

  // Return the result, multiplied by 1024 because it is given in kilobytes.
  return SearchForMemoryValue("VmRSS", buffer) +
         SearchForMemoryValue("VmSwap", buffer);
}
