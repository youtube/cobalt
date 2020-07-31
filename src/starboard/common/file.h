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

// Module Overview: Starboard File module
//
// Implements a set of convenience functions and macros built on top of the core
// Starboard file API.

#ifndef STARBOARD_COMMON_FILE_H_
#define STARBOARD_COMMON_FILE_H_

#include "starboard/file.h"

namespace starboard {

// Deletes the file, symlink or directory at |path|. When |path| is a directory,
// the function will recursively delete the entire tree; however, when
// |preserve_root| is |true| the root directory is not removed. On some
// platforms, this function fails if a file to be deleted is being held open.
//
// Returns |true| if the file, symlink, or directory was able to be deleted, and
// |false| if there was an error at any point.
//
// |path|: The absolute path of the file, symlink, or directory to be deleted.
// |preserve_root|: Whether or not the root directory should be preserved.
bool SbFileDeleteRecursive(const char* path, bool preserve_root);

}  // namespace starboard

#endif  // STARBOARD_COMMON_FILE_H_
