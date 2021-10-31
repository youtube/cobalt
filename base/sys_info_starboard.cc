// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "base/sys_info.h"

#include "base/logging.h"
#include "starboard/system.h"

namespace base {

// static
int SysInfo::NumberOfProcessors() {
  return SbSystemGetNumberOfProcessors();
}

// static
int64_t SysInfo::AmountOfFreeDiskSpace(const FilePath& path) {
  // TODO: This is referred to ONLY by disk_cache::BackendImpl, which I do
  // not think is currently used in Cobalt. There's no need to implement this
  // unless we want to use it for something. If not, we should remove the
  // reference to it, and this amazing implementation.
  NOTIMPLEMENTED();
  return SB_INT64_C(1) * 1024 * 1024 * 1024;
}

// static
int64_t SysInfo::AmountOfPhysicalMemoryImpl() {
  return SbSystemGetTotalCPUMemory();
}

// static
int64_t SysInfo::AmountOfAvailablePhysicalMemoryImpl() {
  return SbSystemGetTotalCPUMemory() - SbSystemGetUsedCPUMemory();
}

// static
int64_t SysInfo::AmountOfTotalDiskSpace(const FilePath& path) {
  ALLOW_UNUSED_LOCAL(path);
  NOTIMPLEMENTED();
  return SB_INT64_C(1) * 1024 * 1024 * 1024;
}

// static
int64_t SysInfo::AmountOfVirtualMemory() {
  return AmountOfPhysicalMemoryImpl();
}

}  // namespace base
