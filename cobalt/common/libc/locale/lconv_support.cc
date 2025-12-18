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

#include "starboard/common/log.h"
#include "third_party/icu/source/common/unicode/localebuilder.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/icu/source/i18n/unicode/decimfmt.h"
#include "third_party/icu/source/i18n/unicode/unum.h"

namespace cobalt {
namespace {

// |cs_precedes| uses these values to determine if the currency symbol precedes
// or succeeds the value for a formatted monetary quantity.
constexpr char kCurrencySymbolSucceeds = 0;
constexpr char kCurrencySymbolPrecedes = 1;

// The POSIX specification defines three distinct definitions for what
// |sep_by_space| and all variations of it can be. These are:
//
// 0: No space separates the currency symbol and value.
//
// 1: If the currency symbol and sign string are adjacent, a space separates
// them from the value; otherwise, a space separates the currency symbol from
// the value.
//
// 2: If the currency symbol and sign string are adjacent, a space separates
// them; otherwise, a space separates the sign string from the value.
constexpr char kNoSpace = 0;
constexpr char kSpaceBetweenClusterAndValue = 1;
constexpr char kSpaceBetweenSignAndSym = 2;

// The POSIX specification defines 5 distinct definitions for the possible
// values |sign_posn| can be. These are:
//
// 0: Parentheses surround the quantity and currency_symbol or int_curr_symbol.
//
// 1: The sign string precedes the quantity and currency_symbol or
// int_curr_symbol.
//
// 2: The sign string succeeds the quantity and currency_symbol or
// int_curr_symbol.
//
// 3: The sign string immediately precedes the currency_symbol or
// int_curr_symbol.
//
// 4: The sign string immediately succeeds the currency_symbol or
// int_curr_symbol.
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

// Convenience method to convert a POSIX locale string to the ICU string format.
// Some POSIX strings like sr_RS@latin require special handling to be fully
// translated from POSIX to ICU.
icu::Locale GetCorrectICULocale(const std::string& posix_name) {
  UErrorCode status = U_ZERO_ERROR;

  char canonical_buffer[ULOC_FULLNAME_CAPACITY];
  uloc_canonicalize(posix_name.c_str(), canonical_buffer,
                    ULOC_FULLNAME_CAPACITY, &status);

  if (U_FAILURE(status)) {
    return icu::Locale::createCanonical(posix_name.c_str());
  }

  icu::Locale loc = icu::Locale::createFromName(canonical_buffer);
  const char* variant = loc.getVariant();

  // ICU will store the modifier variable (e.g. @latin) in its variant field, if
  // it exists. For ICU to correctly translate the string, we must convert the
  // script to its shortname and set it in the script field.
  if (variant && *variant) {
    UScriptCode codes[1];

    UErrorCode script_status = U_ZERO_ERROR;
    int32_t num_codes = uscript_getCode(variant, codes, 1, &script_status);

    if (script_status == U_BUFFER_OVERFLOW_ERROR) {
      SB_LOG(WARNING) << "uscript_getCode returned U_BUFFER_OVERFLOW_ERROR. "
                         "We expected to receive only 1 code, but received "
                      << num_codes << " codes instead.";
    }

    if (U_SUCCESS(script_status) && num_codes == 1) {
      UScriptCode actual_script = codes[0];

      if (actual_script != USCRIPT_INVALID_CODE &&
          actual_script != USCRIPT_UNKNOWN) {
        const char* short_name = uscript_getShortName(actual_script);

        if (short_name) {
          icu::LocaleBuilder builder;
          builder.setLanguage(loc.getLanguage());
          builder.setRegion(loc.getCountry());
          builder.setScript(short_name);

          return builder.build(status);
        }
      }
    }
  }

  return loc;
}

bool StartsWithSpace(const icu::UnicodeString& s) {
  return s.length() > 0 &&
         (s.startsWith((UChar)' ') || s.startsWith((UChar)0x00A0));
}

bool EndsWithSpace(const icu::UnicodeString& s) {
  return s.length() > 0 &&
         (s.endsWith((UChar)' ') || s.endsWith((UChar)0x00A0));
}

// Checks if there is a space *between* two fields within the same string
bool HasInternalGap(const icu::UnicodeString& text,
                    int idx_a,
                    int len_a,
                    int idx_b,
                    int len_b) {
  int start = (idx_a < idx_b) ? idx_a + len_a : idx_b + len_b;
  int end = (idx_a < idx_b) ? idx_b : idx_a;
  int len = end - start;

  if (len <= 0) {
    return false;
  }

  icu::UnicodeString gap = text.tempSubString(start, len);
  return (gap.indexOf((UChar)' ') >= 0 || gap.indexOf((UChar)0x00A0) >= 0);
}

// Helper function that will the derive the sign position of a locale's currency
// format.
char GetSignPosition(const icu::UnicodeString& prefix,
                     const icu::UnicodeString& suffix,
                     int sign_p_idx,
                     int sign_s_idx,
                     int sym_p_idx,
                     int sym_s_idx,
                     char cs_precedes) {
  if (prefix.indexOf((UChar)'(') >= 0 && suffix.indexOf((UChar)')') >= 0) {
    return kSignPosnParens;
  }

  if (sign_p_idx == -1 && sign_s_idx == -1) {
    return kSignPrecedesAll;
  }

  if (cs_precedes == kCurrencySymbolPrecedes) {
    if (sign_p_idx >= 0) {
      if (sign_p_idx > sym_p_idx) {
        return kSignSucceedsSymbol;
      }
      return kSignPrecedesAll;
    }
    return kSignSucceedsAll;
  }

  if (sign_s_idx >= 0) {
    if (sym_s_idx >= 0 && sign_s_idx < sym_s_idx) {
      return kSignPrecedesSymbol;
    }
    return kSignSucceedsAll;
  }

  // It's possible that a currency layout has no currency symbol in its pattern.
  // In this case, we will just return kSignPrecedesAll.
  return kSignPrecedesAll;
}

// Helper function that will determine the |sep_by_space| category for the given
// ICU currency layout.
char GetSepBySpace(const icu::UnicodeString& prefix,
                   const icu::UnicodeString& suffix,
                   const icu::UnicodeString& sign_symbol,
                   const icu::UnicodeString& currency_symbol,
                   int sign_p_idx,
                   int sign_s_idx,
                   int sym_p_idx,
                   int sym_s_idx,
                   char cs_precedes) {
  bool space_touches_val = (cs_precedes == kCurrencySymbolPrecedes)
                               ? EndsWithSpace(prefix)
                               : StartsWithSpace(suffix);

  if (space_touches_val) {
    return kSpaceBetweenClusterAndValue;
  }

  bool internal_space_between_sign_and_sym = false;
  bool adjacent_prefix = (sign_p_idx >= 0 && sym_p_idx >= 0);
  bool adjacent_suffix = (sign_s_idx >= 0 && sym_s_idx >= 0);

  if (adjacent_prefix) {
    internal_space_between_sign_and_sym =
        HasInternalGap(prefix, sign_p_idx, sign_symbol.length(), sym_p_idx,
                       currency_symbol.length());
  } else if (adjacent_suffix) {
    internal_space_between_sign_and_sym =
        HasInternalGap(suffix, sign_s_idx, sign_symbol.length(), sym_s_idx,
                       currency_symbol.length());
  } else {
    if (cs_precedes == kCurrencySymbolPrecedes) {
      if (sign_s_idx >= 0 && StartsWithSpace(suffix)) {
        internal_space_between_sign_and_sym = true;
      }
    } else {
      if (sign_p_idx >= 0 && EndsWithSpace(prefix)) {
        internal_space_between_sign_and_sym = true;
      }
    }
  }

  return internal_space_between_sign_and_sym ? kSpaceBetweenSignAndSym
                                             : kNoSpace;
}

// Attempts to convert the currency layout of an ICU monetary format to the
// lconv's format specifiers for monetary values (cs_precedes, sep_by_space
// etc.) This function will call helper functions that will parse through an
// ICU's monetary format prefix and suffix string to set the lconv values to the
// best of its ability. Due to the design differences in how ICU stores this
// data versus what POSIX wants, these conversions will not always be perfect.
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

