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
#include <unistd.h>

#include <cstdint>
#include <string>
#include <string_view>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/system.h"

int64_t SbSystemGetUsedGPUMemory() {
  // =========================================================================
  // STEP 1: Open the Mali GPU memory kernel sysfs interface.
  // =========================================================================
  // On Mali GPU systems (e.g., Amlogic SoC), the kernel driver exposes a node
  // listing GPU memory allocated per process at:
  // /sys/class/misc/mali0/device/gpu_memory
  starboard::ScopedFile file("/sys/class/misc/mali0/device/gpu_memory",
                             O_RDONLY);
  if (!file.IsValid()) {
    SB_DLOG(INFO) << "Mali sysfs node /sys/class/misc/mali0/device/gpu_memory "
                  << "is not accessible or valid.";
    return 0;
  }

  // =========================================================================
  // STEP 2: Read the table into a stack buffer.
  // =========================================================================
  // The Mali sysfs node (/sys/class/misc/mali0/device/gpu_memory) format is:
  // mali0            total used_pages      18117
  // ----------------------------------------------------
  // kctx             pid              used_pages      
  // ----------------------------------------------------
  // 00000000a0c016f1      30455      17528
  // 000000004e911e4b      30434        170
  // 000000005e2f4eff      30255        419
  char buffer[2048];
  int bytes_read = file.ReadAll(buffer, sizeof(buffer) - 1);
  if (bytes_read <= 0) {
    SB_DLOG(INFO) << "Read 0 bytes from Mali sysfs node.";
    return 0;
  }
  SB_DLOG(INFO) << "Read " << bytes_read << " bytes from Mali GPU sysfs node.";

  std::string_view text(buffer, bytes_read);
  std::string pid_str = std::to_string(getpid());
  SB_DLOG(INFO) << "Searching Mali GPU memory table for current process PID: "
                << pid_str;

  // =========================================================================
  // STEP 3 & 4: Locate our process ID (PID) in the sysfs table and parse pages.
  // =========================================================================
  // Locate PID surrounded by spaces in the 'pid' column so substrings inside
  // kctx or used_pages do not accidentally match.
  std::string target = " " + pid_str + " ";
  size_t pos = text.find(target);
  if (pos == std::string_view::npos) {
    SB_DLOG(INFO) << "PID " << pid_str
                  << " not found in Mali GPU sysfs memory table.";
    return 0;
  }

  // Find the start of the used_pages count immediately after " <pid> ".
  size_t num_pos = text.find_first_not_of(" \t", pos + target.size());
  if (num_pos == std::string_view::npos) {
    SB_DLOG(ERROR) << "Malformed Mali GPU sysfs table: missing pages column "
                   << "after PID " << pid_str;
    return 0;
  }

  std::string num_str = std::string(text.substr(num_pos));
  int64_t pages = std::stoll(num_str);
  long page_size = sysconf(_SC_PAGE_SIZE);

  SB_DLOG(INFO) << "Mali GPU sysfs entry for PID " << pid_str << ": " << pages
                << " pages (page size " << page_size << " bytes).";

  int64_t total_gpu_bytes = pages * page_size;
  SB_DLOG(INFO) << "Total GPU memory used by PID " << pid_str << ": "
                << total_gpu_bytes << " bytes.";

  return total_gpu_bytes;
}
