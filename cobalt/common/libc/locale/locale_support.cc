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

#include <langinfo.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <sstream>

#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/ucnv.h"

namespace {
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
  // TODO: b/462446756 - Properly support the "as_IN" locale.
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

}  // namespace

PrivateLocale* GetGlobalLocale() {
  static PrivateLocale g_current_locale;
  return &g_current_locale;
}

bool IsValidCategory(int category) {
  switch (category) {
    case LC_ALL:
    case LC_COLLATE:
    case LC_CTYPE:
    case LC_MONETARY:
    case LC_NUMERIC:
    case LC_TIME:
    case LC_MESSAGES:
      return true;
    default:
      return false;
  }
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

std::string GetCanonicalLocale(const char* inputLocale) {
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

bool ParseCompositeLocale(const char* input,
                          const PrivateLocale& current_state,
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
    out_categories[i] = current_state.categories[i];
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
    std::string canon = GetCanonicalLocale(val.c_str());
    if (canon.empty()) {
      return false;  // Error: Unsupported locale value found
    }

    // 7. Stage the Update
    out_categories[idx] = canon;
  }

  return true;
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
