// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "base/memory/platform_shared_memory_region.h"

#include "base/notreached.h"

namespace base {
namespace subtle {

PlatformSharedMemoryRegion PlatformSharedMemoryRegion::Take(
    ScopedPlatformSharedMemoryHandle handle,
    Mode mode,
    size_t size,
    const UnguessableToken& guid) {
  NOTREACHED();
  return {};
}

bool PlatformSharedMemoryRegion::IsValid() const {
  NOTREACHED();
  return false;
}

PlatformSharedMemoryRegion PlatformSharedMemoryRegion::Duplicate() const {
  NOTREACHED();
  return {};
}

bool PlatformSharedMemoryRegion::ConvertToReadOnly() {
  NOTREACHED();
  return false;
}

bool PlatformSharedMemoryRegion::ConvertToUnsafe() {
  NOTREACHED();
  return false;
}

PlatformSharedMemoryRegion PlatformSharedMemoryRegion::Create(Mode mode,
                                                              size_t size) {
  NOTREACHED();
  return {};
}

bool PlatformSharedMemoryRegion::CheckPlatformHandlePermissionsCorrespondToMode(
    PlatformSharedMemoryHandle handle,
    Mode mode,
    size_t size) {
  NOTREACHED();
  return false;
}

PlatformSharedMemoryHandle PlatformSharedMemoryRegion::GetPlatformHandle()
    const {
  NOTREACHED();
  return 0;
}

}  // namespace subtle
}  // namespace base