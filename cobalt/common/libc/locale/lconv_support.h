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

#ifndef COBALT_COMMON_LIBC_LOCALE_LCONV_SUPPORT_H_
#define COBALT_COMMON_LIBC_LOCALE_LCONV_SUPPORT_H_

#include "cobalt/common/libc/locale/locale_support.h"

namespace cobalt {

// LconvImpl is the struct that will store the state of any given lconv object.
// It stores the string values that an lconv object will point to, along with
// the current locale strings used, to minimize duplication costs of a locale
// whos values are already loaded.
struct LconvImpl {
  struct lconv result;

  std::string stored_decimal, stored_thousands, stored_grouping;
  std::string stored_mon_decimal, stored_mon_thousands_sep, stored_mon_grouping;
  std::string stored_pos_sign, stored_neg_sign, stored_currency_sym,
      stored_int_curr_sym;

  std::string current_numeric_locale;
  std::string current_monetary_locale;

  LconvImpl() {
    ResetNumericToC();
    ResetMonetaryToC();
  }

  void ResetNumericToC();
  void ResetMonetaryToC();
};

// Updates all lconv values tied to the LC_NUMERIC locale for a given LconvImpl
// object. This function leverages ICU data to fill in the lconv struct to the
// best of its ability. Will return a boolean indicating if the function was
// successful in updating the lconv values.
bool UpdateNumericLconv(const std::string& locale_name, LconvImpl* cur_lconv);

// Updates all lconv values tied to the LC_MONETARY locale for a given LconvImpl
// object. This function leverages ICU data to fill in the lconv struct to the
// best of its ability. For some fields, a 1 : 1 matching from ICU to POSIX
// isn't possible, so we try to match the fields to as close to the POSIX
// standard as possible. Will return a boolean indicating if the function was
// successful in updating the lconv values.
bool UpdateMonetaryLconv(const std::string& locale_name, LconvImpl* cur_lconv);
}  //  namespace cobalt

#endif  // COBALT_COMMON_LIBC_LOCALE_LCONV_SUPPORT_H_
