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

#include <pwd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/shared/nouser/user_internal.h"

namespace starboard {
namespace shared {
namespace nouser {

bool GetHomeDirectory(SbUser user, char* out_path, int path_size) {
  if (user != SbUserGetCurrent()) {
    return false;
  }

  const char* home_directory = getenv("HOME");
  if (home_directory) {
    starboard::strlcpy(out_path, home_directory, path_size);
    return true;
  }

  SB_DLOG(WARNING) << "No HOME environment variable.";
  struct passwd passwd;
  const size_t kBufferSize = kSbFileMaxPath * 4;
  std::vector<char> buffer(kBufferSize);
  struct passwd* pw_result = NULL;
  int result =
      getpwuid_r(getuid(), &passwd, buffer.data(), kBufferSize, &pw_result);
  if (result != 0) {
    SB_DLOG(ERROR) << "getpwuid_r failed for uid " << getuid() << ": result = "
                   << result;
    return false;
  }

  starboard::strlcpy(out_path, passwd.pw_dir, path_size);
  return true;
}

}  // namespace nouser
}  // namespace shared
}  // namespace starboard
