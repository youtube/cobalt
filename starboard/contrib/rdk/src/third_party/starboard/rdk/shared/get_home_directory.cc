//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
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

#include "starboard/configuration_constants.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/shared/starboard/get_home_directory.h"

namespace starboard {

bool GetHomeDirectory(char* out_path, int path_size) {

  const char* home_directory = getenv("HOME");
  if (home_directory) {
    ::starboard::strlcpy<char>(out_path, home_directory, path_size);
    return true;
  }

  SB_DLOG(WARNING) << "No HOME environment variable.";
  struct passwd passwd;
  const size_t kBufferSize = kSbFileMaxPath * 4;
  char* buffer = new char[kBufferSize];
  struct passwd* pw_result = NULL;
  int result =
      getpwuid_r(getuid(), &passwd, buffer, kBufferSize, &pw_result);
  if (result != 0) {
    SB_DLOG(ERROR) << "getpwuid_r failed for uid " << getuid() << ": result = "
                   << result;
    delete[] buffer;
    return false;
  }

  ::starboard::strlcpy<char>(out_path, passwd.pw_dir, path_size);
  delete[] buffer;
  return true;
}

}  // namespace starboard
