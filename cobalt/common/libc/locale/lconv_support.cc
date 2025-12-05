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

#include "cobalt/common/libc/locale/lconv_support.h"

#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/ucnv.h"
#include "third_party/icu/source/i18n/unicode/decimfmt.h"
#include "third_party/icu/source/i18n/unicode/unum.h"

namespace cobalt {
namespace {
constexpr char kCurrencySymbolSucceeds = 0;
constexpr char kCurrencySymbolPrecedes = 1;

constexpr char kNoSpace = 0;
constexpr char kSpaceBetweenClusterAndValue = 1;
constexpr char kSpaceBetweenSignAndSym = 2;

constexpr char kSignPosnParens = 0;
constexpr char kSignPrecedesAll = 1;
constexpr char kSignSucceedsAll = 2;
constexpr char kSignPrecedesSymbol = 3;
constexpr char kSignSucceedsSymbol = 4;

struct CurrencyLayout {
  char cs_precedes;
  char sep_by_space;
  char sign_posn;
};

std::string ToUtf8(const icu::UnicodeString& us) {
  std::string out;
  us.toUTF8String(out);
  return out;
}

CurrencyLayout DeriveCurrencyLayout(const icu::DecimalFormat* monetary_format,
                                    const icu::UnicodeString& currency_symbol,
                                    const icu::UnicodeString& sign_symbol,
                                    bool is_negative) {
  CurrencyLayout result;
  icu::UnicodeString prefix, suffix;
  if (is_negative) {
    monetary_format->getNegativePrefix(prefix);
    monetary_format->getNegativeSuffix(suffix);
  } else {
    monetary_format->getPositivePrefix(prefix);
    monetary_format->getPositiveSuffix(suffix);
  }

  int sym_p_idx = prefix.indexOf(currency_symbol);
  result.cs_precedes =
      (sym_p_idx >= 0) ? kCurrencySymbolPrecedes : kCurrencySymbolSucceeds;

  int sign_p_idx = (!sign_symbol.isEmpty()) ? prefix.indexOf(sign_symbol) : -1;
  int sign_s_idx = (!sign_symbol.isEmpty()) ? suffix.indexOf(sign_symbol) : -1;

  int sym_s_idx = suffix.indexOf(currency_symbol);

  if (prefix.indexOf((UChar)'(') >= 0 && suffix.indexOf((UChar)')') >= 0) {
    result.sign_posn = kSignPosnParens;
  } else {
    if (sign_p_idx == -1 && sign_s_idx == -1) {
      result.sign_posn = kSignPrecedesAll;
    } else if (result.cs_precedes == kCurrencySymbolPrecedes) {
      if (sign_p_idx >= 0) {
        // Both Symbol and Sign are in the prefix.
        if (sign_p_idx > sym_p_idx) {
          result.sign_posn = kSignSucceedsSymbol;
        } else {
          result.sign_posn = kSignPrecedesAll;
        }
      } else {
        result.sign_posn = kSignSucceedsAll;
      }
    } else {
      if (sign_s_idx >= 0) {
        // Both Symbol and Sign are in the suffix.
        if (sym_s_idx >= 0 && sign_s_idx < sym_s_idx) {
          result.sign_posn = kSignPrecedesSymbol;
        } else {
          result.sign_posn = kSignSucceedsAll;
        }
      } else {
        result.sign_posn = kSignPrecedesAll;
      }
    }
  }

  bool touches_space = false;
  if (result.cs_precedes == kCurrencySymbolPrecedes) {
    touches_space = (prefix.endsWith(" ") || prefix.endsWith((UChar)0x00A0));
  } else {
    touches_space =
        (suffix.startsWith(" ") || suffix.startsWith((UChar)0x00A0));
  }

  if (touches_space) {
    result.sep_by_space = kSpaceBetweenClusterAndValue;
  } else {
    bool internal_space = false;
    icu::UnicodeString* target_str = nullptr;
    int idx_a = -1, len_a = 0;
    int idx_b = -1;

    if (sign_p_idx >= 0 && sym_p_idx >= 0) {
      // Both in Prefix
      target_str = &prefix;
      if (sign_p_idx < sym_p_idx) {
        idx_a = sign_p_idx;
        len_a = sign_symbol.length();
        idx_b = sym_p_idx;
      } else {
        idx_a = sym_p_idx;
        len_a = currency_symbol.length();
        idx_b = sign_p_idx;
      }
    } else if (sign_s_idx >= 0 && sym_s_idx >= 0) {
      // Both in Suffix
      target_str = &suffix;
      if (sign_s_idx < sym_s_idx) {
        idx_a = sign_s_idx;
        len_a = sign_symbol.length();
        idx_b = sym_s_idx;
      } else {
        idx_a = sym_s_idx;
        len_a = currency_symbol.length();
        idx_b = sign_s_idx;
      }
    }

    if (target_str) {
      int gap_start = idx_a + len_a;
      int gap_len = idx_b - gap_start;

      if (gap_len > 0) {
        icu::UnicodeString gap = target_str->tempSubString(gap_start, gap_len);
        if (gap.indexOf(" ") >= 0 || gap.indexOf((UChar)0x00A0) >= 0) {
          internal_space = true;
        }
      }
    }

    if (internal_space) {
      result.sep_by_space = kSpaceBetweenSignAndSym;
    } else {
      result.sep_by_space = kNoSpace;
    }
  }

  return result;
}
}  //  namespace

void UpdateNumericLconv(const std::string& locale_name, LconvImpl* cur_lconv) {
  if (cur_lconv->current_numeric_locale == locale_name) {
    return;
  }

  UErrorCode status = U_ZERO_ERROR;
  icu::Locale loc = icu::Locale::createCanonical(locale_name.c_str());
  icu::DecimalFormatSymbols sym(loc, status);

  std::unique_ptr<icu::DecimalFormat> grouping_format(
      static_cast<icu::DecimalFormat*>(
          icu::NumberFormat::createInstance(loc, status)));

  if (U_SUCCESS(status) && grouping_format) {
    cur_lconv->stored_decimal = ToUtf8(
        sym.getSymbol(icu::DecimalFormatSymbols::kDecimalSeparatorSymbol));
    cur_lconv->stored_thousands = ToUtf8(
        sym.getSymbol(icu::DecimalFormatSymbols::kGroupingSeparatorSymbol));

    cur_lconv->stored_grouping.clear();
    int p = grouping_format->getGroupingSize();
    int s = grouping_format->getSecondaryGroupingSize();
    if (p > 0) {
      cur_lconv->stored_grouping.push_back((char)p);
      if (s > 0 && s != p) {
        cur_lconv->stored_grouping.push_back((char)s);
      }
    }
  } else {
    cur_lconv->stored_decimal = ".";
    cur_lconv->stored_thousands = "";
    cur_lconv->stored_grouping.clear();
  }

  cur_lconv->result.decimal_point =
      const_cast<char*>(cur_lconv->stored_decimal.c_str());
  cur_lconv->result.thousands_sep =
      const_cast<char*>(cur_lconv->stored_thousands.c_str());
  cur_lconv->result.grouping =
      const_cast<char*>(cur_lconv->stored_grouping.c_str());
  cur_lconv->current_numeric_locale = locale_name;
}

void UpdateMonetaryLconv(const std::string& locale_name, LconvImpl* cur_lconv) {
  if (cur_lconv->current_monetary_locale == locale_name) {
    return;
  }

  UErrorCode status = U_ZERO_ERROR;
  icu::Locale loc = icu::Locale::createCanonical(locale_name.c_str());
  loc.addLikelySubtags(status);
  status = U_ZERO_ERROR;

  icu::DecimalFormatSymbols sym(loc, status);
  const char16_t* iso_str =
      sym.getSymbol(icu::DecimalFormatSymbols::kIntlCurrencySymbol).getBuffer();
  bool has_iso_code = (iso_str != nullptr && *iso_str != 0);
  int32_t detected_frac_digits = 0;
  if (has_iso_code) {
    detected_frac_digits = ucurr_getDefaultFractionDigits(iso_str, &status);
  } else {
    status = U_MISSING_RESOURCE_ERROR;
  }
  std::unique_ptr<icu::DecimalFormat> loc_currency_fmt(
      static_cast<icu::DecimalFormat*>(
          icu::NumberFormat::createCurrencyInstance(loc, status)));
  std::unique_ptr<icu::DecimalFormat> intl_currency_fmt(
      static_cast<icu::DecimalFormat*>(icu::NumberFormat::createInstance(
          loc, UNumberFormatStyle::UNUM_CURRENCY_ISO, status)));

  if (U_SUCCESS(status) && loc_currency_fmt && intl_currency_fmt) {
    cur_lconv->stored_mon_decimal = ToUtf8(
        sym.getSymbol(icu::DecimalFormatSymbols::kMonetarySeparatorSymbol));
    cur_lconv->stored_mon_thousands_sep = ToUtf8(sym.getSymbol(
        icu::DecimalFormatSymbols::kMonetaryGroupingSeparatorSymbol));
    cur_lconv->stored_currency_sym =
        ToUtf8(sym.getSymbol(icu::DecimalFormatSymbols::kCurrencySymbol));

    cur_lconv->stored_int_curr_sym =
        ToUtf8(sym.getSymbol(icu::DecimalFormatSymbols::kIntlCurrencySymbol));
    if (!cur_lconv->stored_int_curr_sym.empty()) {
      cur_lconv->stored_int_curr_sym += " ";
    }

    cur_lconv->stored_pos_sign =
        ToUtf8(sym.getSymbol(icu::DecimalFormatSymbols::kPlusSignSymbol));
    cur_lconv->stored_neg_sign =
        ToUtf8(sym.getSymbol(icu::DecimalFormatSymbols::kMinusSignSymbol));

    cur_lconv->result.frac_digits = (char)detected_frac_digits;
    cur_lconv->result.int_frac_digits = (char)detected_frac_digits;

    cur_lconv->stored_mon_grouping.clear();
    int p = loc_currency_fmt->getGroupingSize();
    int s = loc_currency_fmt->getSecondaryGroupingSize();
    if (p > 0) {
      cur_lconv->stored_mon_grouping.push_back((char)p);
      if (s > 0 && s != p) {
        cur_lconv->stored_mon_grouping.push_back((char)s);
      }
    }

    icu::UnicodeString loc_sym =
        sym.getSymbol(icu::DecimalFormatSymbols::kCurrencySymbol);
    icu::UnicodeString intl_sym =
        sym.getSymbol(icu::DecimalFormatSymbols::kIntlCurrencySymbol);
    icu::UnicodeString plus_sign =
        sym.getSymbol(icu::DecimalFormatSymbols::kPlusSignSymbol);
    icu::UnicodeString minus_sign =
        sym.getSymbol(icu::DecimalFormatSymbols::kMinusSignSymbol);

    CurrencyLayout loc_pos = DeriveCurrencyLayout(
        loc_currency_fmt.get(), loc_sym, plus_sign, /*is_negative=*/false);
    CurrencyLayout loc_neg = DeriveCurrencyLayout(
        loc_currency_fmt.get(), loc_sym, minus_sign, /*is_negative=*/true);

    cur_lconv->result.p_cs_precedes = loc_pos.cs_precedes;
    cur_lconv->result.p_sep_by_space = loc_pos.sep_by_space;
    cur_lconv->result.p_sign_posn = loc_pos.sign_posn;

    cur_lconv->result.n_cs_precedes = loc_neg.cs_precedes;
    cur_lconv->result.n_sep_by_space = loc_neg.sep_by_space;
    cur_lconv->result.n_sign_posn = loc_neg.sign_posn;

    CurrencyLayout intl_pos = DeriveCurrencyLayout(intl_currency_fmt.get(),
                                                   intl_sym, plus_sign, false);
    CurrencyLayout intl_neg = DeriveCurrencyLayout(intl_currency_fmt.get(),
                                                   intl_sym, minus_sign, true);

    cur_lconv->result.int_p_cs_precedes = intl_pos.cs_precedes;
    cur_lconv->result.int_p_sep_by_space = intl_pos.sep_by_space;
    cur_lconv->result.int_p_sign_posn = intl_pos.sign_posn;

    cur_lconv->result.int_n_cs_precedes = intl_neg.cs_precedes;
    cur_lconv->result.int_n_sep_by_space = intl_neg.sep_by_space;
    cur_lconv->result.int_n_sign_posn = intl_neg.sign_posn;

  } else {
    cur_lconv->stored_mon_decimal = "";
    cur_lconv->stored_mon_thousands_sep = "";
    cur_lconv->stored_currency_sym = "";
    cur_lconv->stored_int_curr_sym = "";
    cur_lconv->stored_pos_sign = "";
    cur_lconv->stored_neg_sign = "";
    cur_lconv->stored_mon_grouping.clear();

    cur_lconv->result.frac_digits = CHAR_MAX;
    cur_lconv->result.int_frac_digits = CHAR_MAX;

    cur_lconv->result.p_cs_precedes = CHAR_MAX;
    cur_lconv->result.p_sep_by_space = CHAR_MAX;
    cur_lconv->result.p_sign_posn = CHAR_MAX;
    cur_lconv->result.n_cs_precedes = CHAR_MAX;
    cur_lconv->result.n_sep_by_space = CHAR_MAX;
    cur_lconv->result.n_sign_posn = CHAR_MAX;

    cur_lconv->result.int_p_cs_precedes = CHAR_MAX;
    cur_lconv->result.int_p_sep_by_space = CHAR_MAX;
    cur_lconv->result.int_p_sign_posn = CHAR_MAX;
    cur_lconv->result.int_n_cs_precedes = CHAR_MAX;
    cur_lconv->result.int_n_sep_by_space = CHAR_MAX;
    cur_lconv->result.int_n_sign_posn = CHAR_MAX;
  }

  cur_lconv->result.mon_decimal_point =
      const_cast<char*>(cur_lconv->stored_mon_decimal.c_str());
  cur_lconv->result.mon_thousands_sep =
      const_cast<char*>(cur_lconv->stored_mon_thousands_sep.c_str());
  cur_lconv->result.currency_symbol =
      const_cast<char*>(cur_lconv->stored_currency_sym.c_str());
  cur_lconv->result.int_curr_symbol =
      const_cast<char*>(cur_lconv->stored_int_curr_sym.c_str());
  cur_lconv->result.positive_sign =
      const_cast<char*>(cur_lconv->stored_pos_sign.c_str());
  cur_lconv->result.negative_sign =
      const_cast<char*>(cur_lconv->stored_neg_sign.c_str());
  cur_lconv->result.mon_grouping =
      const_cast<char*>(cur_lconv->stored_mon_grouping.c_str());

  cur_lconv->current_monetary_locale = locale_name;
}
}  //  namespace cobalt
