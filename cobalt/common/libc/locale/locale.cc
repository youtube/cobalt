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

#include <errno.h>
#include <langinfo.h>
#include <limits.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "cobalt/common/libc/locale/lconv_support.h"
#include "cobalt/common/libc/locale/locale_support.h"
#include "cobalt/common/libc/locale/nl_langinfo_support.h"
#include "cobalt/common/libc/no_destructor.h"
#include "starboard/common/log.h"

namespace {
constexpr char kLangEnv[] = "LANG";

using cobalt::common::libc::NoDestructor;
cobalt::LocaleImpl* GetGlobalLocale() {
  static NoDestructor<cobalt::LocaleImpl> g_current_locale;
  return g_current_locale.get();
}

cobalt::LconvImpl* GetGlobalLconv() {
  static NoDestructor<cobalt::LconvImpl> g_current_lconv;
  return g_current_lconv.get();
}

std::string& GetNlLangInfoBuffer() {
  thread_local NoDestructor<std::string> buffer;
  return *buffer;
}

thread_local cobalt::LocaleImpl* g_current_thread_locale =
    reinterpret_cast<cobalt::LocaleImpl*>(LC_GLOBAL_LOCALE);

}  // namespace

char* setlocale(int category, const char* locale) {
  if (!cobalt::IsValidLcCategory(category)) {
    return nullptr;
  }

  cobalt::LocaleImpl* global_locale = GetGlobalLocale();
  if (locale == nullptr) {
    if (category == LC_ALL) {
      return const_cast<char*>(global_locale->composite_lc_all.c_str());
    }
    return const_cast<char*>(global_locale->categories[category].c_str());
  }

  if (category != LC_ALL) {
    std::string source_locale;
    if (strcmp(locale, "") == 0) {
      const char* env_locale = getenv(kLangEnv);
      if (env_locale == nullptr || env_locale[0] == '\0') {
        env_locale = cobalt::kCLocale;
      }
      source_locale = cobalt::GetCanonicalLocale(env_locale);
    } else {
      source_locale = cobalt::GetCanonicalLocale(locale);
    }

    if (source_locale.empty()) {
      return nullptr;
    }

    global_locale->categories[category] = source_locale;

    // We still need to check uniformity to keep LC_ALL return value correct.
    RefreshCompositeString(global_locale);

    // We sync ICU's default locale to the value of |LC_MESSAGES|, as
    // LC_MESSAGES corresponds to how text is displayed.
    if (category == LC_MESSAGES) {
      cobalt::SyncIcuDefault(source_locale);
    }

    return const_cast<char*>(global_locale->categories[category].c_str());
  }

  std::array<std::string, LC_ALL> new_categories = global_locale->categories;
  bool success = false;
  // Special case where the locale string is the composite locale created from
  // setlocale(LC_ALL, NULL);
  if (ParseCompositeLocale(locale, *global_locale, new_categories)) {
    success = true;
  } else {
    std::string canonical_locale;
    if (strcmp(locale, "") == 0) {
      const char* env_locale = getenv(kLangEnv);
      if (env_locale == nullptr || env_locale[0] == '\0') {
        env_locale = cobalt::kCLocale;
      }
      canonical_locale = cobalt::GetCanonicalLocale(env_locale);
    } else {
      canonical_locale = cobalt::GetCanonicalLocale(locale);
    }
    if (!canonical_locale.empty()) {
      new_categories.fill(canonical_locale);
      success = true;
    }
  }

  if (!success) {
    return nullptr;
  }

  global_locale->categories = new_categories;

  // With the locales updated, we must update our composite string so that
  // setlocale(LC_ALL, NULL) is accurate.
  cobalt::RefreshCompositeString(global_locale);
  cobalt::SyncIcuDefault(global_locale->categories[LC_MESSAGES]);

  return const_cast<char*>(global_locale->composite_lc_all.c_str());
}

