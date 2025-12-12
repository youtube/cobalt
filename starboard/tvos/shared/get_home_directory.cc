// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/get_home_directory.h"

#include "starboard/system.h"

namespace starboard {
namespace shared {
namespace starboard {

bool GetHomeDirectory(char* out_path, int path_size) {
  return SbSystemGetPath(kSbSystemPathCacheDirectory, out_path, path_size);
}

}  // namespace starboard
}  // namespace shared
}  // namespace starboard
