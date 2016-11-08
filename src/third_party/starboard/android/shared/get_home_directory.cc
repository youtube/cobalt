// Copyright 2016 Google Inc. All Rights Reserved.
// Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#include "starboard/log.h"
#include "starboard/shared/nouser/user_internal.h"
#include "starboard/string.h"
#include "third_party/starboard/android/shared/application_android.h"

namespace starboard {
namespace shared {
namespace nouser {

bool GetHomeDirectory(SbUser user, char* out_path, int path_size) {
  if (user != SbUserGetCurrent()) {
    return false;
  }

  const char* external_files_dir = ::starboard::android::shared::ApplicationAndroid::Get()->GetExternalFilesDir();
  if(external_files_dir) {
      SbStringCopy(out_path, external_files_dir, path_size);
      return true;
  }
  return false;
}

}  // namespace nouser
}  // namespace shared
}  // namespace starboard
