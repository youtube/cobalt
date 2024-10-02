// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure_test_api.h"

#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/form_parsing/field_candidates.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::_;
using ::testing::Contains;
using ::testing::Each;
using ::testing::Pair;
using FieldPrediction =
    AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction;

// static
void FormStructureTestApi::SetFieldTypes(
    const std::vector<std::vector<std::pair<PatternSource, ServerFieldType>>>&
        heuristic_types,
    const std::vector<FieldPrediction>& server_types) {
  ASSERT_EQ(form_structure_->field_count(), heuristic_types.size());
  ASSERT_EQ(form_structure_->field_count(), server_types.size());
  ASSERT_THAT(heuristic_types,
              Each(Contains(Pair(GetActivePatternSource(), _))))
      << "There must be a default heuristic prediction for every field.";

  for (size_t i = 0; i < form_structure_->field_count(); ++i) {
    AutofillField* form_field = form_structure_->field(i);
    ASSERT_TRUE(form_field);

    for (const auto& [source, type] : heuristic_types[i])
      form_field->set_heuristic_type(source, type);
    form_field->set_server_predictions({server_types[i]});
  }

  form_structure_->UpdateAutofillCount();
}

void FormStructureTestApi::SetFieldTypes(
    const std::vector<std::vector<std::pair<PatternSource, ServerFieldType>>>&
        heuristic_types,
    const std::vector<ServerFieldType>& server_types) {
  std::vector<FieldPrediction> server_predictions;
  for (ServerFieldType type : server_types)
    server_predictions.push_back(::autofill::test::CreateFieldPrediction(type));
  SetFieldTypes(heuristic_types, server_predictions);
}

void FormStructureTestApi::SetFieldTypes(
    const std::vector<ServerFieldType>& heuristic_types,
    const std::vector<ServerFieldType>& server_types) {
  std::vector<std::vector<std::pair<PatternSource, ServerFieldType>>>
      all_heuristic_types;
  for (ServerFieldType type : heuristic_types)
    all_heuristic_types.push_back({{GetActivePatternSource(), type}});
  SetFieldTypes(all_heuristic_types, server_types);
}

}  // namespace autofill
