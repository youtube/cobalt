// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_GET_HOME_DIRECTORY_H_
#define STARBOARD_SHARED_STARBOARD_GET_HOME_DIRECTORY_H_

#include "starboard/shared/internal_only.h"

namespace starboard {

// A platform implementation of getting the home directory. A return value of
// false indicates that the platform doesn't have a concept of a home directory
// and that custom logic may be needed in |SbSystemGetPath| to determine
// appropriate system paths that may normally be stored under a home directory.
bool GetHomeDirectory(char* out_path, int path_size);

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_GET_HOME_DIRECTORY_H_
