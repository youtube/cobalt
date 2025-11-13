// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License-for-dev at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/common/libc/locale/locale.h"

#include <langinfo.h>
#include <limits.h>
#include <locale.h>
#include <string.h>
// #include <stdlib.h>

#include <cstddef>
#include <string>
#include <vector>

#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/ucnv.h"

namespace {
constexpr int MAX_LC_ID = 32;
struct PrivateLocale {
  std::string categories[MAX_LC_ID];
  PrivateLocale() {
    categories[LC_CTYPE] = "C";
    categories[LC_NUMERIC] = "C";
    categories[LC_TIME] = "C";
    categories[LC_COLLATE] = "C";
    categories[LC_MONETARY] = "C";
    categories[LC_ALL] = "C";

// POSIX / GNU Extensions (Guarded)
#ifdef LC_MESSAGES
    categories[LC_MESSAGES] = "C";
#endif

#ifdef LC_PAPER
    categories[LC_PAPER] = "C";
#endif

#ifdef LC_NAME
    categories[LC_NAME] = "C";
#endif

#ifdef LC_ADDRESS
    categories[LC_ADDRESS] = "C";
#endif

#ifdef LC_TELEPHONE
    categories[LC_TELEPHONE] = "C";
#endif

#ifdef LC_MEASUREMENT
    categories[LC_MEASUREMENT] = "C";
#endif

#ifdef LC_IDENTIFICATION
    categories[LC_IDENTIFICATION] = "C";
#endif
  }
};

PrivateLocale g_current_locale;

// TODO: This shouldn't be initialized until uselocale is called. Maybe set to
// LC_GLOBAL???
// thread_local locale_t g_current_locale = reinterpret_cast<locale_t>(0);

// thread_local PrivateLocale* g_current_thread_locale = &g_current_locale;

// The default locale is the C locale.
const lconv* GetCLocaleConv() {
  static const lconv c_locale_conv = {
      .decimal_point = const_cast<char*>("."),
      .thousands_sep = const_cast<char*>(""),
      .grouping = const_cast<char*>(""),
      .int_curr_symbol = const_cast<char*>(""),
      .currency_symbol = const_cast<char*>(""),
      .mon_decimal_point = const_cast<char*>(""),
      .mon_thousands_sep = const_cast<char*>(""),
      .mon_grouping = const_cast<char*>(""),
      .positive_sign = const_cast<char*>(""),
      .negative_sign = const_cast<char*>(""),
      .int_frac_digits = CHAR_MAX,
      .frac_digits = CHAR_MAX,
      .p_cs_precedes = CHAR_MAX,
      .p_sep_by_space = CHAR_MAX,
      .n_cs_precedes = CHAR_MAX,
      .n_sep_by_space = CHAR_MAX,
      .p_sign_posn = CHAR_MAX,
      .n_sign_posn = CHAR_MAX,
      .int_p_cs_precedes = CHAR_MAX,
      .int_p_sep_by_space = CHAR_MAX,
      .int_n_cs_precedes = CHAR_MAX,
      .int_n_sep_by_space = CHAR_MAX,
      .int_p_sign_posn = CHAR_MAX,
      .int_n_sign_posn = CHAR_MAX,
  };
  return &c_locale_conv;
}

// The C locale can be referenced by this statically allocated object.
const lconv* GetCLocale() {
  static const lconv c_locale(*GetCLocaleConv());
  return &c_locale;
}

bool is_valid_category(int category) {
  switch (category) {
    case LC_ALL:
    case LC_COLLATE:
    case LC_CTYPE:
    case LC_MONETARY:
    case LC_NUMERIC:
    case LC_TIME:
// Include POSIX/GNU extensions if they are defined on your system
#ifdef LC_MESSAGES
    case LC_MESSAGES:
#endif
#ifdef LC_PAPER
    case LC_PAPER:
#endif
#ifdef LC_NAME
    case LC_NAME:
#endif
#ifdef LC_ADDRESS
    case LC_ADDRESS:
#endif
#ifdef LC_TELEPHONE
    case LC_TELEPHONE:
#endif
#ifdef LC_MEASUREMENT
    case LC_MEASUREMENT:
#endif
#ifdef LC_IDENTIFICATION
    case LC_IDENTIFICATION:
#endif
      return true;
    default:
      return false;
  }
}

std::string getCanonicalLocale(const char* inputLocale) {
  // 1. Create a Locale object to canonicalize the input string.
  icu::Locale locale_to_check(inputLocale);
  const char* canonical_name = locale_to_check.getName();

  // Handle malformed input that results in an empty canonical name.
  if (strcmp(canonical_name, "") == 0 && strcmp(inputLocale, "") != 0) {
    return "";
  }

  // 2. Get the list of all available locales from ICU.
  int32_t count = 0;
  const icu::Locale* available_locales =
      icu::Locale::getAvailableLocales(count);

  if (available_locales == nullptr) {
    return "";
  }

  // 3. Find a match for our canonical name in the official list.
  for (int32_t i = 0; i < count; ++i) {
    if (strcmp(canonical_name, available_locales[i].getName()) == 0) {
      // Match found. Return the pointer to the official, canonical string.
      return available_locales[i].getName();
    }
  }

  // 4. No match was found.
  return "";
}

std::string getCanonicalCodeset(const char* encodingName) {
  UErrorCode status = U_ZERO_ERROR;
  UConverter* conv = ucnv_open(encodingName, &status);

  if (U_FAILURE(status)) {
    return "";
  }

  const char* canonicalName = ucnv_getName(conv, &status);

  if (U_FAILURE(status)) {
    ucnv_close(conv);
    return "";
  }

  std::string result(canonicalName);

  ucnv_close(conv);

  return result;
}

}  // namespace

