// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/file.h"

#include <cstdio>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/shared/starboard/file_atomic_replace_write_file.h"

#if SB_API_VERSION >= 12

namespace {

const char kTempFileSuffix[] = ".temp";

}  // namespace

bool SbFileAtomicReplace(const char* path,
                         const char* data,
                         int64_t data_size) {
  if ((data_size < 0) || ((data_size > 0) && !data)) {
    return false;
  }

  const bool file_exists = SbFileExists(path);
  std::vector<char> temp_path(kSbFileMaxPath + 1, 0);

  starboard::strlcpy(temp_path.data(), path, kSbFileMaxPath);
  starboard::strlcat(temp_path.data(), kTempFileSuffix, kSbFileMaxPath);

  if (!::starboard::shared::starboard::SbFileAtomicReplaceWriteFile(
          temp_path.data(), data, data_size)) {
    return false;
  }
  if (file_exists && !SbFileDelete(path)) {
    return false;
  }
  if (rename(temp_path.data(), path) != 0) {
    return false;
  }
  return true;
}

#endif  // SB_API_VERSION >= 12
