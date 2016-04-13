// Copyright 2015 Google Inc. All Rights Reserved.
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

// Directory listing API.

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

// Represents a directory entry.
typedef struct SbDirectoryEntry {
  // The name of this directory entry.
  char name[SB_FILE_MAX_NAME];
} SbDirectoryEntry;

// Well-defined value for an invalid directory stream handle.
#define kSbDirectoryInvalid ((SbDirectory)NULL)

// Returns whether the given directory stream handle is valid.
static SB_C_INLINE bool SbDirectoryIsValid(SbDirectory directory) {
  return directory != kSbDirectoryInvalid;
}

// Opens the given existing directory for listing. Will return
// kSbDirectoryInvalidHandle if not successful. If |out_error| is provided by
// the caller, it will be set to the appropriate SbFileError code on failure.
SB_EXPORT SbDirectory SbDirectoryOpen(const char* path, SbFileError* out_error);

// Closes an open directory stream handle. Returns whether the close was
// successful.
SB_EXPORT bool SbDirectoryClose(SbDirectory directory);

// Populates |out_entry| with the next entry in that directory stream, and moves
// the stream forward by one entry. Returns |true| if there was a next
// directory, and |false| at the end of the directory stream.
SB_EXPORT bool SbDirectoryGetNext(SbDirectory directory,
                                  SbDirectoryEntry* out_entry);

// Returns whether SbDirectoryOpen is allowed for the given |path|.
SB_EXPORT bool SbDirectoryCanOpen(const char* path);

// Creates the directory |path|, assuming the parent directory already exists.
// Returns whether the directory now exists (even if it existed before).
SB_EXPORT bool SbDirectoryCreate(const char* path);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_DIRECTORY_H_
