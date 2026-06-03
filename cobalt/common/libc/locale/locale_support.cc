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

#include "cobalt/common/libc/locale/locale_support.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <unordered_set>

#include "starboard/common/log.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/ucnv.h"

namespace cobalt {

namespace {
constexpr char kLcCtypeStr[] = "LC_CTYPE";
constexpr char kLcNumericStr[] = "LC_NUMERIC";
constexpr char kLcTimeStr[] = "LC_TIME";
constexpr char kLcCollateStr[] = "LC_COLLATE";
constexpr char kLcMonetaryStr[] = "LC_MONETARY";
constexpr char kLcMessagesStr[] = "LC_MESSAGES";

constexpr char kUTF8EncodingStr[] = ".UTF-8";

constexpr char kCompositeSeparator = ';';
constexpr char kCompositeAssign = '=';

// Returns the string name of a category (e.g., "LC_TIME").
const char* GetCategoryName(int category) {
  switch (category) {
    case LC_CTYPE:
      return kLcCtypeStr;
    case LC_NUMERIC:
      return kLcNumericStr;
    case LC_TIME:
      return kLcTimeStr;
    case LC_COLLATE:
      return kLcCollateStr;
    case LC_MONETARY:
      return kLcMonetaryStr;
    case LC_MESSAGES:
      return kLcMessagesStr;
    default:
      return "";
  }
}

// Returns the index of a category from its name.
int GetCategoryIndexFromName(const std::string& name) {
  if (name == kLcCtypeStr) {
    return LC_CTYPE;
  }
  if (name == kLcNumericStr) {
    return LC_NUMERIC;
  }
  if (name == kLcTimeStr) {
    return LC_TIME;
  }
  if (name == kLcCollateStr) {
    return LC_COLLATE;
  }
  if (name == kLcMonetaryStr) {
    return LC_MONETARY;
  }
  if (name == kLcMessagesStr) {
    return LC_MESSAGES;
  }
  return -1;
}

// Extracts the locale string's encoding (ex. UTF-8) from the locale. This
// encoding value string is returned, if it exists. We do this because when
// giving a POSIX formatted locale to ICU, ICU always drops the encoding. This
// function ensures that the encoding isn't loss upon conversion. This function
// supports locale strings of the format
// language[_territory][.codeset][@modifier].
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

// Checks to see if the given locale exists within ICU's database.
bool IsExactLocaleAvailable(const char* locale_id) {
  static const auto kAvailableLocales = []() {
    std::unordered_set<std::string> locales;
    int32_t count = 0;
    const icu::Locale* available = icu::Locale::getAvailableLocales(count);
    for (int32_t i = 0; i < count; ++i) {
      locales.insert(available[i].getName());
    }
    return locales;
  }();

  return kAvailableLocales.count(locale_id) > 0;
}

// Helps convert an ICU language script (like "Latn" in sr_Latn_RS) to its
// corresponding POSIX compliant @ modifier (@latin for sr_RS@latin), if the
// script exists.
std::string MapScriptToModifier(const char* script) {
  if (!script || script[0] == '\0') {
    return "";
  }
  // The values inside kScriptMap are the only ones we need to support.
  constexpr std::array<std::pair<std::string_view, std::string_view>, 4>
      kScriptMap = {{{"Latn", "@latin"},
                     {"Cyrl", "@cyrillic"},
                     {"Hant", "@hant"},
                     {"Hans", "@hans"}}};

  auto it = std::find_if(
      kScriptMap.begin(), kScriptMap.end(),
      [script](const auto& entry) { return entry.first == script; });

  if (it != kScriptMap.end()) {
    return std::string(it->second);
  }
  return "";
}

// Check to see if the given locale is supported by ICU. If it isn't, we try
// to see if the locale's parent is supported.
bool IsSupportedThroughFallback(const char* canonical_name) {
  // TODO: b/462446756 - Properly address the "as_IN" locale.
  if (strcmp(canonical_name, "as_IN") == 0) {
    return true;
  }

  UErrorCode status = U_ZERO_ERROR;
  char current[ULOC_FULLNAME_CAPACITY];
  char parent[ULOC_FULLNAME_CAPACITY];

  strncpy(current, canonical_name, ULOC_FULLNAME_CAPACITY);
  current[ULOC_FULLNAME_CAPACITY - 1] = '\0';

  while (strlen(current) > 0) {
    if (IsExactLocaleAvailable(current)) {
      return true;
    }

    // If we fell back all the way to root, it means no specific language data
    // was found.
    if (strcmp(current, "root") == 0) {
      return false;
    }

    int32_t len =
        uloc_getParent(current, parent, ULOC_FULLNAME_CAPACITY, &status);

    if (U_FAILURE(status) || len == 0) {
      break;
    }
    strncpy(current, parent, ULOC_FULLNAME_CAPACITY);
  }

  return false;
}
}  // namespace

bool IsValidLcCategory(int category) {
  switch (category) {
    case LC_CTYPE:
    case LC_NUMERIC:
    case LC_TIME:
    case LC_COLLATE:
    case LC_MONETARY:
    case LC_MESSAGES:
    case LC_ALL:
      return true;
    default:
      return false;
  }
}

std::string GetCanonicalLocale(const char* inputLocale) {
  if (!inputLocale || inputLocale[0] == '\0') {
    return "";
  }

  if (strcmp(inputLocale, kCLocale) == 0 ||
      strcmp(inputLocale, kPosixLocale) == 0) {
    return inputLocale;
  }

  std::string work_str = inputLocale;
  std::string encoding = ExtractEncoding(work_str);

  if (encoding != "" && encoding != kUTF8EncodingStr) {
    SB_LOG(WARNING) << "Encoding " << encoding
                    << " was requested, but Cobalt only supports the "
                       "\".UTF-8\" encoding. Setting the encoding to UTF-8.";
    encoding = kUTF8EncodingStr;
  }

  icu::Locale loc = icu::Locale::createCanonical(work_str.c_str());
  if (loc.isBogus()) {
    return "";
  }

  const char* icu_name = loc.getName();

  if (!IsSupportedThroughFallback(icu_name)) {
    return "";
  }

  // Since we have validated that the locale exists inside ICU, we now
  // reconstruct the string in POSIX format for the locale functions to use.
  // According to the man pages of setlocale, a POSIX compliant locale will
  // follow the form:
  //
  // language[_territory][.codeset][@modifier]
  std::string posix_id = loc.getLanguage();
  const char* country = loc.getCountry();

  if (country && country[0] != '\0') {
    posix_id += "_";
    posix_id += country;
  }

  posix_id += encoding;

  // If our locale given was "sr_latn_RS", the |_latn_| will be converted to
  // @latin.
  std::string modifier_str = "";
  modifier_str += MapScriptToModifier(loc.getScript());

  // If our locale given explicitly had an @ modifier like "sr_RS@latin", ICU
  // will store @latin inside its variant field. We retrieve the field and
  // append it. If for some reason the modifier string already has an @ modifier
  // present, we do not use the contents of the variant.
  const char* variant = loc.getVariant();
  if (variant && variant[0] != '\0' && modifier_str.empty()) {
    std::string v = variant;
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);
    modifier_str = "@" + v;
  }

