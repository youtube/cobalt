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

#include "starboard/user.h"

#include "starboard/shared/nouser/user_internal.h"
#include "starboard/string.h"

int SbUserGetPropertySize(SbUser user, SbUserPropertyId property_id) {
  if (!SbUserIsValid(user)) {
    return 0;
  }

  switch (property_id) {
    case kSbUserPropertyUserName:
      return SbStringGetLength(user->name) + 1;

    case kSbUserPropertyUserId:
      return SbStringGetLength(user->id) + 1;

    case kSbUserPropertyHomeDirectory: {
      char path[SB_FILE_MAX_PATH];
      if (!starboard::shared::nouser::GetHomeDirectory(
              user, path, SB_ARRAY_SIZE_INT(path))) {
        return 0;
      }
      return SbStringGetLength(path);
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
      SbStringCopy(out_value, user->name, value_size);
      return true;

    case kSbUserPropertyUserId:
      SbStringCopy(out_value, user->id, value_size);
      return true;

    case kSbUserPropertyAvatarUrl:
    default:
      return false;
  }
}
