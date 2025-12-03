// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/nplb/posix_compliance/posix_locale_helpers.h"

#include <locale.h>

#include <cstring>
#include <string>
#include <vector>

namespace starboard {
namespace nplb {

const char* LocaleFinder::FindSupported(
    const std::vector<const char*>& locales) {
  for (const char* locale_name : locales) {
    locale_t loc = newlocale(LC_ALL_MASK, locale_name, (locale_t)0);
    if (loc) {
      freelocale(loc);
      return locale_name;
    }
  }
  return nullptr;
}

const char* GetCommaDecimalSeparatorLocale() {
  // The availability of locales on a system is not guaranteed, so this
  // function does a best effort to find one, to minimize the number of tests
  // that have to be skipped.
  // A list of locales that use a comma as the decimal separator.
  constexpr const char* kCommaDecimalSeparatorLocales[] = {
      "af",         "af_NA",      "af_ZA",      "agq",        "agq_CM",
      "sq",         "sq_AL",      "sq_XK",      "sq_MK",      "hy",
      "hy_AM",      "ast",        "ast_ES",     "az",         "az_Cyrl",
      "az_Cyrl_AZ", "az_Latn",    "az_Latn_AZ", "bas",        "bas_CM",
      "eu",         "eu_ES",      "be",         "be_BY",      "br",
      "br_FR",      "bg",         "bg_BG",      "ca",         "ca_AD",
      "ca_ES",      "ca_FR",      "ca_IT",      "hr",         "hr_BA",
      "cs",         "cs_CZ",      "da",         "da_DK",      "da_GL",
      "nl",         "nl_AW",      "nl_BE",      "nl_BQ",      "nl_CW",
      "nl_NL",      "nl_SX",      "nl_SR",      "en_AT",      "en_BE",
      "en_DE",      "en_DK",      "en_FI",      "en_NL",      "en_SI",
      "en_SE",      "eo",         "et",         "et_EE",      "fo",
      "fo_DK",      "fo_FO",      "fi",         "fi_FI",      "fr",
      "fr_BE",      "fr_BF",      "fr_BI",      "fr_BJ",      "fr_BL",
      "fr_CA",      "fr_CD",      "fr_CF",      "fr_CG",      "fr_CI",
      "fr_CM",      "fr_DJ",      "fr_DZ",      "fr_FR",      "fr_GA",
      "fr_GF",      "fr_GN",      "fr_GP",      "fr_GQ",      "fr_HT",
      "fr_KM",      "fr_LU",      "fr_MA",      "fr_MC",      "fr_MF",
      "fr_MG",      "fr_ML",      "fr_MQ",      "fr_MR",      "fr_MU",
      "fr_NC",      "fr_NE",      "fr_PF",      "fr_PM",      "fr_RE",
      "fr_RW",      "fr_SC",      "fr_SN",      "fr_SY",      "fr_TD",
      "fr_TG",      "fr_TN",      "fr_VU",      "fr_WF",      "fr_YT",
      "fur",        "fur_IT",     "ff",         "ff_Latn",    "ff_Latn_BF",
      "ff_Latn_CM", "ff_Latn_GH", "ff_Latn_GM", "ff_Latn_GN", "ff_Latn_GW",
      "ff_Latn_LR", "ff_Latn_MR", "ff_Latn_NE", "ff_Latn_NG", "ff_Latn_SL",
      "ff_Latn_SN", "gl",         "gl_ES",      "ka",         "ka_GE",
      "de",         "de_AT",      "de_BE",      "de_DE",      "de_IT",
      "de_LU",      "el",         "el_CY",      "el_GR",      "hu",
      "hu_HU",      "is",         "is_IS",      "id",         "id_ID",
      "ia",         "it",         "it_IT",      "it_SM",      "it_VA",
      "dyo",        "dyo_SN",     "kea",        "kea_CV",     "kab",
      "kab_DZ",     "kkj",        "kkj_CM",     "kl",         "kl_GL",
      "nmg",        "nmg_CM",     "ky",         "ky_KG",      "lv",
      "lv_LV",      "ln",         "ln_AO",      "ln_CD",      "ln_CF",
      "ln_CG",      "lt",         "lt_LT",      "dsb",        "dsb_DE",
      "lu",         "lu_CD",      "lb",         "lb_LU",      "mk",
      "mk_MK",      "mgh",        "mgh_MZ",     "ms_ID",      "mua",
      "mua_CM",     "nnh",        "nnh_CM",     "jgo",        "jgo_CM",
      "yav",        "yav_CM",     "se",         "se_FI",      "se_NO",
      "se_SE",      "nb",         "nb_NO",      "nb_SJ",      "os",
      "os_GE",      "os_RU",      "pl",         "pl_PL",      "pt",
      "pt_AO",      "pt_BR",      "pt_CV",      "pt_GQ",      "pt_GW",
      "pt_LU",      "pt_MZ",      "pt_PT",      "pt_ST",      "pt_TL",
      "ro",         "ro_MD",      "ro_RO",      "rn",         "rn_BI",
      "ru",         "ru_BY",      "ru_KG",      "ru_KZ",      "ru_MD",
      "ru_RU",      "ru_UA",      "sg",         "sg_CF",      "sc",
      "sc_IT",      "sr",         "sr_Cyrl",    "sr_Cyrl_BA", "sr_Cyrl_ME",
      "sr_Cyrl_RS", "sr_Cyrl_XK", "sr_Latn",    "sr_Latn_BA", "sr_Latn_ME",
      "sr_Latn_RS", "sr_Latn_XK", "sk",         "sk_SK",      "sl",
      "sl_SI",      "es_AR",      "es_BO",      "es_CL",      "es_CO",
      "es_CR",      "es_CU",      "es_DO",      "es_EC",      "es_ES",
      "es_GQ",      "es_GT",      "es_HN",      "es_PY",      "es_PE",
      "es_UY",      "es_VE",      "sw_CD",      "sv",         "sv_AX",
      "sv_FI",      "sv_SE",      "shi",        "shi_Latn",   "shi_Latn_MA",
      "tg",         "tg_TJ",      "tt",         "tt_RU",      "tr",
      "tr_CY",      "tr_TR",      "tk",         "tk_TM",      "uk",
      "uk_UA",      "hsb",        "hsb_DE",     "uz_Cyrl",    "uz_Cyrl_UZ",
      "vi",         "vi_VN",      "fy",         "fy_NL",      "wo",
      "wo_SN",      "sah",        "sah_RU",     "ybb",        "ybb_CM"};

  static const char* locale = [kCommaDecimalSeparatorLocales]() -> const char* {
    static std::vector<std::string> generated_locales;
    if (generated_locales.empty()) {
      const char* suffixes[] = {"", ".UTF-8"};
      for (const char* base_locale : kCommaDecimalSeparatorLocales) {
        for (const char* suffix : suffixes) {
          generated_locales.push_back(std::string(base_locale) + suffix);
        }
      }
    }

    for (const std::string& locale_name : generated_locales) {
      if (setlocale(LC_NUMERIC, locale_name.c_str())) {
        struct lconv* lconv_data = localeconv();
        bool found = (lconv_data && lconv_data->decimal_point &&
                      strcmp(lconv_data->decimal_point, ",") == 0);
        // Restore the default locale.
        setlocale(LC_NUMERIC, "C");
        if (found) {
          return locale_name.c_str();
        }
      }
    }
    return nullptr;
  }();
  return locale;
}

const char* GetNonDefaultLocale() {
  static const char* locale = [] {
    const char* en_us_locale = LocaleFinder::FindSupported({"en_US"});
    if (en_us_locale) {
      return en_us_locale;
    }
    // Fallback to a locale that is likely to exist.
    return GetCommaDecimalSeparatorLocale();
  }();
  return locale;
}

}  // namespace nplb
}  // namespace starboard
