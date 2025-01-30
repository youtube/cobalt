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

#include "base/system/sys_info.h"

#include "base/notreached.h"
#include "starboard/common/system_property.h"
#include "starboard/system.h"

namespace base {
 using starboard::kSystemPropertyMaxLength;

// static
int SysInfo::NumberOfProcessors() {
  return SbSystemGetNumberOfProcessors();
}

int SysInfo::NumberOfEfficientProcessorsImpl() {
  return NumberOfProcessors();
}

size_t SysInfo::VMAllocationGranularity() {
  // This is referred to ONLY by persistent memory allocator and shared
  // memory feature; not used in Cobalt production.
  NOTIMPLEMENTED();
  return 4096U;
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
uint64_t SysInfo::AmountOfPhysicalMemoryImpl() {
  return SbSystemGetTotalCPUMemory();
}

// static
uint64_t SysInfo::AmountOfAvailablePhysicalMemoryImpl() {
  return SbSystemGetTotalCPUMemory() - SbSystemGetUsedCPUMemory();
}

// static
int64_t SysInfo::AmountOfTotalDiskSpace(const FilePath& /* path */) {
  NOTIMPLEMENTED();
  return SB_INT64_C(1) * 1024 * 1024 * 1024;
}

// static
uint64_t SysInfo::AmountOfVirtualMemory() {
  return AmountOfPhysicalMemoryImpl();
}

// static
std::string SysInfo::OperatingSystemName() {
  char value[kSystemPropertyMaxLength];
  SbSystemGetProperty(kSbSystemPropertyPlatformName, value,
                      kSystemPropertyMaxLength);
  return value;
}

// static
std::string SysInfo::OperatingSystemVersion() {
  return SysInfo::OperatingSystemName();
}

// static
void SysInfo::OperatingSystemVersionNumbers(int32_t* major_version,
                                            int32_t* minor_version,
                                            int32_t* bugfix_version) {
  NOTIMPLEMENTED();
}

SysInfo::HardwareInfo SysInfo::GetHardwareInfoSync() {
  NOTIMPLEMENTED();
  HardwareInfo info;
  return info;
}

}  // namespace base
