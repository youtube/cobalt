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

struct LconvImpl {
  struct lconv result;

  std::string stored_decimal, stored_thousands, stored_grouping;
  std::string stored_mon_decimal, stored_mon_thousands_sep, stored_mon_grouping;
  std::string stored_pos_sign, stored_neg_sign, stored_currency_sym,
      stored_int_curr_sym;

  std::string current_numeric_locale;
  std::string current_monetary_locale;

  LconvImpl() { ResetToC(); }

  void ResetToC() {
    result.decimal_point = const_cast<char*>(".");
    result.thousands_sep = const_cast<char*>("");
    result.grouping = const_cast<char*>("");

    result.mon_decimal_point = const_cast<char*>("");
    result.mon_thousands_sep = const_cast<char*>("");
    result.mon_grouping = const_cast<char*>("");
    result.positive_sign = const_cast<char*>("");
    result.negative_sign = const_cast<char*>("");
    result.currency_symbol = const_cast<char*>("");
    result.int_curr_symbol = const_cast<char*>("");

    result.frac_digits = CHAR_MAX;
    result.int_frac_digits = CHAR_MAX;
    result.p_cs_precedes = CHAR_MAX;
    result.p_sep_by_space = CHAR_MAX;
    result.n_cs_precedes = CHAR_MAX;
    result.n_sep_by_space = CHAR_MAX;
    result.p_sign_posn = CHAR_MAX;
    result.n_sign_posn = CHAR_MAX;
    result.int_p_cs_precedes = CHAR_MAX;
    result.int_p_sep_by_space = CHAR_MAX;
    result.int_n_cs_precedes = CHAR_MAX;
    result.int_n_sep_by_space = CHAR_MAX;
    result.int_p_sign_posn = CHAR_MAX;
    result.int_n_sign_posn = CHAR_MAX;

    current_numeric_locale = "C";
    current_monetary_locale = "C";
  }
};

void UpdateNumericLconv(const std::string& locale_name, LconvImpl* cur_lconv);

void UpdateMonetaryLconv(const std::string& locale_name, LconvImpl* curr_lconv);
}  //  namespace cobalt

#endif  // COBALT_COMMON_LIBC_LOCALE_LCONV_SUPPORT_H_
