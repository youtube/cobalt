// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_UWP_LOG_FILE_IMPL_H_
#define STARBOARD_SHARED_UWP_LOG_FILE_IMPL_H_

namespace starboard {
namespace shared {
namespace uwp {

void OpenLogFileUWP(Windows::Storage::StorageFolder^ folder,
                    const char* filename);

void OpenLogFileWin32(const char* path);

void CloseLogFile();
void WriteToLogFile(const char* text, int text_length);

}  // namespace uwp
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_UWP_LOG_FILE_IMPL_H_
