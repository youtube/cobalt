// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/file.h"
#include "starboard/log.h"
#include "starboard/string.h"

// We find the current amount of used memory on Linux by opening
// '/proc/self/status' and scan the file for its "VmRSS" entry.  Essentially,
// we need to parse a buffer that has the following format:
//
// xxxxxx:       45327 kB
// yyyyyy:          23 kB
// VmRSS:        87432 kB
// zzzzzz:        3213 kB
// ...
//
// And here, we would want to return the value 87432 * 1024.
// See http://man7.org/linux/man-pages/man5/proc.5.html for more details.

// Searches for the value of VmRSS and returns it.  Will modify |buffer| in
// order to do so quickly and easily.
int64_t SearchForVmRSSValue(char* buffer, size_t length) {
  // Search for the string ""VmRSS:".
  const char kSearchString[] = "\nVmRSS:";
  enum State {
    // We are currently searching for kSearchString
    kSearchingForSearchString,
    // We found the search string and are advancing through spaces/tabs until
    // we see a number.
    kAdvancingSpacesToNumber,
    // We found the number and are now searching for the end of it.
    kFindingEndOfNumber,
  };
  State state = kSearchingForSearchString;
  const char* number_start = NULL;
  for (size_t i = 0; i < length - sizeof(kSearchString); ++i) {
    if (state == kSearchingForSearchString) {
      if (SbStringCompare(&buffer[i], kSearchString,
                          sizeof(kSearchString) - 1) == 0) {
        // Advance until we find a number.
        state = kAdvancingSpacesToNumber;
        i += sizeof(kSearchString) - 2;
      }
    } else if (state == kAdvancingSpacesToNumber) {
      if (buffer[i] >= '0' && buffer[i] <= '9') {
        // We found the start of the number, record where that is and then
        // continue searching for the end of the number.
        number_start = &buffer[i];
        state = kFindingEndOfNumber;
      }
    } else {
      SB_DCHECK(state == kFindingEndOfNumber);
      if (buffer[i] < '0' || buffer[i] > '9') {
        // Drop a null at the end of the number so that we can call atoi() on
        // it and return.
        buffer[i] = '\0';
        return SbStringAToI(number_start);
      }
    }
  }

  SB_LOG(ERROR) << "Could not find 'VmRSS:' in /proc/self/status.";
  return 0;
}

int64_t SbSystemGetUsedCPUMemory() {
  // Read our process' current physical memory usage from /proc/self/status.
  // This requires a bit of parsing through the output to find the value for
  // the "VmRSS" field which indicates used physical memory.
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
  int remaining = kBufferSize;
  char* output_pointer = buffer;
  do {
    int result = status_file.Read(output_pointer, remaining);
    if (result <= 0)
      break;

    remaining -= result;
    output_pointer += result;
  } while (remaining);

  // Return the result, multiplied by 1024 because it is given in kilobytes.
  return SearchForVmRSSValue(buffer,
                             static_cast<size_t>(output_pointer - buffer)) *
         1024;
}
