// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_parsing_utils.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_types.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace plus_addresses {

namespace {

// Creates a PlusProfile for `dict` if it fits this schema (in TS notation):
// {
//   "ProfileId": string,
//   "facet": string,
//   "plusEmail": {
//     "plusAddress": string,
//     "plusMode": string,
//   }
// }
// Returns nullopt if none of the values are parsed.
std::optional<PlusProfile> ParsePlusProfileFromV1Dict(base::Value::Dict dict) {
  std::string profile_id;
  std::string facet_str;
  PlusAddress plus_address;
  std::optional<bool> is_confirmed;
  for (std::pair<const std::string&, base::Value&> entry : dict) {
    auto [key, val] = entry;
    if (base::MatchPattern(key, "*ProfileId") && val.is_string()) {
      profile_id = std::move(val.GetString());
      continue;
    }
    if (base::MatchPattern(key, "facet") && val.is_string()) {
      facet_str = std::move(val.GetString());
      continue;
    }
    if (base::MatchPattern(key, "*Email") && val.is_dict()) {
      for (std::pair<const std::string&, base::Value&> email_entry :
           val.GetDict()) {
        auto [email_key, email_val] = email_entry;
        if (!email_val.is_string()) {
          continue;
        }
        if (base::MatchPattern(email_key, "*Address")) {
          plus_address = PlusAddress(std::move(email_val.GetString()));
        }
        if (base::MatchPattern(email_key, "*Mode")) {
          is_confirmed =
              !base::MatchPattern(email_val.GetString(), "*UNSPECIFIED");
        }
      }
    }
  }
  if (profile_id.empty() || facet_str.empty() || plus_address->empty() ||
      !is_confirmed.has_value()) {
    return std::nullopt;
  }
  affiliations::FacetURI facet =
      affiliations::FacetURI::FromPotentiallyInvalidSpec(facet_str);
  if (!facet.is_valid()) {
    return std::nullopt;
  }
  return PlusProfile(std::move(profile_id), std::move(facet),
                     std::move(plus_address), *is_confirmed);
}

// Attempts to parse a string of format "[0-9]+s" into a `base::TimeDelta`.
std::optional<base::TimeDelta> ParseLifetime(std::string* str) {
  if (!str) {
    return std::nullopt;
  }
  if (str->empty() || !str->ends_with('s')) {
    return std::nullopt;
  }

  int64_t seconds;
  if (!base::StringToInt64(std::string_view(*str).substr(0, str->size() - 1),
                           &seconds)) {
    return std::nullopt;
  }
  return base::Seconds(seconds);
}

// Attempts to parse a `base::Value::Dict` into to a `PreallocatedPlusAddress`.
std::optional<PreallocatedPlusAddress> ParsePreallocatedPlusAddress(
    base::Value::Dict dict) {
  static constexpr std::string_view kAddressKey = "emailAddress";
  static constexpr std::string_view kLifetimeKey = "reservationLifetime";

  PlusAddress address;
  if (std::string* address_str = dict.FindString(kAddressKey)) {
    address = PlusAddress(std::move(*address_str));
  } else {
    return std::nullopt;
  }

  base::TimeDelta lifetime;
  if (std::optional<base::TimeDelta> time =
          ParseLifetime(dict.FindString(kLifetimeKey))) {
    lifetime = *time;
  } else {
    return std::nullopt;
  }

  return std::make_optional<PreallocatedPlusAddress>(std::move(address),
                                                     lifetime);
}

}  // namespace

std::optional<PlusProfile> ParsePlusProfileFromV1Create(
    data_decoder::DataDecoder::ValueOrError response) {
  if (!response.has_value() || !response->is_dict()) {
    return std::nullopt;
  }

  // Use iterators to avoid looking up by JSON keys.
  base::Value::List* existing_profiles = nullptr;
  for (std::pair<const std::string&, base::Value&> first_level_entry :
       response->GetDict()) {
    auto [first_key, first_val] = first_level_entry;
    if (base::MatchPattern(first_key, "*Profile") && first_val.is_dict()) {
      return ParsePlusProfileFromV1Dict(std::move(first_val.GetDict()));
    }
    if (base::MatchPattern(first_key, "existing*Profiles") &&
        first_val.is_list()) {
      existing_profiles = first_val.GetIfList();
    }
  }
  if (!existing_profiles || existing_profiles->empty() ||
      !base::FeatureList::IsEnabled(
          features::kPlusAddressParseExistingProfilesFromCreateResponse)) {
    return std::nullopt;
  }

  // If kPlusAddressParseExistingProfilesFromCreateResponse is enabled, there is
  // no entry for a newly created profile, but there is an entry for existing
  // profiles, then we return the first existing profile. This scenario can
  // occur if the client tries to create a plus profile on a domain for which
  // the server already has affiliated profiles. In theory, there could even be
  // multiple such profiles (due to changing affiliations over time), but to
  // save complexity, we currently just pick the first profile and return it.
  // At a later point, we may choose to add logic that picks one out of multiple
  // profiles further downstream - in that case, we would need to change the
  // signature of this function.
  base::Value::Dict* first_existing_profile =
      (*existing_profiles)[0].GetIfDict();
  return first_existing_profile
             ? ParsePlusProfileFromV1Dict(std::move(*first_existing_profile))
             : std::nullopt;
}

std::optional<std::vector<PreallocatedPlusAddress>>
ParsePreallocatedPlusAddresses(
    data_decoder::DataDecoder::ValueOrError response) {
  static constexpr std::string_view kAddressesKey = "emailAddresses";
  if (!response.has_value() || !response->is_dict()) {
    return std::nullopt;
  }
  base::Value::List* addresses = response->GetDict().FindList(kAddressesKey);
  if (!addresses) {
    return std::nullopt;
  }
  std::vector<PreallocatedPlusAddress> result;
  result.reserve(addresses->size());
  for (base::Value& entry : *addresses) {
    if (!entry.is_dict()) {
      continue;
    }
    if (std::optional<PreallocatedPlusAddress> address =
            ParsePreallocatedPlusAddress(std::move(entry.GetDict()))) {
      result.push_back(*std::move(address));
    }
  }
  return std::move(result);
}

}  // namespace plus_addresses
