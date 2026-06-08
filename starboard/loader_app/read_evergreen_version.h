// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_LOADER_APP_READ_EVERGREEN_VERSION_H_
#define STARBOARD_LOADER_APP_READ_EVERGREEN_VERSION_H_

#include <vector>

namespace loader_app {

// The max length of Evergreen version string, including the
// null terminator.
extern const int kMaxEgVersionLength;

// Reads the Evergreen version from the manifest file at the
// |manifest_file_path|, and stores in |version|. Returns false
// if the manifest file is not found or cannot be read, the
// evergreen version cannot be parsed, or the stored |version|
// string is truncated. |version_length| should include space for
// the terminating null character.
bool ReadEvergreenVersion(const std::vector<char>& manifest_file_path,
                          char* version,
                          int version_length);

}  // namespace loader_app

#endif  // STARBOARD_LOADER_APP_READ_EVERGREEN_VERSION_H_
