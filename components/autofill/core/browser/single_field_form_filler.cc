// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/single_field_form_filler.h"

namespace autofill {

SingleFieldFormFiller::SingleFieldFormFiller() = default;

SingleFieldFormFiller::~SingleFieldFormFiller() = default;

SingleFieldFormFiller::QueryHandler::QueryHandler(
    FieldGlobalId field_id,
    AutoselectFirstSuggestion autoselect_first_suggestion,
    std::u16string prefix,
    base::WeakPtr<SuggestionsHandler> handler)
    : field_id_(field_id),
      autoselect_first_suggestion_(autoselect_first_suggestion),
      prefix_(prefix),
      handler_(std::move(handler)) {}

SingleFieldFormFiller::QueryHandler::QueryHandler(
    const QueryHandler& original) = default;

SingleFieldFormFiller::QueryHandler::~QueryHandler() = default;

}  // namespace autofill
