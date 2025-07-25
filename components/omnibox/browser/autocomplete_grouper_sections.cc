// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_grouper_sections.h"

#include <algorithm>
#include <memory>

#include "base/dcheck_is_on.h"
#include "base/ranges/algorithm.h"
#include "components/omnibox/browser/autocomplete_grouper_groups.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/omnibox_proto/groups.pb.h"

namespace {
constexpr size_t kAndroidMostVisitedTilesLimit = 10;
}

Section::Section(size_t limit,
                 Groups groups,
                 omnibox::GroupConfigMap& group_configs)
    : limit_(limit), groups_(std::move(groups)) {
#if DCHECK_IS_ON()
  // Make sure all the `Group`s in the `Section` have the same `SideType` and
  // and all the `GroupId`s in the `Group`s have the same `RenderType`.
  absl::optional<omnibox::GroupConfig_SideType> last_side_type;
  for (const auto& group : groups_) {
    absl::optional<omnibox::GroupConfig_RenderType> last_render_type;
    for (const auto& [group_id, _] : group.group_id_limits_and_counts()) {
      const auto& group_config = group_configs[group_id];

      DCHECK(last_side_type.value_or(group_config.side_type()) ==
             group_config.side_type());
      last_side_type = group_config.side_type();

      DCHECK(last_render_type.value_or(group_config.render_type()) ==
             group_config.render_type());
      last_render_type = group_config.render_type();
    }
  }
#endif  // DCHECK_IS_ON()
}

Section::~Section() = default;

// static
ACMatches Section::GroupMatches(PSections sections, ACMatches& matches) {
  for (auto& section : sections) {
    section->InitFromMatches(matches);
  }

  for (const auto& match : matches) {
    DCHECK(match.suggestion_group_id.has_value());
    for (const auto& section : sections) {
      if (section->Add(match))
        break;
    }
  }

  ACMatches grouped_matches = {};
  for (const auto& section : sections) {
    for (const auto& group : section->groups_) {
      for (auto* match : group.matches()) {
        grouped_matches.push_back(std::move(*match));
      }
    }
  }

  return grouped_matches;
}

Groups::iterator Section::FindGroup(const AutocompleteMatch& match) {
  return base::ranges::find_if(
      groups_, [&](const auto& group) { return group.CanAdd(match); });
}

bool Section::Add(const AutocompleteMatch& match) {
  if (count_ >= limit_) {
    return false;
  }
  auto group_itr = FindGroup(match);
  if (group_itr == groups_.end()) {
    return false;
  }
  group_itr->Add(match);
  count_++;
  return true;
}

ZpsSection::ZpsSection(size_t limit,
                       Groups groups,
                       omnibox::GroupConfigMap& group_configs)
    : Section(limit, groups, group_configs) {}

void ZpsSection::InitFromMatches(ACMatches& matches) {
  // Sort matches in the order of their potential containing groups. E.g., if
  // `groups_ = {group 1, group 2}, this sorts all matches that can be added to
  // group 1 before those that can only be added to group 2.
  base::ranges::stable_sort(matches, std::less<int>{}, [&](const auto& match) {
    // Don't have to handle `FindGroup()` returning `groups_.end()` since
    // those matches won't be added to the section anyways.
    return std::distance(groups_.begin(), FindGroup(match));
  });
}

