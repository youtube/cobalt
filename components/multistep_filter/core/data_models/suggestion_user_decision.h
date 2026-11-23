// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_SUGGESTION_USER_DECISION_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_SUGGESTION_USER_DECISION_H_

namespace multistep_filter {

// The user's decision upon interacting with a Multistep Filter suggestion.
enum class SuggestionUserDecision {
  kAccepted,
  kDismissed,
  kIgnored,
  kSettingsOpened,
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_SUGGESTION_USER_DECISION_H_
