// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_METRICS_CONSTANTS_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_METRICS_CONSTANTS_H_

namespace omnibox {

// Histogram name which counts the number of times the user enters
// keyword hint mode and via what method.  The possible values are listed
// in the metrics OmniboxEnteredKeywordMode2 enum which is defined in metrics
// enum XML file.
inline constexpr char kEnteredKeywordModeHistogram[] =
    "Omnibox.EnteredKeywordMode2";

// Like `kEnteredKeywordModeHistogram` but only logged when user has additional
// text input at the time keyword mode was entered.
inline constexpr char kEnteredKeywordModeWithInputHistogram[] =
    "Omnibox.EnteredKeywordModeWithInput";

// Histogram name which counts the number of times the user completes a search
// in keyword mode, enumerated by how they enter keyword mode.
inline constexpr char kAcceptedKeywordSuggestionHistogram[] =
    "Omnibox.AcceptedKeywordSuggestion";

// Histogram name which counts the number of times the user enters the keyword
// mode, enumerated by the type of search engine.
inline constexpr char kKeywordModeUsageByEngineTypeEnteredHistogramName[] =
    "Omnibox.KeywordModeUsageByEngineType.Entered";

// Histogram name which counts the number of times the user completes a search
// in keyword mode, enumerated by the type of search engine.
inline constexpr char kKeywordModeUsageByEngineTypeAcceptedHistogramName[] =
    "Omnibox.KeywordModeUsageByEngineType.Accepted";

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_METRICS_CONSTANTS_H_
