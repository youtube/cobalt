// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard Thread module
//
// Defines functionality related to thread creation and cleanup.

#ifndef STARBOARD_THREAD_H_
#define STARBOARD_THREAD_H_

#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "starboard/configuration.h"
#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif

// An ID type that is unique per thread.
typedef int32_t SbThreadId;

// Well-defined constant value to mean "no thread ID."
#define kSbThreadInvalidId (SbThreadId)0

// Returns whether the given thread ID is valid.
static inline bool SbThreadIsValidId(SbThreadId id) {
  return id != kSbThreadInvalidId;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_THREAD_H_
