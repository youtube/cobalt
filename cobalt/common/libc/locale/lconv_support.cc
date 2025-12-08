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

// Convenience struct to hold all the values DeriveCurrencyLayout must return
// when used.
struct CurrencyLayout {
  char cs_precedes;
  char sep_by_space;
  char sign_posn;
  std::string sign;
};

// Convenience method to convert icu::UnicodeStrings to std::string.
std::string ToUtf8(const icu::UnicodeString& us) {
  std::string out;
  us.toUTF8String(out);
  return out;
}

// Attempts to convert the currency layout of an ICU monetary format to the
// lconv's format specifiers for monetary values (cs_precedes, sep_by_space
// etc.) This function will parse through an ICU's monetary format prefix and
// suffix string to set the lconv values to the best of its ability. Due to the
// design differences in how ICU stores this data versus what POSIX wants, these
// conversions will not always be perfect.
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

  // The currency symbol (if present) can either be in the prefix or suffix, so
  // we must check both.
  const bool has_sign = !sign_symbol.isEmpty();
  int sign_p_idx = has_sign ? prefix.indexOf(sign_symbol) : -1;
  int sign_s_idx = has_sign ? suffix.indexOf(sign_symbol) : -1;
  int sym_s_idx = suffix.indexOf(currency_symbol);

  if (sign_p_idx != -1 || sign_s_idx != -1) {
    result.sign = ToUtf8(sign_symbol);
  } else {
    result.sign = "";
  }

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
    // To check if |sep_by_space| should be set to kSpaceBetweenSignAndSym, we
    // must:
    // - Check to see if the sign and currency symbol are in the same affix
    // - If they are, see if there is a space between the two characters
    const icu::UnicodeString* shared_affix = nullptr;
    int sign_idx = -1;
    int sym_idx = -1;

    if (sign_p_idx >= 0 && sym_p_idx >= 0) {
      shared_affix = &prefix;
      sign_idx = sign_p_idx;
      sym_idx = sym_p_idx;
    } else if (sign_s_idx >= 0 && sym_s_idx >= 0) {
      shared_affix = &suffix;
      sign_idx = sign_s_idx;
      sym_idx = sym_s_idx;
    }

    bool internal_space = false;

    if (shared_affix) {
      int gap_start = 0;
      int gap_end = 0;

      if (sign_idx < sym_idx) {
        gap_start = sign_idx + sign_symbol.length();
        gap_end = sym_idx;
      } else {
        gap_start = sym_idx + currency_symbol.length();
        gap_end = sign_idx;
      }

      int gap_len = gap_end - gap_start;
      if (gap_len > 0) {
        icu::UnicodeString gap =
            shared_affix->tempSubString(gap_start, gap_len);
        if (gap.indexOf((UChar)' ') >= 0 || gap.indexOf((UChar)0x00A0) >= 0) {
          internal_space = true;
        }
      }
    }
    result.sep_by_space = internal_space ? kSpaceBetweenSignAndSym : kNoSpace;
  }

  return result;
}

// Convenience method to get the grouping string for a given decimal format.
std::string GetGroupingString(const icu::DecimalFormat* formatter) {
  std::string grouping;
  int primary_size = formatter->getGroupingSize();
  if (primary_size > 0) {
    grouping.push_back(static_cast<char>(primary_size));
    int secondary_size = formatter->getSecondaryGroupingSize();
    if (secondary_size > 0 && secondary_size != primary_size) {
      grouping.push_back(static_cast<char>(secondary_size));
    }
  }
  return grouping;
}
}  //  namespace