// The POSIX setlocale is not hermetic, so we must provide our own
// implementation.

// Check if locale is set, if not, set it
// check if the given locale is legal, if it is, set it
//
char* setlocale(int category, const char* locale) {
  if (!is_valid_category(category)) {
    return nullptr;
  }

  if (locale == nullptr) {
    return const_cast<char*>(g_current_locale.categories[category].c_str());
  }

  // TODO: Change this.
  if (strcmp(locale, "") == 0) {
    return const_cast<char*>("C");
  }

  std::string canonical_locale;

  if (strcmp(locale, "C") == 0 || strcmp(locale, "POSIX") == 0) {
    canonical_locale = locale;
  }

  else {
    // with special locales out of the way, we now need to ensure that the given
    // locale is valid and that its string is normalized.
    std::string newLocale(locale);

    std::string language, codeset;

    size_t separatorPosition = newLocale.find(".");

    if (separatorPosition != std::string::npos) {
      language = newLocale.substr(0, separatorPosition);
      codeset = newLocale.substr(separatorPosition + 1);
    } else {
      language = newLocale;
    }

    std::string canonical_language = getCanonicalLocale(language.c_str());
    std::string canonical_codeset = getCanonicalCodeset(codeset.c_str());

    if ((separatorPosition != std::string::npos)) {
      if (canonical_language == "" || canonical_codeset == "") {
        return nullptr;
      }
      canonical_locale = canonical_language + "." + canonical_codeset;
    } else {
      if (canonical_language == "") {
        return nullptr;
      }
      canonical_locale = canonical_language;
    }
  }
  //   // if not empty string, we:
  //   // 1. check if string is validd
  //   // 2. If cat is LC_ALL, set all categories to this string value, return
  //   // 3. set category to string
  //   // 4. if LC_ALL is set and the new locale for the category isn't set, we
  //   set LC_ALL to empty string, return

  //   //TODO: check that the given locale is supported

  if (category == LC_ALL) {
    for (int i = 0; i < MAX_LC_ID; ++i) {
      g_current_locale.categories[i] = canonical_locale;
    }
    return const_cast<char*>(g_current_locale.categories[category].c_str());
  }

  g_current_locale.categories[category] = canonical_locale;

  if (strcmp(g_current_locale.categories[category].c_str(),
             g_current_locale.categories[LC_ALL].c_str()) != 0) {
    g_current_locale.categories[LC_ALL] = "";
  }

  return const_cast<char*>("Test string");
}

// TODO: Make sure that locale_t does NOT point to an lconv. Point to new
// structure in point
locale_t newlocale(int category_mask, const char* locale, locale_t base) {
  if (locale == nullptr ||
      (strcmp(locale, "C") != 0 && strcmp(locale, "") != 0)) {
    return (locale_t)0;
  }

  lconv* new_lconv = new lconv;
  if (base != (locale_t)0) {
    memcpy(new_lconv, reinterpret_cast<lconv*>(base), sizeof(lconv));
    freelocale(base);
  } else {
    memcpy(new_lconv, GetCLocale(), sizeof(lconv));
  }
  return reinterpret_cast<locale_t>(new_lconv);
}

locale_t uselocale(locale_t newloc) {
  // TODO: b/403007005 Fill in this stub.
  return reinterpret_cast<locale_t>(&g_current_locale);
  // return reinterpret_cast<locale_t>(g_current_thread_locale);
}