  posix_id += modifier_str;

  return posix_id;
}

void SyncIcuDefault(const std::string& posix_locale_id) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale icu_loc = icu::Locale::createCanonical(posix_locale_id.c_str());
  icu::Locale::setDefault(icu_loc, status);
}

bool ParseCompositeLocale(const char* input,
                          const LocaleImpl& current_state,
                          std::array<std::string, LC_ALL>& out_categories) {
  std::string str = input;

  // Our pattern for the composite locale is LC_CTYPE=C;LC_TIME=POSIX...
  // If it doesn't contain '=', it is likely a simple locale name (e.g. "C" or
  // "en_US").
  if (str.find(kCompositeAssign) == std::string::npos) {
    return false;
  }

  out_categories = current_state.categories;

  std::stringstream ss(str);
  std::string segment;

  while (std::getline(ss, segment, kCompositeSeparator)) {
    if (segment.empty()) {
      continue;
    }

    size_t eq_pos = segment.find(kCompositeAssign);
    if (eq_pos == std::string::npos) {
      return false;
    }

    std::string key = segment.substr(0, eq_pos);
    std::string val = segment.substr(eq_pos + 1);

    int idx = GetCategoryIndexFromName(key);
    if (idx == -1) {
      return false;
    }

    // As a safety measure, we check that every locale inside the string is
    // actually supported.
    std::string canon = GetCanonicalLocale(val.c_str());
    if (canon.empty()) {
      return false;
    }
    out_categories[idx] = canon;
  }
  return true;
}

void RefreshCompositeString(LocaleImpl* loc) {
  bool is_uniform = true;
  const std::string& first = loc->categories[0];

  for (int i = 1; i < LC_ALL; ++i) {
    if (loc->categories[i] != first) {
      is_uniform = false;
      break;
    }
  }

  // If all the LC categories contain the same string, we can just set
  // the composite locale string to the single locale. If not, we construct
  // the composite string.
  if (is_uniform) {
    loc->composite_lc_all = first;
  } else {
    std::stringstream ss;
    for (int i = 0; i < LC_ALL; ++i) {
      if (i > 0) {
        ss << kCompositeSeparator;
      }
      ss << GetCategoryName(i) << kCompositeAssign << loc->categories[i];
    }
    loc->composite_lc_all = ss.str();
  }
}

void UpdateLocaleSettings(int mask, const char* locale, LocaleImpl* base) {
  if (mask == kAllValidCategoriesMask) {
    base->categories.fill(locale);
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

}  // namespace cobalt
