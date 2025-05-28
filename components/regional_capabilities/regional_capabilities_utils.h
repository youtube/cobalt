// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_UTILS_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_UTILS_H_

#include <optional>
#include <variant>

namespace regional_capabilities {

// Returns whether `country_id` refers to a country member of the European
// Economic Area.
// See `//components/country_codes` for the Country ID format.
bool IsEeaCountry(int country_id);

// The state of the search engine choice country command line override.
// See `switches::kSearchEngineChoiceCountry`.
enum class SearchEngineCountryListOverride {
  // Display all the search engines used in the EEA region.
  kEeaAll,
  // Display the search engines that we default to when the country is unknown.
  kEeaDefault,
};

using SearchEngineCountryOverride =
    std::variant<int, SearchEngineCountryListOverride>;

// Gets the search engine country command line override.
// Returns an int if the country id is passed to the command line or a
// `SearchEngineCountryListOverride` if the special values of
// `kDefaultListCountryOverride` or `kEeaListCountryOverride` are passed.
std::optional<SearchEngineCountryOverride> GetSearchEngineCountryOverride();

// Returns whether the search engine list is overridden in the command line to
// return the default list or the list of all eea engines.
bool HasSearchEngineCountryListOverride();

}  // namespace regional_capabilities

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_UTILS_H_
