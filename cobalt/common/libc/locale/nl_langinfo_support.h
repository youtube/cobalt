// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License-for-dev at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_COMMON_LIBC_LOCALE_NL_LANGINFO_SUPPORT_H_
#define COBALT_COMMON_LIBC_LOCALE_NL_LANGINFO_SUPPORT_H_

#include <langinfo.h>

#include "cobalt/common/libc/locale/lconv_support.h"
#include "cobalt/common/libc/locale/locale_support.h"

namespace cobalt {

// Enum types for GetLocalizedDateSymbol to easily distinguish which nl_item we
// should be retrieving.
enum class TimeNameType { kDay, kAbbrevDay, kMonth, kAbbrevMonth, kAmPm };

// Will retrieve the corresponding locale data in relation to the types in
// TimeNameType. This includes |DAY*|, |ABDAY*|, |AM/PM_STR|, |MON*|, |ABMON*|.
std::string GetLocalizedDateSymbol(const std::string& locale,
                                   TimeNameType type,
                                   int index);

// Will retrieve the corresponding Numeric data for a given locale. This mainly
// supports the |RADIXCHAR| and |THOUSEP| nl_items.
std::string NlGetNumericData(const std::string& locale, const nl_item& type);

}  //  namespace cobalt

#endif  // COBALT_COMMON_LIBC_LOCALE_NL_LANGINFO_SUPPORT_H_
