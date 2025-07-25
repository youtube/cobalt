// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_SIGNALS_ANNOTATOR_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_SIGNALS_ANNOTATOR_H_

#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

class AutocompleteInput;
class AutocompleteResult;

// Base class for annotating suggestions in autocomplete results with various
// signals for ML training and scoring.
class AutocompleteScoringSignalsAnnotator {
 public:
  using ScoringSignals =
      ::metrics::OmniboxEventProto::Suggestion::ScoringSignals;

  // Whether the autocomplete match is eligible to be annotated.
  // Currently, includes only history and bookmark URLs.
  static bool IsEligibleMatch(const AutocompleteMatch& match) {
    const auto& ml_config = OmniboxFieldTrial::GetMLConfig();
    return match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED ||
           match.type == AutocompleteMatchType::HISTORY_URL ||
           match.type == AutocompleteMatchType::HISTORY_TITLE ||
           match.type == AutocompleteMatchType::BOOKMARK_TITLE ||
           (ml_config.shortcut_document_signals &&
            match.type == AutocompleteMatchType::DOCUMENT_SUGGESTION);
  }

  AutocompleteScoringSignalsAnnotator() = default;
  AutocompleteScoringSignalsAnnotator(
      const AutocompleteScoringSignalsAnnotator&) = delete;
  AutocompleteScoringSignalsAnnotator& operator=(
      const AutocompleteScoringSignalsAnnotator&) = delete;
  virtual ~AutocompleteScoringSignalsAnnotator() = default;

  // Annotate the autocomplete result.
  virtual void AnnotateResult(const AutocompleteInput& input,
                              AutocompleteResult* result) = 0;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_SIGNALS_ANNOTATOR_H_
