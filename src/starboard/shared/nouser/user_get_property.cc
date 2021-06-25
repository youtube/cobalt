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

#include "starboard/user.h"

#include <vector>

#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/shared/nouser/user_internal.h"

int SbUserGetPropertySize(SbUser user, SbUserPropertyId property_id) {
  if (!SbUserIsValid(user)) {
    return 0;
  }

  switch (property_id) {
    case kSbUserPropertyUserName:
      return static_cast<int>(strlen(user->name) + 1);

    case kSbUserPropertyUserId:
      return static_cast<int>(strlen(user->id) + 1);

    case kSbUserPropertyHomeDirectory: {
      std::vector<char> path(kSbFileMaxPath);
      const int path_size = static_cast<int>(path.size());
      if (!starboard::shared::nouser::GetHomeDirectory(user, path.data(),
                                                       path_size)) {
        return 0;
      }
      return static_cast<int>(strlen(path.data()));
    }

    case kSbUserPropertyAvatarUrl:
    default:
      return 0;
  }
}

bool SbUserGetProperty(SbUser user,
                       SbUserPropertyId property_id,
                       char* out_value,
                       int value_size) {
  if (!SbUserIsValid(user) || !out_value ||
      value_size < SbUserGetPropertySize(user, property_id)) {
    return false;
  }

  switch (property_id) {
    case kSbUserPropertyHomeDirectory:
      return starboard::shared::nouser::GetHomeDirectory(user, out_value,
                                                         value_size);

    case kSbUserPropertyUserName:
      starboard::strlcpy(out_value, user->name, value_size);
      return true;

    case kSbUserPropertyUserId:
      starboard::strlcpy(out_value, user->id, value_size);
      return true;

    case kSbUserPropertyAvatarUrl:
    default:
      return false;
  }
}
