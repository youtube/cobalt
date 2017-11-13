// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/base/language.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/logging.h"
#include "third_party/icu/source/common/unicode/uloc.h"

namespace base {

std::string GetSystemLanguage() {
  char buffer[ULOC_LANG_CAPACITY];
  UErrorCode icu_result = U_ZERO_ERROR;

  // Get the ISO language and country and assemble the system language.
  uloc_getLanguage(NULL, buffer, arraysize(buffer), &icu_result);
  if (!U_SUCCESS(icu_result)) {
    DLOG(FATAL) << __FUNCTION__ << ": Unable to get language from ICU for "
                << "default locale " << uloc_getDefault() << ".";
    return "en-US";
  }

  std::string language = buffer;
  uloc_getCountry(NULL, buffer, arraysize(buffer), &icu_result);
  if (U_SUCCESS(icu_result) && buffer[0]) {
    language += "-";
    language += buffer;
  }

  // We should end up with something like "en" or "en-US".
  return language;
}

std::string GetSystemLanguageScript() {
  char buffer[ULOC_LANG_CAPACITY];
  UErrorCode icu_result = U_ZERO_ERROR;

  // Combine the ISO language and script.
  uloc_getLanguage(NULL, buffer, arraysize(buffer), &icu_result);
  if (!U_SUCCESS(icu_result)) {
    DLOG(FATAL) << __FUNCTION__ << ": Unable to get language from ICU for "
                << "default locale " << uloc_getDefault() << ".";
    return "en";
  }

  std::string language = buffer;
  uloc_getScript(NULL, buffer, arraysize(buffer), &icu_result);
  if (U_SUCCESS(icu_result) && buffer[0]) {
    language += "-";
    language += buffer;
  }

  // We should end up with something like "en" or "en-Latn".
  return language;
}

}  // namespace base