void freelocale(locale_t loc) {
  // if (loc) {
  //   delete reinterpret_cast<lconv*>(loc);
  // }
}

struct lconv* localeconv(void) {
  return const_cast<struct lconv*>(GetCLocaleConv());
}

locale_t duplocale(locale_t loc) {
  if (loc == LC_GLOBAL_LOCALE) {
    return LC_GLOBAL_LOCALE;
  }
  if (loc == (locale_t)0) {
    return (locale_t)0;
  }
  lconv* new_lconv = new lconv;
  memcpy(new_lconv, reinterpret_cast<lconv*>(loc), sizeof(lconv));
  return reinterpret_cast<locale_t>(new_lconv);
}

char* nl_langinfo_l(nl_item item, locale_t locale) {
  if (locale == (locale_t)0) {
    // The behavior is undefined according to POSIX, but we will follow the
    // behavior of glibc and return an empty string.
    return const_cast<char*>("");
  }

  switch (item) {
    // Date and time formats
    case D_T_FMT:
      return const_cast<char*>("%a %b %e %H:%M:%S %Y");
    case D_FMT:
      return const_cast<char*>("%m/%d/%y");
    case T_FMT:
      return const_cast<char*>("%H:%M:%S");
    case T_FMT_AMPM:
      return const_cast<char*>("%I:%M:%S %p");
    case AM_STR:
      return const_cast<char*>("AM");
    case PM_STR:
      return const_cast<char*>("PM");

    // Days
    case DAY_1:
      return const_cast<char*>("Sunday");
    case DAY_2:
      return const_cast<char*>("Monday");
    case DAY_3:
      return const_cast<char*>("Tuesday");
    case DAY_4:
      return const_cast<char*>("Wednesday");
    case DAY_5:
      return const_cast<char*>("Thursday");
    case DAY_6:
      return const_cast<char*>("Friday");
    case DAY_7:
      return const_cast<char*>("Saturday");

    // Abbreviated days
    case ABDAY_1:
      return const_cast<char*>("Sun");
    case ABDAY_2:
      return const_cast<char*>("Mon");
    case ABDAY_3:
      return const_cast<char*>("Tue");
    case ABDAY_4:
      return const_cast<char*>("Wed");
    case ABDAY_5:
      return const_cast<char*>("Thu");
    case ABDAY_6:
      return const_cast<char*>("Fri");
    case ABDAY_7:
      return const_cast<char*>("Sat");

    // Months
    case MON_1:
      return const_cast<char*>("January");
    case MON_2:
      return const_cast<char*>("February");
    case MON_3:
      return const_cast<char*>("March");
    case MON_4:
      return const_cast<char*>("April");
    case MON_5:
      return const_cast<char*>("May");
    case MON_6:
      return const_cast<char*>("June");
    case MON_7:
      return const_cast<char*>("July");
    case MON_8:
      return const_cast<char*>("August");
    case MON_9:
      return const_cast<char*>("September");
    case MON_10:
      return const_cast<char*>("October");
    case MON_11:
      return const_cast<char*>("November");
    case MON_12:
      return const_cast<char*>("December");

    // Abbreviated months
    case ABMON_1:
      return const_cast<char*>("Jan");
    case ABMON_2:
      return const_cast<char*>("Feb");
    case ABMON_3:
      return const_cast<char*>("Mar");
    case ABMON_4:
      return const_cast<char*>("Apr");
    case ABMON_5:
      return const_cast<char*>("May");
    case ABMON_6:
      return const_cast<char*>("Jun");
    case ABMON_7:
      return const_cast<char*>("Jul");
    case ABMON_8:
      return const_cast<char*>("Aug");
    case ABMON_9:
      return const_cast<char*>("Sep");
    case ABMON_10:
      return const_cast<char*>("Oct");
    case ABMON_11:
      return const_cast<char*>("Nov");
    case ABMON_12:
      return const_cast<char*>("Dec");

    // Other
    case CODESET:
      return const_cast<char*>("US-ASCII");
    case YESEXPR:
      return const_cast<char*>("^[yY]");
    case NOEXPR:
      return const_cast<char*>("^[nN]");

    // Some other values from POSIX
    case ERA:
    case ERA_D_FMT:
    case ERA_D_T_FMT:
    case ERA_T_FMT:
    case ALT_DIGITS:
      return const_cast<char*>("");

    default:
      return const_cast<char*>("");
  }
}

char* nl_langinfo(nl_item item) {
  return nl_langinfo_l(item, reinterpret_cast<locale_t>(&g_current_locale));
}
