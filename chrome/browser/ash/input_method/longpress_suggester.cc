// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/longpress_suggester.h"

#include "chrome/browser/ash/input_method/suggestion_handler_interface.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::input_method {

LongpressSuggester::LongpressSuggester(
    SuggestionHandlerInterface* suggestion_handler)
    : suggestion_handler_(suggestion_handler) {}

LongpressSuggester::~LongpressSuggester() = default;

void LongpressSuggester::OnFocus(int context_id) {
  Reset();
  focused_context_id_ = context_id;
}

void LongpressSuggester::OnBlur() {
  focused_context_id_ = absl::nullopt;
  Reset();
}

void LongpressSuggester::OnExternalSuggestionsUpdated(
    const std::vector<ime::AssistiveSuggestion>& suggestions) {
  // Clipboard history updates are handled elsewhere, and diacritics suggestions
  // are not updated externally.
  return;
}

bool LongpressSuggester::HasSuggestions() {
  // Unused.
  return false;
}

std::vector<ime::AssistiveSuggestion> LongpressSuggester::GetSuggestions() {
  // Unused.
  return {};
}

}  // namespace ash::input_method
