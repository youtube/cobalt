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

// Module Overview: Starboard Directory module
//
// Provides directory listing functions.

#ifndef STARBOARD_DIRECTORY_H_
#define STARBOARD_DIRECTORY_H_

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/file.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Private structure representing an open directory stream.
struct SbDirectoryPrivate;

// A handle to an open directory stream.
typedef struct SbDirectoryPrivate* SbDirectory;

// Well-defined value for an invalid directory stream handle.
#define kSbDirectoryInvalid ((SbDirectory)NULL)

// Returns whether the given directory stream handle is valid.
static inline bool SbDirectoryIsValid(SbDirectory directory) {
  return directory != kSbDirectoryInvalid;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_DIRECTORY_H_