// Helper function to set an LconvImpl to the default C lconv.
void LconvImpl::ResetToC() {
  stored_decimal = ".";
  stored_thousands = "";
  stored_grouping = "";

  stored_mon_decimal = "";
  stored_mon_thousands_sep = "";
  stored_mon_grouping = "";
  stored_pos_sign = "";
  stored_neg_sign = "";
  stored_currency_sym = "";
  stored_int_curr_sym = "";

  result.frac_digits = CHAR_MAX;
  result.int_frac_digits = CHAR_MAX;
  result.p_cs_precedes = CHAR_MAX;
  result.p_sep_by_space = CHAR_MAX;
  result.p_sign_posn = CHAR_MAX;
  result.n_cs_precedes = CHAR_MAX;
  result.n_sep_by_space = CHAR_MAX;
  result.n_sign_posn = CHAR_MAX;
  result.int_p_cs_precedes = CHAR_MAX;
  result.int_p_sep_by_space = CHAR_MAX;
  result.int_p_sign_posn = CHAR_MAX;
  result.int_n_cs_precedes = CHAR_MAX;
  result.int_n_sep_by_space = CHAR_MAX;
  result.int_n_sign_posn = CHAR_MAX;

  result.decimal_point = const_cast<char*>(stored_decimal.c_str());
  result.thousands_sep = const_cast<char*>(stored_thousands.c_str());
  result.grouping = const_cast<char*>(stored_grouping.c_str());

  result.mon_decimal_point = const_cast<char*>(stored_mon_decimal.c_str());
  result.mon_thousands_sep =
      const_cast<char*>(stored_mon_thousands_sep.c_str());
  result.mon_grouping = const_cast<char*>(stored_mon_grouping.c_str());
  result.positive_sign = const_cast<char*>(stored_pos_sign.c_str());
  result.negative_sign = const_cast<char*>(stored_neg_sign.c_str());
  result.currency_symbol = const_cast<char*>(stored_currency_sym.c_str());
  result.int_curr_symbol = const_cast<char*>(stored_int_curr_sym.c_str());
  current_numeric_locale = "C";
  current_monetary_locale = "C";
}

bool UpdateNumericLconv(const std::string& locale_name, LconvImpl* cur_lconv) {
  if (cur_lconv->current_numeric_locale == locale_name) {
    return true;
  }

  // To avoid partially allocated lconv structs, we first initialize all the ICU
  // objects needed to fill out the struct. If any initialization fails, we just
  // return false and make no edits to the lconv.
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale loc = icu::Locale::createCanonical(locale_name.c_str());
  icu::DecimalFormatSymbols sym(loc, status);

  std::unique_ptr<icu::DecimalFormat> grouping_format(
      static_cast<icu::DecimalFormat*>(
          icu::NumberFormat::createInstance(loc, status)));

  if (U_FAILURE(status) || !grouping_format) {
    return false;
  }
  cur_lconv->stored_decimal =
      ToUtf8(sym.getSymbol(icu::DecimalFormatSymbols::kDecimalSeparatorSymbol));
  cur_lconv->stored_thousands = ToUtf8(
      sym.getSymbol(icu::DecimalFormatSymbols::kGroupingSeparatorSymbol));

  cur_lconv->stored_grouping = GetGroupingString(grouping_format.get());
  cur_lconv->result.decimal_point =
      const_cast<char*>(cur_lconv->stored_decimal.c_str());
  cur_lconv->result.thousands_sep =
      const_cast<char*>(cur_lconv->stored_thousands.c_str());
  cur_lconv->result.grouping =
      const_cast<char*>(cur_lconv->stored_grouping.c_str());
  cur_lconv->current_numeric_locale = locale_name;
  return true;
}

