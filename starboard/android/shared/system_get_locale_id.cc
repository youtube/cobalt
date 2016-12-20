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

#include "starboard/system.h"

#include <android/configuration.h>
#include <string>

#include "starboard/once.h"

#include "starboard/string.h"

namespace {

// A singleton class to hold a locale string
class LocaleInfo {
 public:
  // The Starboard locale id
  std::string locale_id;

  LocaleInfo() {
    AConfiguration* config = AConfiguration_new();

    // "The output will be filled with an array of two characters. They are not
    // 0-terminated. If a language is not set, they will be 0."
    //
    // https://developer.android.com/ndk/reference/group___configuration.html
    char outLanguage[2];
    AConfiguration_getLanguage(config, outLanguage);

    char outCountry[2];
    AConfiguration_getCountry(config, outCountry);

    if (outLanguage[0] == 0) {
      locale_id = "en_US";
    } else {
      locale_id.append(1, outLanguage[0]);
      locale_id.append(1, outLanguage[1]);
      if (outCountry[0] != 0) {
        locale_id.append(1, '_');
        locale_id.append(1, outCountry[0]);
        locale_id.append(1, outCountry[1]);
      }
    }

    AConfiguration_delete(config);
  }
};

SB_ONCE_INITIALIZE_FUNCTION(LocaleInfo, GetLocale);
}  // namespace

const char* SbSystemGetLocaleId() {
  return GetLocale()->locale_id.c_str();
}
