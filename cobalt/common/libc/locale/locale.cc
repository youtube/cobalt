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

#include <sstream>
#include <string>
#include <vector>

#include "cobalt/common/libc/locale/locale_support.h"

namespace {

thread_local PrivateLocale* g_current_thread_locale =
    (PrivateLocale*)LC_GLOBAL_LOCALE;

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

constexpr int kAllValidCategoriesMask =
    LC_CTYPE_MASK | LC_NUMERIC_MASK | LC_TIME_MASK | LC_COLLATE_MASK |
    LC_MONETARY_MASK | LC_MESSAGES_MASK | LC_ALL_MASK;

}  // namespace

char* setlocale(int category, const char* locale) {
  if (!IsValidCategory(category)) {
    return nullptr;
  }

  if (locale == nullptr) {
    if (category == LC_ALL) {
      return const_cast<char*>(GetGlobalLocale()->composite_lc_all.c_str());
    }
    return const_cast<char*>(GetGlobalLocale()->categories[category].c_str());
  }

  // 3. PREPARE TRANSACTION
  std::vector<std::string> new_categories(LC_ALL);
  for (int i = 0; i < LC_ALL; ++i) {
    new_categories[i] = GetGlobalLocale()->categories[i];
  }

  bool success = false;

  // 4. RESOLVE LOGIC (Same as before)
  if (category == LC_ALL) {
    if (strcmp(locale, "") == 0) {
      // Case A: Env Vars
      const char* env = getenv("LANG");
      std::string canon = GetCanonicalLocale(env ? env : "C");
      if (!canon.empty()) {
        for (int i = 0; i < LC_ALL; ++i) {
          new_categories[i] = canon;
        }
        success = true;
      }
    } else if (ParseCompositeLocale(locale, *GetGlobalLocale(),
                                    new_categories)) {
      // Case B: Composite String (This now handles ALL round-trip restores)
      success = true;
    } else {
      // Case C: Simple String ("en_US")
      std::string canon = GetCanonicalLocale(locale);
      if (!canon.empty()) {
        for (int i = 0; i < LC_ALL; ++i) {
          new_categories[i] = canon;
        }
        success = true;
      }
    }
  } else {
    // Case D: Specific Category
    const char* target = locale;
    if (strcmp(locale, "") == 0) {
      const char* env = getenv("LANG");
      target = (env && env[0]) ? env : "C";
    }
    std::string canon = GetCanonicalLocale(target);
    if (!canon.empty()) {
      new_categories[category] = canon;
      success = true;
    }
  }

  if (!success) {
    return nullptr;
  }

  // 5. COMMIT STATE
  for (int i = 0; i < LC_ALL; ++i) {
    GetGlobalLocale()->categories[i] = new_categories[i];
  }

  // -------------------------------------------------------------------------
  // 6. UPDATE COMPOSITE STRING (SIMPLIFIED)
  // We ALWAYS generate the key-value list. No more "is_uniform" check.
  // Result is always: "LC_CTYPE=val;LC_NUMERIC=val;..."
  // -------------------------------------------------------------------------
  bool is_uniform = true;
  std::string first = GetGlobalLocale()->categories[0];

  // Check if every category matches the first one
  for (int i = 1; i < LC_ALL; ++i) {
    if (GetGlobalLocale()->categories[i] != first) {
      is_uniform = false;
      break;
    }
  }

  if (is_uniform) {
    // OPTIMIZATION: If all are "C", string is "C".
    // If all are "en_US.UTF-8", string is "en_US.UTF-8".
    GetGlobalLocale()->composite_lc_all = first;
  } else {
    // MIXED STATE: Generate standard Key-Value list.
    // Format: LC_CTYPE=val;LC_NUMERIC=val;...
    std::stringstream ss;
    for (int i = 0; i < LC_ALL; ++i) {
      if (i > 0) {
        ss << ";";
      }
      ss << GetCategoryName(i) << "=" << GetGlobalLocale()->categories[i];
    }
    GetGlobalLocale()->composite_lc_all = ss.str();
  }
  // -------------------------------------------------------------------------

  // 7. RETURN RESULT
  if (category == LC_ALL) {
    return const_cast<char*>(GetGlobalLocale()->composite_lc_all.c_str());
  }
  return const_cast<char*>(GetGlobalLocale()->categories[category].c_str());
}

locale_t newlocale(int category_mask, const char* locale, locale_t base) {
  if ((category_mask & ~kAllValidCategoriesMask) != 0 || locale == NULL) {
    errno = EINVAL;
    return (locale_t)0;
  }

  std::string canonical_locale;
  if (strcmp(locale, "") == 0) {
    canonical_locale = GetCanonicalLocale(getenv("LANG"));
  } else {
    canonical_locale = GetCanonicalLocale(locale);
  }

  if (canonical_locale == "") {
    errno = ENOENT;
    return (locale_t)0;
  }

  PrivateLocale* cur_locale;

  if (base == (locale_t)0) {
    cur_locale = new PrivateLocale();
  } else {
    cur_locale = (PrivateLocale*)base;
  }

  UpdateLocaleSettings(category_mask, canonical_locale.c_str(), cur_locale);

  return (locale_t)cur_locale;
}

locale_t uselocale(locale_t newloc) {
  if (newloc == (locale_t)0) {
    return (locale_t)g_current_thread_locale;
  }

  if (newloc == (locale_t)LC_GLOBAL_LOCALE) {
    PrivateLocale* return_locale = g_current_thread_locale;
    g_current_thread_locale = (PrivateLocale*)LC_GLOBAL_LOCALE;
    return (locale_t)return_locale;
  }

  PrivateLocale* return_locale = g_current_thread_locale;
  g_current_thread_locale = (PrivateLocale*)newloc;

  return (locale_t)return_locale;
}

void freelocale(locale_t loc) {
  delete (PrivateLocale*)loc;
}

locale_t duplocale(locale_t loc) {
  if (loc == LC_GLOBAL_LOCALE) {
    PrivateLocale* global_copy = new PrivateLocale();
    for (int i = 0; i < LC_ALL; ++i) {
      global_copy->categories[i] = GetGlobalLocale()->categories[i];
    }
    return (locale_t)global_copy;
  }

  if (loc == (locale_t)0) {
    return (locale_t)0;
  }

  PrivateLocale* new_copy = new PrivateLocale();
  for (int i = 0; i < LC_ALL; ++i) {
    new_copy->categories[i] = ((PrivateLocale*)loc)->categories[i];
  }
  return (locale_t)new_copy;
}

struct lconv* localeconv(void) {
  // TODO: b/461906423 - Properly implement localeconv.
  return const_cast<lconv*>(GetCLocaleConv());
}

char* nl_langinfo_l(nl_item item, locale_t locale) {
  // TODO: b/461906423 - Properly implement nl_langinfo_l.
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
  return nl_langinfo_l(item, reinterpret_cast<locale_t>(GetGlobalLocale()));
}
