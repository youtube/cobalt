// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_LOADER_APP_DRAIN_FILE_H_
#define STARBOARD_LOADER_APP_DRAIN_FILE_H_

#include <stdbool.h>
#include <stdint.h>

#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif

// The units of time that the drain file age is represented in, in microseconds.
extern const int64_t kDrainFileAgeUnitUsec;

// The amount of time of which a drain file is valid, in microseconds.
extern const int64_t kDrainFileMaximumAgeUsec;

// The prefix that all drain file names will have.
extern const char kDrainFilePrefix[];

// Attempts to create a drain file in |dir| with the provided |app_key|. If
// there is an existing, non-expired drain file with a matching |app_key| it
// will be used instead. Returns |true| if a file was created or reused,
// otherwise |false|.
bool DrainFileTryDrain(const char* dir, const char* app_key);

// Ranks the non-expired drain files in |dir| in non-descending order by drain
// file creation time, breaking ties by a C string comparison of the app keys
// of the drain files, and compares the provided |app_key| with the app key of
// the first ranked drain file. Returns |true| if they match, otherwise |false|.
bool DrainFileRankAndCheck(const char* dir, const char* app_key);

// Clears all expired drain files in |dir| for all apps.
void DrainFileClearExpired(const char* dir);

// Clears the drain files in |dir| with an app key matching |app_key|.
void DrainFileClearForApp(const char* dir, const char* app_key);

// Clears all files and directories in |dir| except for the drain file with an
// app key matching |app_key|.
void DrainFilePrepareDirectory(const char* dir, const char* app_key);

// Checks whether a non-expired drain file exists in |dir| for an app
// with key |app_key|.
bool DrainFileIsAppDraining(const char* dir, const char* app_key);

// Checks whether a non-expired drain file exists in |dir| for an app
// with key different from |app_key|.
bool DrainFileIsAnotherAppDraining(const char* dir, const char* app_key);

// Converts a POSIX microseconds timestamp to a Windows microseconds timestamp.
static SB_C_FORCE_INLINE int64_t PosixTimeToWindowsTime(int64_t posix_time) {
  // Add number of microseconds since Jan 1, 1601 (UTC) until Jan 1, 1970 (UTC).
  return posix_time + 11644473600000000ULL;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_LOADER_APP_DRAIN_FILE_H_
