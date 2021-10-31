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

#if SB_API_VERSION < 12
// Represents a directory entry.
typedef struct SbDirectoryEntry {
  // The name of this directory entry.
  char name[SB_FILE_MAX_NAME];
} SbDirectoryEntry;
#endif  // SB_API_VERSION < 12

// Well-defined value for an invalid directory stream handle.
#define kSbDirectoryInvalid ((SbDirectory)NULL)

// Returns whether the given directory stream handle is valid.
static SB_C_INLINE bool SbDirectoryIsValid(SbDirectory directory) {
  return directory != kSbDirectoryInvalid;
}

// Opens the given existing directory for listing. This function returns
// kSbDirectoryInvalidHandle if it is not successful.
//
// If |out_error| is provided by the caller, it will be set to the appropriate
// SbFileError code on failure.
//
// |out_error|: An output parameter that, in case of an error, is set to the
// reason that the directory could not be opened.
SB_EXPORT SbDirectory SbDirectoryOpen(const char* path, SbFileError* out_error);

// Closes an open directory stream handle. The return value indicates whether
// the directory was closed successfully.
//
// |directory|: The directory stream handle to close.
SB_EXPORT bool SbDirectoryClose(SbDirectory directory);

#if SB_API_VERSION >= 12
// Populates |out_entry| with the next entry in the specified directory stream,
// and moves the stream forward by one entry.
//
// Platforms may, but are not required to, return |.| (referring to the
// directory itself) and/or |..| (referring to the directory's parent directory)
// as entries in the directory stream.
//
// This function returns |true| if there was a next directory, and |false|
// at the end of the directory stream or if |out_entry_size| is smaller than
// kSbFileMaxName.
//
// |directory|: The directory stream from which to retrieve the next directory.
// |out_entry|: The null terminated string to be populated with the next
//              directory entry. The space allocated for this string should be
//              equal to |out_entry_size|.
// |out_entry_size|: The size of the space allocated for |out_entry|. This
//                   should be at least equal to kSbFileMaxName.
SB_EXPORT bool SbDirectoryGetNext(SbDirectory directory,
                                  char* out_entry,
                                  size_t out_entry_size);
#else   // SB_API_VERSION >= 12
// Populates |out_entry| with the next entry in the specified directory stream,
// and moves the stream forward by one entry.
//
// Platforms may, but are not required to, return |.| (referring to the
// directory itself) and/or |..| (referring to the directory's parent directory)
// as entries in the directory stream.
//
// This function returns |true| if there was a next directory, and |false|
// at the end of the directory stream.
//
// |directory|: The directory stream from which to retrieve the next directory.
// |out_entry|: The variable to be populated with the next directory entry.
SB_EXPORT bool SbDirectoryGetNext(SbDirectory directory,
                                  SbDirectoryEntry* out_entry);
#endif  // SB_API_VERSION >= 12

// Indicates whether SbDirectoryOpen is allowed for the given |path|.
//
// |path|: The path to be checked.
SB_EXPORT bool SbDirectoryCanOpen(const char* path);

// Creates the directory |path|, assuming the parent directory already exists.
// This function returns |true| if the directory now exists (even if it existed
// before) and returns |false| if the directory does not exist.
//
// |path|: The path to be created.
SB_EXPORT bool SbDirectoryCreate(const char* path);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_DIRECTORY_H_
