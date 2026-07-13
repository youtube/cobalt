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

#ifndef STARBOARD_TOOLS_FILE_UTILS_FILE_UTILS_H_
#define STARBOARD_TOOLS_FILE_UTILS_FILE_UTILS_H_

#include <vector>

namespace starboard {
namespace tools {

bool ReadFile(const char* file_path, std::vector<char>& buffer);
bool WriteFile(const char* file_path, const std::vector<char>& buffer);

}  // namespace tools
}  // namespace starboard

#endif  // STARBOARD_TOOLS_FILE_UTILS_FILE_UTILS_H_
