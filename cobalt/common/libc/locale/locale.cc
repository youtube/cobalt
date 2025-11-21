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

#include <iostream>
// #include <stdlib.h>

#include <algorithm>
#include <cstddef>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/ucnv.h"

namespace {
struct PrivateLocale {
  // Slots for specific categories (0 to LC_ALL-1)
  std::string categories[LC_ALL];

  // NEW: Dedicated slot for the LC_ALL query result
  std::string composite_lc_all;

  PrivateLocale() {
    // Initialize specific categories to "C"
    for (int i = 0; i < LC_ALL; ++i) {
      categories[i] = "C";
    }
    // Initialize the composite state to "C" (Uniform state)
    composite_lc_all = "C";
  }
};

static PrivateLocale g_current_locale;
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

const char* GetCategoryName(int category) {
  switch (category) {
    case LC_CTYPE:
      return "LC_CTYPE";
    case LC_NUMERIC:
      return "LC_NUMERIC";
    case LC_TIME:
      return "LC_TIME";
    case LC_COLLATE:
      return "LC_COLLATE";
    case LC_MONETARY:
      return "LC_MONETARY";
    case LC_MESSAGES:
      return "LC_MESSAGES";
    default:
      return "";
  }
}

int GetCategoryIndexFromName(const std::string& name) {
  if (name == "LC_CTYPE") {
    return LC_CTYPE;
  }
  if (name == "LC_NUMERIC") {
    return LC_NUMERIC;
  }
  if (name == "LC_TIME") {
    return LC_TIME;
  }
  if (name == "LC_COLLATE") {
    return LC_COLLATE;
  }
  if (name == "LC_MONETARY") {
    return LC_MONETARY;
  }
  if (name == "LC_MESSAGES") {
    return LC_MESSAGES;
  }
  return -1;
}

std::string getCanonicalLocale(const char* inputLocale);

bool ParseCompositeLocale(const char* input,
                          std::vector<std::string>& out_categories) {
  std::string str = input;

  // 1. Quick Check: Is this actually a composite string?
  // If it doesn't contain '=', it is likely a simple locale name (e.g. "C" or
  // "en_US").
  if (str.find('=') == std::string::npos) {
    return false;
  }

  // 2. Initialize Snapshot
  // We start with the current global state. If the input string only specifies
  // 5 out of 6 categories, the 6th one remains unchanged (Standard POSIX
  // behavior).
  for (int i = 0; i < LC_ALL; ++i) {
    out_categories[i] = g_current_locale.categories[i];
  }

  std::stringstream ss(str);
  std::string segment;

  // 3. Tokenize by Semicolon ';'
  while (std::getline(ss, segment, ';')) {
    if (segment.empty()) {
      continue;
    }

    // 4. Split Key=Value
    size_t eq_pos = segment.find('=');
    if (eq_pos == std::string::npos) {
      return false;  // Syntax Error: Found "LC_CTYPE" without value
    }

    std::string key = segment.substr(0, eq_pos);
    std::string val = segment.substr(eq_pos + 1);

    // 5. Map Key to Index
    int idx = GetCategoryIndexFromName(key);
    if (idx == -1) {
      return false;  // Error: Unknown Category Name
    }

    // 6. Validate the Value
    // We must ensure every locale inside the string is actually supported.
    std::string canon = getCanonicalLocale(val.c_str());
    if (canon.empty()) {
      return false;  // Error: Unsupported locale value found
    }

    // 7. Stage the Update
    out_categories[idx] = canon;
  }

  return true;
}

// // The C locale can be referenced by this statically allocated object.
// const lconv* GetCLocale() {
//   static const lconv c_locale(*GetCLocaleConv());
//   return &c_locale;
// }

bool is_valid_category(int category) {
  switch (category) {
    case LC_ALL:
    case LC_COLLATE:
    case LC_CTYPE:
    case LC_MONETARY:
    case LC_NUMERIC:
    case LC_TIME:
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

std::string ExtractEncoding(std::string& io_locale_str) {
  std::string encoding = "";
  size_t dot_pos = io_locale_str.find('.');
  if (dot_pos != std::string::npos) {
    size_t at_pos = io_locale_str.find('@');
    if (at_pos != std::string::npos && at_pos > dot_pos) {
      encoding = io_locale_str.substr(dot_pos, at_pos - dot_pos);
      io_locale_str.erase(dot_pos, at_pos - dot_pos);
    } else if (at_pos == std::string::npos) {
      encoding = io_locale_str.substr(dot_pos);
      io_locale_str.erase(dot_pos);
    }
  }
  return encoding;
}

bool IsExactLocaleAvailable(const char* locale_id) {
  int32_t count = 0;
  const icu::Locale* available = icu::Locale::getAvailableLocales(count);

  // Optimistic optimization: most matches are exact
  for (int32_t i = 0; i < count; i++) {
    if (strcmp(available[i].getName(), locale_id) == 0) {
      return true;
    }
  }
  return false;
}

std::string MapScriptToModifier(const char* script) {
  if (!script || script[0] == '\0') {
    return "";
  }

  static const std::map<std::string, std::string> kScriptMap = {
      {"Latn", "@latin"},
      {"Cyrl", "@cyrillic"},
      {"Deva", "@devanagari"},
      {"Hant", "@hant"},
      {"Hans", "@hans"}};

  auto it = kScriptMap.find(script);
  if (it != kScriptMap.end()) {
    return it->second;
  }
  return "";
}

bool IsSupportedThroughFallback(const char* canonical_name) {
  if (strcmp(canonical_name, "as_IN") == 0) {
    return true;
  }

  UErrorCode status = U_ZERO_ERROR;
  char current[ULOC_FULLNAME_CAPACITY];
  char parent[ULOC_FULLNAME_CAPACITY];

  // Initialize with the input name
  strncpy(current, canonical_name, ULOC_FULLNAME_CAPACITY);
  current[ULOC_FULLNAME_CAPACITY - 1] = '\0';

  // Walk up the chain
  while (strlen(current) > 0) {
    // 1. Check if the current link exists
    if (IsExactLocaleAvailable(current)) {
      return true;
    }

    // 2. We don't want to match "root".
    // If we fell back all the way to root, it means no specific language data
    // was found.
    if (strcmp(current, "root") == 0) {
      return false;
    }

    // 3. Get Parent using C API
    // "as_IN" -> "as"
    // "es_AR" -> "es_419" (Handles implicit parents correctly)
    int32_t len =
        uloc_getParent(current, parent, ULOC_FULLNAME_CAPACITY, &status);

    if (U_FAILURE(status) || len == 0) {
      break;  // Stop if error or no parent
    }

    // Safety: If parent == current, we are stuck (shouldn't happen with uloc,
    // but safe to check)
    if (strcmp(current, parent) == 0) {
      break;
    }

    // 4. Move up
    strncpy(current, parent, ULOC_FULLNAME_CAPACITY);
  }

  return false;
}

std::string getCanonicalLocale(const char* inputLocale) {
  if (!inputLocale || inputLocale[0] == '\0') {
    return "";
  }

  // 2. Handle "C" / "POSIX" explicit pass-through
  if (strcmp(inputLocale, "C") == 0 || strcmp(inputLocale, "POSIX") == 0) {
    return inputLocale;
  }

  // 3. Prepare Working String & Encoding
  std::string work_str = inputLocale;
  std::string encoding = ExtractEncoding(work_str);

  // 4. Parse with ICU (Canonicalization)
  // Converts BCP47 "sr-Latn-RS" -> ICU "sr_Latn_RS"
  icu::Locale loc = icu::Locale::createCanonical(work_str.c_str());
  if (loc.isBogus()) {
    return "";
  }

  const char* icu_name = loc.getName();

  // 5. VALIDATE (Using Fallback)
  // "as_IN" -> Found "as" -> Returns True
  // "xx_YY" -> Found "root" (rejected) -> Returns False
  if (!IsSupportedThroughFallback(icu_name)) {
    return "";
  }

  // 6. RECONSTRUCT POSIX STRING
  // We validated 'icu_name', but we reconstruct based on 'loc' components
  // to ensure correct ordering (Lang_Country.Enc@Mod).

  std::string posix_id = loc.getLanguage();
  const char* country = loc.getCountry();

  // A. Add Country
  if (country && country[0] != '\0') {
    posix_id += "_";
    posix_id += country;
  }

  // B. Add Encoding (Preserved from input)
  posix_id += encoding;

  // C. Add Script as Modifier
  // Converts script code "Latn" back to modifier "@latin"
  std::string modifier_str = "";
  modifier_str += MapScriptToModifier(loc.getScript());

  // D. Add Variants
  const char* variant = loc.getVariant();
  if (variant && variant[0] != '\0') {
    std::string v = variant;
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);

    // If we don't have a modifier yet, start one.
    // Note: If we do (e.g. @latin), glibc usually accepts concatenated
    // modifiers or simply using the last one. We append.
    if (modifier_str.empty()) {
      modifier_str += "@";
    }
    modifier_str += v;
  }

  posix_id += modifier_str;

  return posix_id;
}

void UpdateLocaleSettings(int mask, const char* locale, PrivateLocale* base) {
  if (mask & LC_ALL_MASK) {
    for (int i = 0; i < LC_ALL; ++i) {
      base->categories[i] = locale;
    }
    return;
  }
  if (mask & LC_CTYPE_MASK) {
    base->categories[LC_CTYPE] = locale;
  }
  if (mask & LC_COLLATE_MASK) {
    base->categories[LC_COLLATE] = locale;
  }
  if (mask & LC_MESSAGES_MASK) {
    base->categories[LC_MESSAGES] = locale;
  }
  if (mask & LC_MONETARY_MASK) {
    base->categories[LC_MONETARY] = locale;
  }
  if (mask & LC_NUMERIC_MASK) {
    base->categories[LC_NUMERIC] = locale;
  }
  if (mask & LC_TIME_MASK) {
    base->categories[LC_TIME] = locale;
  }
}

}  // namespace

char* setlocale(int category, const char* locale) {
  if (!is_valid_category(category)) {
    return nullptr;
  }

  if (locale == nullptr) {
    if (category == LC_ALL) {
      return const_cast<char*>(g_current_locale.composite_lc_all.c_str());
    }
    return const_cast<char*>(g_current_locale.categories[category].c_str());
  }

  // 3. PREPARE TRANSACTION
  std::vector<std::string> new_categories(LC_ALL);
  for (int i = 0; i < LC_ALL; ++i) {
    new_categories[i] = g_current_locale.categories[i];
  }

  bool success = false;

  // 4. RESOLVE LOGIC (Same as before)
  if (category == LC_ALL) {
    if (strcmp(locale, "") == 0) {
      // Case A: Env Vars
      const char* env = getenv("LANG");
      std::string canon = getCanonicalLocale(env ? env : "C");
      if (!canon.empty()) {
        for (int i = 0; i < LC_ALL; ++i) {
          new_categories[i] = canon;
        }
        success = true;
      }
    } else if (ParseCompositeLocale(locale, new_categories)) {
      // Case B: Composite String (This now handles ALL round-trip restores)
      success = true;
    } else {
      // Case C: Simple String ("en_US")
      std::string canon = getCanonicalLocale(locale);
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
    std::string canon = getCanonicalLocale(target);
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
    g_current_locale.categories[i] = new_categories[i];
  }

  // -------------------------------------------------------------------------
  // 6. UPDATE COMPOSITE STRING (SIMPLIFIED)
  // We ALWAYS generate the key-value list. No more "is_uniform" check.
  // Result is always: "LC_CTYPE=val;LC_NUMERIC=val;..."
  // -------------------------------------------------------------------------
  bool is_uniform = true;
  std::string first = g_current_locale.categories[0];

  // Check if every category matches the first one
  for (int i = 1; i < LC_ALL; ++i) {
    if (g_current_locale.categories[i] != first) {
      is_uniform = false;
      break;
    }
  }

  if (is_uniform) {
    // OPTIMIZATION: If all are "C", string is "C".
    // If all are "en_US.UTF-8", string is "en_US.UTF-8".
    g_current_locale.composite_lc_all = first;
  } else {
    // MIXED STATE: Generate standard Key-Value list.
    // Format: LC_CTYPE=val;LC_NUMERIC=val;...
    std::stringstream ss;
    for (int i = 0; i < LC_ALL; ++i) {
      if (i > 0) {
        ss << ";";
      }
      ss << GetCategoryName(i) << "=" << g_current_locale.categories[i];
    }
    g_current_locale.composite_lc_all = ss.str();
  }
  // -------------------------------------------------------------------------

  // 7. RETURN RESULT
  if (category == LC_ALL) {
    return const_cast<char*>(g_current_locale.composite_lc_all.c_str());
  }
  return const_cast<char*>(g_current_locale.categories[category].c_str());
}

locale_t newlocale(int category_mask, const char* locale, locale_t base) {
  if ((category_mask & ~kAllValidCategoriesMask) != 0 || locale == NULL) {
    errno = EINVAL;
    return (locale_t)0;
  }

  std::string canonical_locale;
  if (strcmp(locale, "") == 0) {
    canonical_locale = getCanonicalLocale(getenv("LANG"));
  } else {
    canonical_locale = getCanonicalLocale(locale);
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
      global_copy->categories[i] = g_current_locale.categories[i];
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
  // TODO: Properly implement this.
  return const_cast<struct lconv*>(GetCLocaleConv());
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
