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

#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <cstdint>

#include "starboard/common/file.h"
#include "starboard/system.h"

int64_t SbSystemGetUsedGPUMemory() {
  starboard::ScopedFile file("/sys/class/misc/mali0/device/gpu_memory",
                             O_RDONLY);
  if (!file.IsValid()) {
    return 0;
  }

  char buffer[2048];
  int bytes_read = file.ReadAll(buffer, sizeof(buffer) - 1);
  if (bytes_read <= 0) {
    return 0;
  }
  buffer[bytes_read] = '\0';

  char target[32];
  int target_len = snprintf(target, sizeof(target), "%d", getpid());

  char* pos = strstr(buffer, target);
  if (!pos) {
    return 0;
  }

  int64_t pages = 0;
  if (sscanf(pos + target_len, "%" SCNd64, &pages) == 1) {
    return pages * sysconf(_SC_PAGE_SIZE);
  }

  return 0;
}
