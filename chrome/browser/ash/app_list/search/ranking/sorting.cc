// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/sorting.h"

namespace app_list {

namespace {

bool CategoryAffectsScore(const ChromeSearchResult* result) {
  return result->display_type() == ash::SearchResultDisplayType::kList;
}

}  // namespace

void SortCategories(CategoriesList& categories) {
  // Sort categories first by burn-in iteration number, then by score.
  std::sort(categories.begin(), categories.end(),
            [](const auto& a, const auto& b) {
              const int a_burn_in = a.burn_in_iteration;
              const int b_burn_in = b.burn_in_iteration;
              if (a_burn_in != b_burn_in) {
                // Sort order: 0, 1, 2, 3, ... then -1.
                // The effect of this is to sort by arrival order, with unseen
                // categories ranked last.
                // N.B. (a ^ b) < 0 checks for opposite sign.
                return (a_burn_in ^ b_burn_in) < 0 ? a_burn_in > b_burn_in
                                                   : a_burn_in < b_burn_in;
              } else if (a.category == Category::kSearchAndAssistant ||
                         b.category == Category::kSearchAndAssistant) {
                // Special-case the search and assistant category, which should
                // be sorted last, when burn-in iteration numbers are equal.
                return b.category == Category::kSearchAndAssistant;
              } else {
                return a.score > b.score;
              }
            });
}

void SortResults(std::vector<ChromeSearchResult*>& results,
                 const CategoriesList& categories) {
  std::sort(
      results.begin(), results.end(),
      [&](const ChromeSearchResult* a, const ChromeSearchResult* b) {
        const int a_best_match_rank = a->scoring().best_match_rank();
        const int b_best_match_rank = b->scoring().best_match_rank();
        if (a_best_match_rank != b_best_match_rank) {
          // First, sort by best match. All best matches are brought to
          // the front of the list and ordered by best_match_rank.
          //
          // The following gives sort order:
          //    0, 1, 2, ... (best matches) then -1 (non-best matches).
          // N.B. (a ^ b) < 0 checks for opposite sign.
          return (a_best_match_rank ^ b_best_match_rank) < 0
                     ? a_best_match_rank > b_best_match_rank
                     : a_best_match_rank < b_best_match_rank;
        }

        const bool ignore_categories =
            !CategoryAffectsScore(a) || !CategoryAffectsScore(b);
        if (a_best_match_rank == -1 && !ignore_categories &&
            a->category() != b->category()) {
          // Next, sort by categories, except for within best match.
          // |categories_| has been sorted above so the first category in
          // |categories_| should be ranked more highly.
          for (const auto& category : categories) {
            if (category.category == a->category()) {
              return true;
            }
            if (category.category == b->category()) {
              return false;
            }
          }
          // Any category associated with a result should also be present
          // in |categories_|.
          NOTREACHED();
          return false;
        }

        if (a->scoring().burn_in_iteration() !=
            b->scoring().burn_in_iteration()) {
          // Next, sort by burn-in iteration number. This has no effect on
          // results which arrive pre-burn-in. For post-burn-in results
          // for a given category, later-arriving results are placed below
          // earlier-arriving results.
          // This happens before sorting on display_score, as a trade-off
          // between ranking accuracy and UX pop-in mitigation.
          return a->scoring().burn_in_iteration() <
                 b->scoring().burn_in_iteration();
        }

        if (a->scoring().continue_rank() != b->scoring().continue_rank()) {
          return a->scoring().continue_rank() > b->scoring().continue_rank();
        }

        // Lastly, sort by display score.
        return a->display_score() > b->display_score();
      });
}

}  // namespace app_list
