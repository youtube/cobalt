// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include <windows.h>

#include "starboard/shared/environment.h"

namespace starboard {

std::string GetEnvironment(std::string name) {
  std::wstring w_name(name.begin(), name.end());
  DWORD len = GetEnvironmentVariable(w_name.c_str(), nullptr, 0);
  if (len == 0)
    return "";
  std::wstring w_environment_string(len, 0);
  GetEnvironmentVariable(w_name.c_str(), &w_environment_string[0], len);
  std::string environment_string(w_environment_string.begin(),
                                 w_environment_string.end());
  return environment_string;
}

}  // namespace starboard
