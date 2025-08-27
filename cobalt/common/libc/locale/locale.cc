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

#include <limits.h>
#include <locale.h>
#include <string.h>

#include "cobalt/common/libc/no_destructor.h"

namespace {
// See https://en.cppreference.com/w/c/locale/lconv.
const lconv* GetCLocaleConv() {
  static const lconv c_locale_conv = {
      .decimal_point = const_cast<char*>("."),
      .thousands_sep = const_cast<char*>(""),
      .grouping = const_cast<char*>(""),
      .int_curr_symbol = const_cast<char*>(""),
      .currency_symbol = const_cast<char*>("C"),
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
}  // namespace

// The POSIX setlocale is not hermetic, so we must provide our own
// implementation.
char* setlocale(int category, const char* locale) {
  if (locale == NULL) {
    return const_cast<char*>("C");
  }
  if (strcmp(locale, "C") == 0 || strcmp(locale, "") == 0) {
    return const_cast<char*>("C");
  }
  return NULL;
}

locale_t newlocale(int category_mask, const char* locale, locale_t base) {
  if (locale == NULL || (strcmp(locale, "C") != 0 && strcmp(locale, "") != 0)) {
    return (locale_t)0;
  }

  lconv* new_lconv = new lconv;
  if (base != (locale_t)0) {
    memcpy(new_lconv, reinterpret_cast<lconv*>(base), sizeof(lconv));
    freelocale(base);
  } else {
    memcpy(new_lconv, GetCLocaleConv(), sizeof(lconv));
  }
  return reinterpret_cast<locale_t>(new_lconv);
}

namespace {
thread_local locale_t g_current_locale = LC_GLOBAL_LOCALE;
}  // namespace

locale_t uselocale(locale_t newloc) {
  locale_t old_locale = g_current_locale;
  if (newloc != (locale_t)0) {
    g_current_locale = newloc;
  }
  return old_locale;
}

void freelocale(locale_t loc) {
  if (loc) {
    delete reinterpret_cast<lconv*>(loc);
  }
}

struct lconv* localeconv(void) {
  return const_cast<lconv*>(GetCLocaleConv());
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