locale_t newlocale(int category_mask, const char* locale, locale_t base) {
  int effective_mask = (category_mask == LC_ALL_MASK)
                           ? cobalt::kAllValidCategoriesMask
                           : category_mask;

  if (locale == nullptr ||
      (effective_mask & ~cobalt::kAllValidCategoriesMask) != 0) {
    errno = EINVAL;
    return (locale_t)0;
  }

  std::string canonical_locale;
  if (strcmp(locale, "") == 0) {
    const char* env_locale = getenv(kLangEnv);
    if (env_locale == nullptr || env_locale[0] == '\0') {
      env_locale = cobalt::kCLocale;
    }
    canonical_locale = cobalt::GetCanonicalLocale(env_locale);
  } else {
    canonical_locale = cobalt::GetCanonicalLocale(locale);
  }

  if (canonical_locale == "") {
    errno = ENOENT;
    return (locale_t)0;
  }

  cobalt::LocaleImpl* cur_locale;
  if (base == (locale_t)0) {
    cur_locale = new cobalt::LocaleImpl();
  } else {
    cur_locale = reinterpret_cast<cobalt::LocaleImpl*>(base);
  }

  UpdateLocaleSettings(effective_mask, canonical_locale.c_str(), cur_locale);
  RefreshCompositeString(cur_locale);

  return reinterpret_cast<locale_t>(cur_locale);
}

locale_t uselocale(locale_t newloc) {
  if (newloc == (locale_t)0) {
    return reinterpret_cast<locale_t>(g_current_thread_locale);
  }

  cobalt::LocaleImpl* return_locale = g_current_thread_locale;
  if (newloc == (locale_t)LC_GLOBAL_LOCALE) {
    g_current_thread_locale =
        reinterpret_cast<cobalt::LocaleImpl*>(LC_GLOBAL_LOCALE);
    return reinterpret_cast<locale_t>(return_locale);
  }

  g_current_thread_locale = reinterpret_cast<cobalt::LocaleImpl*>(newloc);
  return reinterpret_cast<locale_t>(return_locale);
}

void freelocale(locale_t loc) {
  delete reinterpret_cast<cobalt::LocaleImpl*>(loc);
}

locale_t duplocale(locale_t loc) {
  if (loc == (locale_t)0) {
    return (locale_t)0;
  }

  const cobalt::LocaleImpl* original_loc =
      (loc == LC_GLOBAL_LOCALE)
          ? GetGlobalLocale()
          : reinterpret_cast<const cobalt::LocaleImpl*>(loc);

  cobalt::LocaleImpl* new_copy = new cobalt::LocaleImpl();
  new_copy->categories = original_loc->categories;
  new_copy->composite_lc_all = original_loc->composite_lc_all;
  return reinterpret_cast<locale_t>(new_copy);
}

struct lconv* localeconv(void) {
  cobalt::LconvImpl* current_lconv = GetGlobalLconv();
  cobalt::LocaleImpl* current_loc;
  if (g_current_thread_locale !=
      reinterpret_cast<cobalt::LocaleImpl*> LC_GLOBAL_LOCALE) {
    current_loc = g_current_thread_locale;
  } else {
    current_loc = GetGlobalLocale();
  }

  std::string numeric_locale = current_loc->categories[LC_NUMERIC];
  std::string monetary_locale = current_loc->categories[LC_MONETARY];

  if ((numeric_locale == cobalt::kCLocale ||
       numeric_locale == cobalt::kPosixLocale)) {
    current_lconv->ResetNumericToC();
  } else {
    if (!cobalt::UpdateNumericLconv(numeric_locale, current_lconv)) {
      SB_LOG(WARNING)
          << "Failed to properly retrieve the updated lconv numeric values. "
             "Returning the C lconv numeric values.";
    }
  }

  if ((monetary_locale == cobalt::kCLocale ||
       monetary_locale == cobalt::kPosixLocale)) {
    current_lconv->ResetMonetaryToC();
  } else {
    if (!cobalt::UpdateMonetaryLconv(monetary_locale, current_lconv)) {
      SB_LOG(WARNING)
          << "Failed to properly retrieve the updated lconv monetary values. "
             "Returning the C lconv monetary values.";
    }
  }

  return &(current_lconv->result);
}

