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

#ifndef COBALT_COMMON_LIBC_LOCALE_LOCALE_SUPPORT_H_
#define COBALT_COMMON_LIBC_LOCALE_LOCALE_SUPPORT_H_

#include <locale.h>

#include <array>
#include <string>
#include <vector>

// Enums values that represent the corresponding LC type inside
// LocaleImpl::categories. For example categories[kCobaltLcCtype] stores the
// locale string for LC_CTYPE.
enum CobaltLocaleCategoryIndex {
  kCobaltLcCtype = 0,
  kCobaltLcNumeric = 1,
  kCobaltLcTime = 2,
  kCobaltLcCollate = 3,
  kCobaltLcMonetary = 4,
  kCobaltLcMessages = 5,
  kCobaltLcCount = 6  // Total size of LocaleImpl's internal array
};

// LocaleImpl is the struct that will store the state of any give locale.
// It stores the set locales for all LC categories, along with a composite
// string that represents the state of LC_ALL. Upon initialization, all values
// are set to the default "C" locale.
struct LocaleImpl {
  std::array<std::string, kCobaltLcCount> categories;

  // From the PUBS open group documentation on setlocale:
  //
  // The string returned by setlocale() is such that a subsequent call with that
  // string and its associated category shall restore that part of the program's
  // locale.
  //
  // This requirement gets tricky when a call such as setlocale(LC_ALL, nullptr)
  // is called. In the event that the categories are in a mixed state (LC_CTYPE
  // = en_US but LC_NUMERIC = sr_RS), we must return a string that if setlocale
  // is called again with this mixed string, will properly restore all the
  // categories to their original state when setlocale(LC_ALL, nullptr) was
  // called.
  //
  // To address this, we store all the states of the categories inside
  // |composite_lc_all|. If all the categories are the same, e.g. "C",
  // |composite_lc_all| will just hold the value "C". However, if the categories
  // are not all the same value, then |composite_lc_all| will store all the
  // categories and their values, following the pattern of
  // category1=value;category2=value etc. Our locale functions are then able to
  // parse and construct this string, ensuring that the representaition of
  // LC_ALL is accurate at all times.
  std::string composite_lc_all;

  // On initialization, all LocaleImpl objects are set to the "C" locale (all
  // conforming systems must support this locale).
  LocaleImpl() {
    categories.fill("C");
    composite_lc_all = "C";
  }
};

// Returns the mapping of an LC category to the index storing the category
// inside LocaleImpl.
int SystemToCobaltIndex(int system_category);

// Validates, Canonicalizes, and Reconstructs a  given locale string to POSIX
// format. Returns empty string on failure.
std::string GetCanonicalLocale(const char* input_locale);

// Parses a composite string like "LC_CTYPE=en_US;LC_TIME=fr_FR..."
// Returns true on success, updating out_categories.
bool ParseCompositeLocale(
    const char* input,
    const LocaleImpl& current_state,
    std::array<std::string, kCobaltLcCount>& out_categories);

// Updates a LocaleImpl's composite string value to the values stored inside the
// categories.
void RefreshCompositeString(LocaleImpl* loc);

// Updates the LocaleImpl struct based on a given mask and locale string.
void UpdateLocaleSettings(int mask, const char* locale, LocaleImpl* base);

#endif  // COBALT_COMMON_LIBC_LOCALE_LOCALE_SUPPORT_H_
