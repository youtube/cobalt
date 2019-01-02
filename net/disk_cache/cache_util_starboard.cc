// Copyright 2018 Google Inc. All Rights Reserved.
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

// Adapted from cache_util_posix.cc

#include "net/disk_cache/cache_util.h"

#include "base/files/file_util.h"

namespace disk_cache {
bool DeleteCacheFile(const base::FilePath& name) {
  return base::DeleteFile(name, false);
}

}  // namespace disk_cache