char* nl_langinfo_l(nl_item item, locale_t locale) {
  if (locale == (locale_t)0) {
    // The behavior is undefined according to POSIX, but we will follow the
    // behavior of glibc and return an empty string.
    return const_cast<char*>("");
  }

  cobalt::LocaleImpl* cur_locale;
  if (locale == LC_GLOBAL_LOCALE) {
    cur_locale = GetGlobalLocale();
  } else {
    cur_locale = reinterpret_cast<cobalt::LocaleImpl*>(locale);
  }

  std::string& langinfo_buffer = GetNlLangInfoBuffer();
  switch (item) {
    // Date and time formats
    case D_T_FMT:
    case D_FMT:
    case T_FMT:
    case T_FMT_AMPM:
      langinfo_buffer =
          cobalt::GetPosixPattern(cur_locale->categories[LC_TIME], item);
      break;
    case AM_STR:
    case PM_STR:
      langinfo_buffer = cobalt::GetLocalizedDateSymbol(
          cur_locale->categories[LC_TIME], cobalt::TimeNameType::kAmPm,
          item - AM_STR);
      break;

    // Days
    case DAY_1:
    case DAY_2:
    case DAY_3:
    case DAY_4:
    case DAY_5:
    case DAY_6:
    case DAY_7:
      langinfo_buffer = cobalt::GetLocalizedDateSymbol(
          cur_locale->categories[LC_TIME], cobalt::TimeNameType::kDay,
          item - DAY_1);
      break;

    // Abbreviated days
    case ABDAY_1:
    case ABDAY_2:
    case ABDAY_3:
    case ABDAY_4:
    case ABDAY_5:
    case ABDAY_6:
    case ABDAY_7:
      langinfo_buffer = cobalt::GetLocalizedDateSymbol(
          cur_locale->categories[LC_TIME], cobalt::TimeNameType::kAbbrevDay,
          item - ABDAY_1);
      break;

    // Months
    case MON_1:
    case MON_2:
    case MON_3:
    case MON_4:
    case MON_5:
    case MON_6:
    case MON_7:
    case MON_8:
    case MON_9:
    case MON_10:
    case MON_11:
    case MON_12:
      langinfo_buffer = cobalt::GetLocalizedDateSymbol(
          cur_locale->categories[LC_TIME], cobalt::TimeNameType::kMonth,
          item - MON_1);
      break;

    // Abbreviated months
    case ABMON_1:
    case ABMON_2:
    case ABMON_3:
    case ABMON_4:
    case ABMON_5:
    case ABMON_6:
    case ABMON_7:
    case ABMON_8:
    case ABMON_9:
    case ABMON_10:
    case ABMON_11:
    case ABMON_12:
      langinfo_buffer = cobalt::GetLocalizedDateSymbol(
          cur_locale->categories[LC_TIME], cobalt::TimeNameType::kAbbrevMonth,
          item - ABMON_1);
      break;
    case RADIXCHAR:
    case THOUSEP:
      langinfo_buffer =
          cobalt::NlGetNumericData(cur_locale->categories[LC_NUMERIC], item);
      break;
    // Our ICU build configuration only supports "UTF-8", so |CODESET|
    // is set to always return "UTF-8".
    case CODESET:
      langinfo_buffer = "UTF-8";
      break;
    // ICU has deprecated support for |YESEXPR| and |NOEXPR|. If these items are
    // ever requested, we return the default "C" locale expressions.
    case YESEXPR:
      SB_LOG(WARNING) << "nl_item YESEXPR was requested, but is not supported "
                         "by ICU. Returning ^[yY].";
      langinfo_buffer = "^[yY]";
      break;
    case NOEXPR:
      SB_LOG(WARNING) << "nl_item NOEXPR was requested, but is not supported "
                         "by ICU. Returning ^[nN].";
      langinfo_buffer = "^[nN]";
      break;

    // For the following items, we currently do not implement them. If they are
    // ever called, |SB_NOTIMPLEMENTED()| is called. See b/466160361 for more
    // info.
    case CRNCYSTR:
      SB_NOTIMPLEMENTED()
          << "CRNCYSTR is not supported. Returning the empty string.";
      langinfo_buffer = "";
      break;
    case ERA:
    case ERA_D_FMT:
    case ERA_D_T_FMT:
    case ERA_T_FMT:
      SB_NOTIMPLEMENTED() << "ERA* items are currently not supported. "
                             "Returning the empty string.";
      langinfo_buffer = "";
      break;
    case ALT_DIGITS:
      SB_NOTIMPLEMENTED() << "ALT_DIGITS is currently not supported. Returning "
                             "the empty string.";
      langinfo_buffer = "";
      break;
    default:
      SB_LOG(ERROR) << "Received unknown nl_item for nl_langinfo. Returning "
                       "the empty string.";
      langinfo_buffer = "";
      break;
  }
  return const_cast<char*>(langinfo_buffer.c_str());
}

char* nl_langinfo(nl_item item) {
  return nl_langinfo_l(item,
                       reinterpret_cast<locale_t>(g_current_thread_locale));
}
