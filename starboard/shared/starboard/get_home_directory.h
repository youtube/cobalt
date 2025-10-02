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

// RDK build on CI still defines GetHomeDirectory in the legacy
// `starboard::shared::starboard` namespace. See:
// https://source.corp.google.com/h/lbshell-internal/cobalt_src/+/25.lts.1+:starboard/contrib/rdk/src/third_party/starboard/rdk/shared/get_home_directory.cc;l=1?q=rdk%2Fshared%2Fget_home_directory.cc
// TODO: b/441955897 - Remove this nested namespace once RDK build on CI is
// updated.
namespace shared::starboard {

// A platform implementation of getting the home directory. A return value of
// false indicates that the platform doesn't have a concept of a home directory
// and that custom logic may be needed in |SbSystemGetPath| to determine
// appropriate system paths that may normally be stored under a home directory.
bool GetHomeDirectory(char* out_path, int path_size);
}  // namespace shared::starboard

// TODO: b/441955897 - Remove this alias once RDK build on CI is
// updated.
using shared::starboard::GetHomeDirectory;

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_GET_HOME_DIRECTORY_H_
