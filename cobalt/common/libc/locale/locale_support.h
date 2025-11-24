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

#include <string>
#include <vector>

enum CobaltLocaleCategoryIndex {
  kCobaltLcCtype = 0,
  kCobaltLcNumeric = 1,
  kCobaltLcTime = 2,
  kCobaltLcCollate = 3,
  kCobaltLcMonetary = 4,
  kCobaltLcMessages = 5,
  kCobaltLcCount = 6  // Total size of LocaleImpl's internal array
};

// The private structure used to store locale state.
// Exposed here so locale.cc can instantiate it.
struct LocaleImpl {
  // Slots for specific categories (0 to LC_ALL-1)
  std::string categories[kCobaltLcCount];

  // NEW: Dedicated slot for the LC_ALL query result
  std::string composite_lc_all;

  LocaleImpl() {
    // Initialize specific categories to "C"
    for (int i = 0; i < kCobaltLcCount; ++i) {
      categories[i] = "C";
    }
    // Initialize the composite state to "C" (Uniform state)
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
bool ParseCompositeLocale(const char* input,
                          const LocaleImpl& current_state,
                          std::vector<std::string>& out_categories);

// Updates a LocaleImpl's composite string value to the values stored inside the
// categories.
void RefreshCompositeString(LocaleImpl* loc);

// Updates the LocaleImpl struct based on a given mask and locale string.
void UpdateLocaleSettings(int mask, const char* locale, LocaleImpl* base);

#endif  // COBALT_COMMON_LIBC_LOCALE_LOCALE_SUPPORT_H_
