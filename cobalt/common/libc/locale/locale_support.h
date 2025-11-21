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

// The private structure used to store locale state.
// Exposed here so locale.cc can instantiate it.
struct PrivateLocale {
  // Slots for specific categories (0 to LC_ALL-1)
  std::string categories[LC_ALL];

  // NEW: Dedicated slot for the LC_ALL query result
  std::string composite_lc_all;

  PrivateLocale() {
    // Initialize specific categories to "C"
    for (int i = 0; i < LC_ALL; ++i) {
      categories[i] = "C";
    }
    // Initialize the composite state to "C" (Uniform state)
    composite_lc_all = "C";
  }
};

// -----------------------------------------------------------------------------
// Global Locale Access
// -----------------------------------------------------------------------------
PrivateLocale* GetGlobalLocale();

// -----------------------------------------------------------------------------
// Validation & Conversion Helpers
// -----------------------------------------------------------------------------

// Checks if a category index is valid (0 to LC_ALL).
bool IsValidCategory(int category);

// Returns the string name of a category (e.g., "LC_TIME").
const char* GetCategoryName(int category);

// Returns the index of a category from its name.
int GetCategoryIndexFromName(const std::string& name);

// -----------------------------------------------------------------------------
// Core Logic Helpers
// -----------------------------------------------------------------------------

// Validates, Canonicalizes, and Reconstructs a locale string to POSIX format.
// Returns empty string on failure.
std::string GetCanonicalLocale(const char* input_locale);

// Parses a composite string like "LC_CTYPE=en_US;LC_TIME=fr_FR..."
// Returns true on success, updating out_categories.
bool ParseCompositeLocale(const char* input,
                          const PrivateLocale& current_state,
                          std::vector<std::string>& out_categories);

// Updates the PrivateLocale struct based on a mask and a locale string.
void UpdateLocaleSettings(int mask, const char* locale, PrivateLocale* base);

#endif  // COBALT_COMMON_LIBC_LOCALE_LOCALE_SUPPORT_H_