AndroidNTPZpsSection::AndroidNTPZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSection(
          30,
          {
              {1, omnibox::GROUP_MOBILE_CLIPBOARD},
              {15, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
              {OmniboxFieldTrial::kQueryTilesShowAboveTrends.Get() ? 10u : 0u,
               omnibox::GROUP_MOBILE_QUERY_TILES},
              {5, omnibox::GROUP_TRENDS},
              {OmniboxFieldTrial::kQueryTilesShowAboveTrends.Get() ? 0u : 10u,
               omnibox::GROUP_MOBILE_QUERY_TILES},
          },
          group_configs) {}

AndroidSRPZpsSection::AndroidSRPZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSection(15,
                 {
                     {1, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX},
                     {1, omnibox::GROUP_MOBILE_CLIPBOARD},
                     {15, omnibox::GROUP_PREVIOUS_SEARCH_RELATED},
                     {15, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
                 },
                 group_configs) {}

AndroidWebZpsSection::AndroidWebZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSection(15,  // Excludes MV tile count (calculated at runtime).
                 {
                     {1, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX},
                     {1, omnibox::GROUP_MOBILE_CLIPBOARD},
                     {kAndroidMostVisitedTilesLimit,
                      omnibox::GROUP_MOBILE_MOST_VISITED},
                     {8, omnibox::GROUP_VISITED_DOC_RELATED},
                     {15, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
                 },
                 group_configs) {}

void AndroidWebZpsSection::InitFromMatches(ACMatches& matches) {
  size_t tile_count = std::count_if(
      matches.begin(), matches.end(), [](const AutocompleteMatch& m) {
        return m.suggestion_group_id.value_or(omnibox::GROUP_INVALID) ==
               omnibox::GROUP_MOBILE_MOST_VISITED;
      });
  // In the event we find more MV tiles than we can accommodate, trim the limit.
  limit_ += std::min(tile_count, kAndroidMostVisitedTilesLimit);
  // Note that the horizontal render group takes a single slot in vertical list:
  // we therefore count it as an individual item, meaning this list:
  //     [ URL_WHAT_YOU_TYPED    ]
  //     [ [MV] [MV] [MV] [MV]   ]
  //     [ SEARCH_SUGGEST        ]
  // has 3 elements built from 6 AutocompleteMatch objects.
  limit_ -= (tile_count ? 1 : 0);
  ZpsSection::InitFromMatches(matches);
}

DesktopNTPZpsSection::DesktopNTPZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSection(8,
                 {{8, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
                  {8, omnibox::GROUP_TRENDS}},
                 group_configs) {}

DesktopSecondaryNTPZpsSection::DesktopSecondaryNTPZpsSection(
    size_t max_previous_search_related,
    omnibox::GroupConfigMap& group_configs)
    : ZpsSection(max_previous_search_related,
                 {{max_previous_search_related,
                   omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS}},
                 group_configs) {}

DesktopSRPZpsSection::DesktopSRPZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSection(8,
                 {{8, omnibox::GROUP_PREVIOUS_SEARCH_RELATED},
                  {8, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST}},
                 group_configs) {}

DesktopWebZpsSection::DesktopWebZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSection(8,
                 {{8, omnibox::GROUP_VISITED_DOC_RELATED},
                  {8, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST}},
                 group_configs) {}

DesktopNonZpsSection::DesktopNonZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : Section(10,
              {{1,
                {{omnibox::GROUP_STARTER_PACK, {1}},
                 {omnibox::GROUP_SEARCH, {1}},
                 {omnibox::GROUP_OTHER_NAVS, {1}}},
                /*is_default=*/true},
               {9, omnibox::GROUP_STARTER_PACK},
               {9,
                {{omnibox::GROUP_SEARCH, {9}},
                 {omnibox::GROUP_HISTORY_CLUSTER, {1}}}},
               {7, omnibox::GROUP_OTHER_NAVS}},
              group_configs) {}

void DesktopNonZpsSection::InitFromMatches(ACMatches& matches) {
  auto& default_group = groups_[0];
  auto& search_group = groups_[2];
  auto& nav_group = groups_[3];

  // Determine if `matches` contains any searches.
  bool has_search = base::ranges::any_of(
      matches, [&](const auto& match) { return search_group.CanAdd(match); });

  // Determine if the default match will be a search.
  auto default_match = base::ranges::find_if(
      matches, [&](const auto& match) { return default_group.CanAdd(match); });
  bool default_is_search =
      default_match != matches.end() && search_group.CanAdd(*default_match);

  // Find the 1st nav's index.
  size_t first_nav_index = std::distance(
      matches.begin(), base::ranges::find_if(matches, [&](const auto& match) {
        return nav_group.CanAdd(match);
      }));

  // Show at most 8 suggestions if doing so includes navs; otherwise show 9 or
  // 10, if doing so doesn't include navs.
  limit_ = std::clamp<size_t>(first_nav_index, 8, 10);

  // Show at least 1 search, either in the default group or the search group.
  if (has_search && !default_is_search) {
    DCHECK_GE(limit_, 2U);
    nav_group.set_limit(limit_ - 2);
  }
}

IOSNTPZpsSection::IOSNTPZpsSection(size_t max_trending_queries,
                                   size_t max_psuggest_queries,
                                   omnibox::GroupConfigMap& group_configs)
    : ZpsSection(
          max_trending_queries + max_psuggest_queries + 1,
          {{1, omnibox::GROUP_MOBILE_CLIPBOARD},
           {max_psuggest_queries, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
           {max_trending_queries, omnibox::GROUP_TRENDS}},
          group_configs) {}

IOSSRPZpsSection::IOSSRPZpsSection(omnibox::GroupConfigMap& group_configs)
    : ZpsSection(20,
                 {
                     // Verbatim match:
                     {1, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX},
                     {1, omnibox::GROUP_MOBILE_CLIPBOARD},
                     {1, omnibox::GROUP_MOBILE_MOST_VISITED},
                     {8, omnibox::GROUP_PREVIOUS_SEARCH_RELATED},
                     {20, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
                 },
                 group_configs) {}

IOSWebZpsSection::IOSWebZpsSection(omnibox::GroupConfigMap& group_configs)
    : ZpsSection(20,
                 {
                     // Verbatim match:
                     {1, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX},
                     {1, omnibox::GROUP_MOBILE_CLIPBOARD},
                     {1, omnibox::GROUP_MOBILE_MOST_VISITED},
                     {8, omnibox::GROUP_VISITED_DOC_RELATED},
                     {20, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
                 },
                 group_configs) {}

IOSIpadNTPZpsSection::IOSIpadNTPZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSection(10,
                 {
                     {1, omnibox::GROUP_MOBILE_CLIPBOARD},
                     {10, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
                 },
                 group_configs) {}

IOSIpadSRPZpsSection::IOSIpadSRPZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSection(10,
                 {
                     // Verbatim match:
                     {1, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX},
                     {1, omnibox::GROUP_MOBILE_CLIPBOARD},
                     {1, omnibox::GROUP_MOBILE_MOST_VISITED},
                     {8, omnibox::GROUP_PREVIOUS_SEARCH_RELATED},
                     {10, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
                 },
                 group_configs) {}

IOSIpadWebZpsSection::IOSIpadWebZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSection(10,
                 {
                     // Verbatim match:
                     {1, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX},
                     {1, omnibox::GROUP_MOBILE_CLIPBOARD},
                     {1, omnibox::GROUP_MOBILE_MOST_VISITED},
                     {8, omnibox::GROUP_VISITED_DOC_RELATED},
                     {10, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
                 },
                 group_configs) {}
