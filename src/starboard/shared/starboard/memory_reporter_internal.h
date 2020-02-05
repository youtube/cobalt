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

#ifndef STARBOARD_SHARED_STARBOARD_MEMORY_REPORTER_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_MEMORY_REPORTER_INTERNAL_H_

#include "starboard/export.h"
#include "starboard/shared/internal_only.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Internal function used to track mapped memory. This is used internally by
// implementations of SbMemoryMap().
void SbMemoryReporterReportMappedMemory(const void* memory, size_t size);

// Internal function used to track mapped memory. This is used internally by
// implementations of SbMemoryUnmap().
void SbMemoryReporterReportUnmappedMemory(const void* memory, size_t size);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_STARBOARD_MEMORY_REPORTER_INTERNAL_H_
