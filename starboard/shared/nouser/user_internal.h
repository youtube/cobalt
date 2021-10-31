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

#ifndef STARBOARD_SHARED_NOUSER_USER_INTERNAL_H_
#define STARBOARD_SHARED_NOUSER_USER_INTERNAL_H_

#include "starboard/shared/internal_only.h"
#include "starboard/user.h"

struct SbUserPrivate {
  static const int kMaxUserName = 256;

  const char name[kMaxUserName];
  const char id[kMaxUserName];
};

namespace starboard {
namespace shared {
namespace nouser {
// The one instance of the signed-in user.
extern SbUserPrivate g_user;

// A platform implementation of getting the home directory for a user.
bool GetHomeDirectory(SbUser user, char* out_path, int path_size);
}  // namespace nouser
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_NOUSER_USER_INTERNAL_H_
