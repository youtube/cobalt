// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

// This header provides a mechanism for multiple Android logging
// formats to share a single log file handle.

#ifndef STARBOARD_SHARED_WIN32_LOG_FILE_IMPL_H_
#define STARBOARD_SHARED_WIN32_LOG_FILE_IMPL_H_

#include "starboard/common/mutex.h"
#include "starboard/file.h"

namespace starboard {
namespace shared {
namespace win32 {

// Closes the log file.
void CloseLogFile();

// Opens a file in |kSbSystemPathCacheDirectory| directory.
// |log_file_name|: C-style string of a filename
// |creation_flags|: Must be kSbFileCreateAlways (which will truncate the file)
//   |kSbFileOpenAlways|, which can be used to append to the file.
void OpenLogInCacheDirectory(const char* log_file_name, int creation_flags);

// Opens a file at |log_file_path| with |creation_flags|.
// |log_file_name|: C-style string of a filename
// |creation_flags|: Must be kSbFileCreateAlways (which will truncate the file)
//   |kSbFileOpenAlways|, which can be used to append to the file.
void OpenLogFile(const char* log_file_path, int creation_flags);

// Writes |text_length| bytes starting from |text| to the current log file.
void WriteToLogFile(const char* text, const int text_length);

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_LOG_FILE_IMPL_H_
