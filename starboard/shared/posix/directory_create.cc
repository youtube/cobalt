// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/directory.h"

#include <stddef.h>
#include <sys/stat.h>

#include <string>

#include "starboard/file.h"

namespace {

void RemoveTrailingSeparators(std::string* path) {
  size_t found = path->find_last_not_of('/');
  if (found != std::string::npos) {
    path->resize(found + 1);
  } else {
    path->resize(1);
  }
}

std::string GetParent(const std::string& path) {
  size_t last_slash = path.find_last_of('/');
  if (last_slash != std::string::npos) {
    std::string parent = path.substr(0, last_slash);
    RemoveTrailingSeparators(&parent);
    return parent;
  }

  return "";
}

}  // namespace

bool SbDirectoryCreate(const char* path) {
  // Require a non-empty, absolute path.
  if (!path || path[0] != '/') {
    return false;
  }

  // Clear trailing slashes.
  std::string adjusted(path);
  RemoveTrailingSeparators(&adjusted);

  if (SbDirectoryCanOpen(adjusted.c_str())) {
    return true;
  }

  std::string parent = GetParent(adjusted);
  if (!parent.empty() && !SbDirectoryCanOpen(parent.c_str())) {
    return false;
  }

  if (mkdir(adjusted.c_str(), 0700) == 0) {
    return true;
  }

  // If mkdir failed, the directory may still exist now (because of another
  // racing process or thread), so check again.
  return SbDirectoryCanOpen(adjusted.c_str());
}