bool UpdateMonetaryLconv(const std::string& locale_name, LconvImpl* cur_lconv) {
  if (cur_lconv->current_monetary_locale == locale_name) {
    return true;
  }

  // To avoid partially allocated lconv structs, we first initialize all the ICU
  // objects needed to fill out the struct. If any initialization fails, we just
  // return false and make no edits to the lconv.
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale loc = icu::Locale::createCanonical(locale_name.c_str());
  loc.addLikelySubtags(status);

  icu::DecimalFormatSymbols sym(loc, status);
  if (!U_SUCCESS(status)) {
    return false;
  }
  const icu::UnicodeString intl_sym =
      sym.getSymbol(icu::DecimalFormatSymbols::kIntlCurrencySymbol);
  if (intl_sym.isEmpty()) {
    return false;
  }
  int32_t detected_frac_digits =
      ucurr_getDefaultFractionDigits(intl_sym.getBuffer(), &status);

  std::unique_ptr<icu::DecimalFormat> loc_currency_fmt(
      static_cast<icu::DecimalFormat*>(
          icu::NumberFormat::createCurrencyInstance(loc, status)));

  std::unique_ptr<icu::DecimalFormat> intl_currency_fmt(
      static_cast<icu::DecimalFormat*>(icu::NumberFormat::createInstance(
          loc, UNumberFormatStyle::UNUM_CURRENCY_ISO, status)));

  if (!U_SUCCESS(status) || !loc_currency_fmt || !intl_currency_fmt) {
    return false;
  }

  const icu::UnicodeString plus_sign =
      sym.getSymbol(icu::DecimalFormatSymbols::kPlusSignSymbol);
  const icu::UnicodeString minus_sign =
      sym.getSymbol(icu::DecimalFormatSymbols::kMinusSignSymbol);
  const icu::UnicodeString loc_sym =
      sym.getSymbol(icu::DecimalFormatSymbols::kCurrencySymbol);
  const CurrencyLayout loc_pos = DeriveCurrencyLayout(
      loc_currency_fmt.get(), loc_sym, plus_sign, /*is_negative=*/false);
  const CurrencyLayout loc_neg = DeriveCurrencyLayout(
      loc_currency_fmt.get(), loc_sym, minus_sign, /*is_negative=*/true);
  const CurrencyLayout intl_pos =
      DeriveCurrencyLayout(intl_currency_fmt.get(), intl_sym, plus_sign, false);
  const CurrencyLayout intl_neg =
      DeriveCurrencyLayout(intl_currency_fmt.get(), intl_sym, minus_sign, true);

  cur_lconv->stored_mon_decimal = ToUtf8(
      sym.getSymbol(icu::DecimalFormatSymbols::kMonetarySeparatorSymbol));
  cur_lconv->stored_mon_thousands_sep = ToUtf8(sym.getSymbol(
      icu::DecimalFormatSymbols::kMonetaryGroupingSeparatorSymbol));
  cur_lconv->stored_currency_sym = ToUtf8(loc_sym);
  cur_lconv->stored_int_curr_sym = ToUtf8(intl_sym);
  // POSIX expects this symbol to store a space after the string, but ICU does
  // not do this. To bridge the gap, we add a space ourselves.
  if (!cur_lconv->stored_int_curr_sym.empty()) {
    cur_lconv->stored_int_curr_sym += " ";
  }

  cur_lconv->result.frac_digits = (char)detected_frac_digits;
  cur_lconv->result.int_frac_digits = (char)detected_frac_digits;

  cur_lconv->stored_pos_sign = ToUtf8(plus_sign);
  cur_lconv->stored_neg_sign = ToUtf8(minus_sign);

  cur_lconv->stored_mon_grouping = GetGroupingString(loc_currency_fmt.get());

  cur_lconv->result.p_cs_precedes = loc_pos.cs_precedes;
  cur_lconv->result.p_sep_by_space = loc_pos.sep_by_space;
  cur_lconv->result.p_sign_posn = loc_pos.sign_posn;
  cur_lconv->stored_pos_sign = loc_pos.sign;

  cur_lconv->result.n_cs_precedes = loc_neg.cs_precedes;
  cur_lconv->result.n_sep_by_space = loc_neg.sep_by_space;
  cur_lconv->result.n_sign_posn = loc_neg.sign_posn;
  cur_lconv->stored_neg_sign = loc_neg.sign;

  cur_lconv->result.int_p_cs_precedes = intl_pos.cs_precedes;
  cur_lconv->result.int_p_sep_by_space = intl_pos.sep_by_space;
  cur_lconv->result.int_p_sign_posn = intl_pos.sign_posn;

  cur_lconv->result.int_n_cs_precedes = intl_neg.cs_precedes;
  cur_lconv->result.int_n_sep_by_space = intl_neg.sep_by_space;
  cur_lconv->result.int_n_sign_posn = intl_neg.sign_posn;

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
  return true;
}
}  //  namespace cobalt
