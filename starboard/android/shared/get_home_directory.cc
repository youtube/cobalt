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

#include <pwd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "starboard/android/shared/file_internal.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"

namespace starboard::shared::starboard {

using ::base::android::ScopedJavaGlobalRef;

bool GetHomeDirectory(char* out_path, int path_size) {
  int len = ::starboard::strlcpy(out_path, g_app_files_dir, path_size);
  return len < path_size;
}

}  // namespace starboard::shared::starboard
