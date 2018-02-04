// Copyright 2016 Google Inc. All Rights Reserved.
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

#include <app_common.h>
#include <unistd.h>

#include "starboard/log.h"
#include "starboard/shared/nouser/user_internal.h"
#include "starboard/string.h"

namespace starboard {
namespace shared {
namespace nouser {

bool GetHomeDirectory(SbUser user, char* out_path, int path_size) {
  if (user != SbUserGetCurrent()) {
    SB_DLOG(ERROR) << "Invalid User";
    return false;
  }

  const char* data_path = app_get_data_path();
  if (data_path) {
    SB_DLOG(INFO) << "App data path [" << data_path << "]";
    SbStringCopy(out_path, data_path, path_size);
    return true;
  }

  // for nplb, unittest, ...
  const char* curr_dir = getcwd(NULL, 0);
  if (curr_dir) {
    SB_DLOG(WARNING) << "No data path. Set CWD " << curr_dir;
    SbStringCopy(out_path, curr_dir, path_size);
    return true;
  }

  SB_DLOG(ERROR) << "No home directory variable";
  return false;
}

}  // namespace nouser
}  // namespace shared
}  // namespace starboard