  const bool has_sign = !sign_symbol.isEmpty();
  int sign_p_idx = has_sign ? prefix.indexOf(sign_symbol) : -1;
  int sign_s_idx = has_sign ? suffix.indexOf(sign_symbol) : -1;
  int sym_s_idx = suffix.indexOf(currency_symbol);

  if (sign_p_idx != -1 || sign_s_idx != -1) {
    result.sign = ToUtf8(sign_symbol);
  } else {
    result.sign = "";
  }

  result.sign_posn = GetSignPosition(prefix, suffix, sign_p_idx, sign_s_idx,
                                     sym_p_idx, sym_s_idx, result.cs_precedes);

  result.sep_by_space =
      GetSepBySpace(prefix, suffix, sign_symbol, currency_symbol, sign_p_idx,
                    sign_s_idx, sym_p_idx, sym_s_idx, result.cs_precedes);

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

  UErrorCode status = U_ZERO_ERROR;
  icu::Locale loc = GetCorrectICULocale(locale_name);

  // To avoid partially allocated lconv structs, we first initialize all the ICU
  // objects needed to fill out the struct. If any initialization fails, we just
  // return false and make no edits to the lconv.
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

  UErrorCode status = U_ZERO_ERROR;
  icu::Locale loc = GetCorrectICULocale(locale_name);

  // If the locale has no Country/Region defined, we can't determine the
  // currency. We use addLikelySubtags to assume the default (e.g., "en" ->
  // "en_US").
  const char* country = loc.getCountry();
  if (country == nullptr || *country == 0) {
    loc.addLikelySubtags(status);
  }

  if (U_FAILURE(status)) {
    return false;
  }
  status = U_ZERO_ERROR;

  // To avoid partially allocated lconv structs, we first initialize all the ICU
  // objects needed to fill out the struct. If any initialization fails, we just
  // return false and make no edits to the lconv.
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
