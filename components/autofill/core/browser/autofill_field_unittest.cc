// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_field.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/field_prediction_test_matchers.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::autofill::test::CreateFieldPrediction;
using ::autofill::test::EqualsPrediction;
using ::testing::ElementsAre;

class AutofillFieldTest : public testing::Test {
 public:
  AutofillFieldTest() = default;

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

TEST_F(AutofillFieldTest, ValueWasIdentifiedAsPotentiallySensitive) {
  AutofillField field;

  // Initially the value should not be identified as sensitive.
  EXPECT_FALSE(field.value_identified_as_potentially_sensitive());

  // We should be able to set the value and retrieve the state.
  field.set_value_identified_as_potentially_sensitive(true);
  EXPECT_TRUE(field.value_identified_as_potentially_sensitive());
}

TEST_F(AutofillFieldTest, FieldIsEligibleForAutofillAiFlag) {
  AutofillField field;

  // Initially the value should not be identified as sensitive.
  EXPECT_FALSE(field.field_is_eligible_for_autofill_ai().has_value());

  // Test that setting the value works.
  field.set_field_is_eligible_for_autofill_ai(true);
  ASSERT_TRUE(field.field_is_eligible_for_autofill_ai().has_value());
  EXPECT_TRUE(field.field_is_eligible_for_autofill_ai().value());

  field.set_field_is_eligible_for_autofill_ai(false);
  ASSERT_TRUE(field.field_is_eligible_for_autofill_ai().has_value());
  EXPECT_FALSE(field.field_is_eligible_for_autofill_ai().value());

  // Verify that the state can also be reset.
  field.set_field_is_eligible_for_autofill_ai(std::nullopt);
  EXPECT_FALSE(field.field_is_eligible_for_autofill_ai().has_value());
}

// Tests that if both autocomplete attributes and server agree it's a phone
// field, always use server predicted type. If they disagree with autocomplete
// says it's a phone field, always use autocomplete attribute.
TEST_F(AutofillFieldTest, Type_ServerPredictionOfCityAndNumber_OverrideHtml) {
  AutofillField field;

  field.SetHtmlType(HtmlFieldType::kTel, HtmlFieldMode::kNone);

  field.set_server_predictions(
      {CreateFieldPrediction(PHONE_HOME_CITY_AND_NUMBER)});
  EXPECT_EQ(PHONE_HOME_CITY_AND_NUMBER, field.Type().GetStorableType());

  // Overrides to another number format.
  field.set_server_predictions({CreateFieldPrediction(PHONE_HOME_NUMBER)});
  EXPECT_EQ(PHONE_HOME_NUMBER, field.Type().GetStorableType());

  // Overrides autocomplete=tel-national too.
  field.SetHtmlType(HtmlFieldType::kTelNational, HtmlFieldMode::kNone);
  field.set_server_predictions(
      {CreateFieldPrediction(PHONE_HOME_WHOLE_NUMBER)});
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER, field.Type().GetStorableType());

  // If autocomplete=tel-national but server says it's not a phone field,
  // do not override.
  field.SetHtmlType(HtmlFieldType::kTelNational, HtmlFieldMode::kNone);
  field.set_server_predictions(
      {CreateFieldPrediction(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR)});
  EXPECT_EQ(PHONE_HOME_CITY_AND_NUMBER, field.Type().GetStorableType());

  // If html type not specified, we still use server prediction.
  field.SetHtmlType(HtmlFieldType::kUnspecified, HtmlFieldMode::kNone);
  field.set_server_predictions(
      {CreateFieldPrediction(PHONE_HOME_CITY_AND_NUMBER)});
  EXPECT_EQ(PHONE_HOME_CITY_AND_NUMBER, field.Type().GetStorableType());
}

// Tests that a local heuristics prediction for `EMAIL_ADDRESS` overrides server
// predictions for `USERNAME` or `SINGLE_USERNAME`.
TEST_F(AutofillFieldTest, EmailOverridesUsernameType) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillGivePrecedenceToEmailOverUsername};
  AutofillField field;

  field.set_server_predictions({CreateFieldPrediction(USERNAME)});
  field.set_heuristic_type(GetActiveHeuristicSource(), EMAIL_ADDRESS);
  EXPECT_EQ(field.Type().GetStorableType(), EMAIL_ADDRESS);

  field.set_server_predictions({CreateFieldPrediction(SINGLE_USERNAME)});
  field.set_heuristic_type(GetActiveHeuristicSource(), EMAIL_ADDRESS);
  EXPECT_EQ(field.Type().GetStorableType(), EMAIL_ADDRESS);
}

TEST_F(AutofillFieldTest, IsFieldFillable) {
  AutofillField field;
  ASSERT_EQ(UNKNOWN_TYPE, field.Type().GetStorableType());

  // Type is unknown.
  EXPECT_FALSE(field.IsFieldFillable());

  // Only heuristic type is set.
  field.set_heuristic_type(GetActiveHeuristicSource(), NAME_FIRST);
  EXPECT_TRUE(field.IsFieldFillable());

  // Only server type is set.
  field.set_heuristic_type(GetActiveHeuristicSource(), UNKNOWN_TYPE);
  field.set_server_predictions({CreateFieldPrediction(NAME_LAST)});
  EXPECT_TRUE(field.IsFieldFillable());

  // Both types set.
  field.set_heuristic_type(GetActiveHeuristicSource(), NAME_FIRST);
  field.set_server_predictions({CreateFieldPrediction(NAME_LAST)});
  EXPECT_TRUE(field.IsFieldFillable());

  // Field has autocomplete="off" set. Since autofill was able to make a
  // prediction, it is still considered a fillable field.
  field.set_should_autocomplete(false);
  EXPECT_TRUE(field.IsFieldFillable());
}

class AutofillFieldWithAutofillAiTest : public base::test::WithFeatureOverride,
                                        public AutofillFieldTest {
 public:
  AutofillFieldWithAutofillAiTest()
      : base::test::WithFeatureOverride(features::kAutofillAiWithDataSchema) {}
};

// Tests that server prediction with SOURCE_AUTOFILL_AI are only added if
// `features::kAutofillAiWithDataSchema` is enabled.
TEST_P(AutofillFieldWithAutofillAiTest, SetAutofillAiPredictions) {
  AutofillField field;

  const FieldPrediction crowdsourcing_prediction = CreateFieldPrediction(
      NAME_FIRST, FieldPrediction::SOURCE_AUTOFILL_DEFAULT);
  const FieldPrediction ai_prediction =
      CreateFieldPrediction(NAME_FIRST, FieldPrediction::SOURCE_AUTOFILL_AI);
  field.set_server_predictions({crowdsourcing_prediction, ai_prediction});

  if (IsParamFeatureEnabled()) {
    EXPECT_THAT(field.server_predictions(),
                ElementsAre(EqualsPrediction(crowdsourcing_prediction),
                            EqualsPrediction(ai_prediction)));
  } else {
    EXPECT_THAT(field.server_predictions(),
                ElementsAre(EqualsPrediction(crowdsourcing_prediction)));
  }
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(AutofillFieldWithAutofillAiTest);

// Parameters for `PrecedenceOverAutocompleteTest`
struct PrecedenceOverAutocompleteParams {
  // These values denote what type the field was viewed by html, server and
  // heuristic prediction.
  const HtmlFieldType html_field_type;
  const FieldType server_type;
  const FieldType heuristic_type;
  // This value denotes what `ComputedType` should return as field type.
  const FieldType expected_result;
};

class PrecedenceOverAutocompleteTest
    : public testing::TestWithParam<PrecedenceOverAutocompleteParams> {};

// Tests giving StreetName or HouseNumber predictions, by heuristic or server,
// precedence over HtmlFieldType::kAddressLine(1|2) autocomplete prediction.
TEST_P(PrecedenceOverAutocompleteTest, PrecedenceOverAutocompleteParams) {
  PrecedenceOverAutocompleteParams test_case = GetParam();
  AutofillField field;
  field.SetHtmlType(test_case.html_field_type, HtmlFieldMode::kNone);
  field.set_server_predictions({CreateFieldPrediction(test_case.server_type)});
  field.set_heuristic_type(GetActiveHeuristicSource(),
                           test_case.heuristic_type);
  EXPECT_EQ(test_case.expected_result, field.ComputedType().GetStorableType());
}

INSTANTIATE_TEST_SUITE_P(
    AutofillFieldTest,
    PrecedenceOverAutocompleteTest,
    testing::Values(
        PrecedenceOverAutocompleteParams{
            .html_field_type = HtmlFieldType::kAddressLine2,
            .server_type = ADDRESS_HOME_STREET_NAME,
            .heuristic_type = ADDRESS_HOME_LINE2,
            .expected_result = ADDRESS_HOME_STREET_NAME},

        PrecedenceOverAutocompleteParams{
            .html_field_type = HtmlFieldType::kAddressLine1,
            .server_type = ADDRESS_HOME_LINE1,
            .heuristic_type = ADDRESS_HOME_HOUSE_NUMBER,
            .expected_result = ADDRESS_HOME_HOUSE_NUMBER},

        PrecedenceOverAutocompleteParams{
            .html_field_type = HtmlFieldType::kGivenName,
            .server_type = ADDRESS_HOME_STREET_NAME,
            .heuristic_type = ADDRESS_HOME_HOUSE_NUMBER,
            .expected_result = NAME_FIRST}));

// Tests ensuring that ac=unrecognized fields receive predictions.
// For such fields, suggestions and filling is suppressed, which is indicated by
// a function `AutofillField::ShouldSuppressSuggestionsAndFillingByDefault()`.
// Every test specifies the predicted type for a field and what the expected
// return value of the aforementioned function is.
struct AutocompleteUnrecognizedTypeTestCase {
  // Either server or heuristic type - this doesn't matter for these tests.
  FieldType predicted_type;
  // If the predicted type should be treated as a server overwrite. Server
  // overwrites already take precedence over ac=unrecognized.
  bool is_server_overwrite = false;
  // Expected value of `ShouldSuppressSuggestionsAndFillingByDefault()`.
  bool expect_should_suppress_suggestions_and_filling;
};

class AutocompleteUnrecognizedTypeTest
    : public testing::TestWithParam<AutocompleteUnrecognizedTypeTestCase> {};

TEST_P(AutocompleteUnrecognizedTypeTest, TypePredictions) {
  // Create a field with ac=unrecognized and the specified predicted type.
  const AutocompleteUnrecognizedTypeTestCase& test = GetParam();
  AutofillField field;
  field.set_server_predictions(
      {CreateFieldPrediction(test.predicted_type, test.is_server_overwrite)});
  field.SetHtmlType(HtmlFieldType::kUnrecognized, HtmlFieldMode::kNone);

  // Expect that the predicted type wins over ac=unrecognized.
  EXPECT_EQ(field.Type().GetStorableType(), test.predicted_type);
  EXPECT_EQ(field.ShouldSuppressSuggestionsAndFillingByDefault(),
            test.expect_should_suppress_suggestions_and_filling);
}

INSTANTIATE_TEST_SUITE_P(
    AutofillFieldTest,
    AutocompleteUnrecognizedTypeTest,
    testing::Values(
        // Predicted address type: Expect no suggestions/filling.
        AutocompleteUnrecognizedTypeTestCase{
            .predicted_type = ADDRESS_HOME_CITY,
            .expect_should_suppress_suggestions_and_filling = true},
        // Server overwrite: Expect suggestions/filling.
        AutocompleteUnrecognizedTypeTestCase{
            .predicted_type = ADDRESS_HOME_CITY,
            .is_server_overwrite = true,
            .expect_should_suppress_suggestions_and_filling = false},
        // Credit card prediction: They ignore ac=unrecognized independently of
        // the feature. Thus, expect suggestions/filling.
        AutocompleteUnrecognizedTypeTestCase{
            .predicted_type = CREDIT_CARD_NUMBER,
            .expect_should_suppress_suggestions_and_filling = false}));

// Parameters for `AutofillLocalHeuristicsOverridesTest`
struct AutofillLocalHeuristicsOverridesParams {
  // These values denote what type the field was classified as html, server and
  // heuristic prediction.
  const HtmlFieldType html_field_type;
  const FieldType server_type;
  const FieldType heuristic_type;
  // This value denotes what `ComputedType` should return as field type.
  const FieldType expected_result;
};

class AutofillLocalHeuristicsOverridesTest
    : public testing::TestWithParam<AutofillLocalHeuristicsOverridesParams> {
 public:
  AutofillLocalHeuristicsOverridesTest() = default;
};

// Tests the correctness of local heuristic overrides while computing the
// overall field type.
TEST_P(AutofillLocalHeuristicsOverridesTest,
       AutofillLocalHeuristicsOverridesParams) {
  AutofillLocalHeuristicsOverridesParams test_case = GetParam();
  AutofillField field;
  field.SetHtmlType(test_case.html_field_type, HtmlFieldMode::kNone);
  field.set_server_predictions({CreateFieldPrediction(test_case.server_type)});
  field.set_heuristic_type(GetActiveHeuristicSource(),
                           test_case.heuristic_type);
  EXPECT_EQ(test_case.expected_result, field.ComputedType().GetStorableType())
      << "html_field_type: " << test_case.html_field_type
      << ", server_type: " << test_case.server_type
      << ", heuristic_type: " << test_case.heuristic_type
      << ", expected_result: " << test_case.expected_result;
}

INSTANTIATE_TEST_SUITE_P(
    AutofillHeuristicsOverrideTest,
    AutofillLocalHeuristicsOverridesTest,
    testing::Values(
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = ADDRESS_HOME_CITY,
            .heuristic_type = ADDRESS_HOME_ADMIN_LEVEL2,
            .expected_result = ADDRESS_HOME_ADMIN_LEVEL2},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = ADDRESS_HOME_HOUSE_NUMBER,
            .heuristic_type = ADDRESS_HOME_APT_NUM,
            .expected_result = ADDRESS_HOME_APT_NUM},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = ADDRESS_HOME_STREET_ADDRESS,
            .heuristic_type = ADDRESS_HOME_BETWEEN_STREETS,
            .expected_result = ADDRESS_HOME_BETWEEN_STREETS},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = ADDRESS_HOME_LINE1,
            .heuristic_type = ADDRESS_HOME_BETWEEN_STREETS,
            .expected_result = ADDRESS_HOME_BETWEEN_STREETS},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = ADDRESS_HOME_LINE2,
            .heuristic_type = ADDRESS_HOME_BETWEEN_STREETS,
            .expected_result = ADDRESS_HOME_BETWEEN_STREETS},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAddressLevel1,
            .server_type = ADDRESS_HOME_STREET_ADDRESS,
            .heuristic_type = ADDRESS_HOME_ADMIN_LEVEL2,
            .expected_result = ADDRESS_HOME_ADMIN_LEVEL2},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAddressLine2,
            .server_type = ADDRESS_HOME_STREET_ADDRESS,
            .heuristic_type = ADDRESS_HOME_APT_NUM,
            .expected_result = ADDRESS_HOME_APT_NUM},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAddressLevel2,
            .server_type = ADDRESS_HOME_STREET_ADDRESS,
            .heuristic_type = ADDRESS_HOME_BETWEEN_STREETS,
            .expected_result = ADDRESS_HOME_BETWEEN_STREETS},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAddressLevel1,
            .server_type = ADDRESS_HOME_STREET_ADDRESS,
            .heuristic_type = ADDRESS_HOME_DEPENDENT_LOCALITY,
            .expected_result = ADDRESS_HOME_DEPENDENT_LOCALITY},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAddressLine1,
            .server_type = ADDRESS_HOME_STREET_ADDRESS,
            .heuristic_type = ADDRESS_HOME_DEPENDENT_LOCALITY,
            .expected_result = ADDRESS_HOME_DEPENDENT_LOCALITY},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAddressLine2,
            .server_type = ADDRESS_HOME_LINE2,
            .heuristic_type = ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
            .expected_result = ADDRESS_HOME_OVERFLOW_AND_LANDMARK},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAddressLine2,
            .server_type = ADDRESS_HOME_LINE2,
            .heuristic_type = ADDRESS_HOME_OVERFLOW,
            .expected_result = ADDRESS_HOME_OVERFLOW},
        // Test non-override behaviour.
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kStreetAddress,
            .server_type = ADDRESS_HOME_STREET_ADDRESS,
            .heuristic_type = ADDRESS_HOME_STREET_ADDRESS,
            .expected_result = ADDRESS_HOME_STREET_ADDRESS},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = ADDRESS_HOME_CITY,
            .heuristic_type = ADDRESS_HOME_APT_NUM,
            .expected_result = ADDRESS_HOME_CITY},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = ADDRESS_HOME_HOUSE_NUMBER,
            .heuristic_type = ADDRESS_HOME_HOUSE_NUMBER_AND_APT,
            .expected_result = ADDRESS_HOME_HOUSE_NUMBER_AND_APT},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = ADDRESS_HOME_APT_NUM,
            .heuristic_type = ADDRESS_HOME_HOUSE_NUMBER_AND_APT,
            .expected_result = ADDRESS_HOME_HOUSE_NUMBER_AND_APT},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = NAME_FULL,
            .heuristic_type = ALTERNATIVE_FULL_NAME,
            .expected_result = ALTERNATIVE_FULL_NAME},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = NAME_FIRST,
            .heuristic_type = ALTERNATIVE_GIVEN_NAME,
            .expected_result = ALTERNATIVE_GIVEN_NAME},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = NAME_LAST,
            .heuristic_type = ALTERNATIVE_FAMILY_NAME,
            .expected_result = ALTERNATIVE_FAMILY_NAME},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = NAME_LAST_SECOND,
            .heuristic_type = ALTERNATIVE_FAMILY_NAME,
            .expected_result = ALTERNATIVE_FAMILY_NAME},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = NAME_LAST_CORE,
            .heuristic_type = ALTERNATIVE_FAMILY_NAME,
            .expected_result = ALTERNATIVE_FAMILY_NAME},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAdditionalName,
            .server_type = NAME_LAST_PREFIX,
            .heuristic_type = NAME_LAST_PREFIX,
            .expected_result = NAME_LAST_PREFIX},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAdditionalNameInitial,
            .server_type = NAME_LAST_PREFIX,
            .heuristic_type = NAME_LAST_PREFIX,
            .expected_result = NAME_LAST_PREFIX},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kFamilyName,
            .server_type = NAME_LAST_CORE,
            .heuristic_type = NAME_LAST_CORE,
            .expected_result = NAME_LAST_CORE},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = NAME_MIDDLE,
            .heuristic_type = NAME_LAST_PREFIX,
            .expected_result = NAME_LAST_PREFIX},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = NAME_LAST,
            .heuristic_type = NAME_LAST_CORE,
            .expected_result = NAME_LAST_CORE}));

// Tests that consecutive identical events are not added twice to the event log.
TEST(AutofillFieldLogEventTypeTest, AppendLogEventIfNotRepeated) {
  // The following three FieldLogEventTypes are arbitrary besides being of
  // distinct types.
  AutofillField::FieldLogEventType a = AskForValuesToFillFieldLogEvent{
      .has_suggestion = OptionalBoolean::kFalse,
      .suggestion_is_shown = OptionalBoolean::kFalse};
  AutofillField::FieldLogEventType a2 = AskForValuesToFillFieldLogEvent{
      .has_suggestion = OptionalBoolean::kTrue,
      .suggestion_is_shown = OptionalBoolean::kTrue};
  AutofillField::FieldLogEventType b =
      TriggerFillFieldLogEvent{.data_type = FillDataType::kUndefined,
                               .associated_country_code = "DE",
                               .timestamp = AutofillClock::Now()};
  AutofillField::FieldLogEventType c = FillFieldLogEvent{
      .fill_event_id = absl::get<TriggerFillFieldLogEvent>(b).fill_event_id,
      .had_value_before_filling = OptionalBoolean::kTrue,
      .autofill_skipped_status = FieldFillingSkipReason::kAlreadyAutofilled,
      .was_autofilled_before_security_policy = OptionalBoolean::kTrue,
      .had_value_after_filling = OptionalBoolean::kTrue};

  AutofillField f;
  EXPECT_TRUE(f.field_log_events().empty());

  f.AppendLogEventIfNotRepeated(a);
  EXPECT_EQ(f.field_log_events().size(), 1u);
  f.AppendLogEventIfNotRepeated(a);
  EXPECT_EQ(f.field_log_events().size(), 1u);

  f.AppendLogEventIfNotRepeated(a2);
  EXPECT_EQ(f.field_log_events().size(), 2u);
  f.AppendLogEventIfNotRepeated(a2);
  EXPECT_EQ(f.field_log_events().size(), 2u);

  f.AppendLogEventIfNotRepeated(b);
  EXPECT_EQ(f.field_log_events().size(), 3u);
  f.AppendLogEventIfNotRepeated(b);
  EXPECT_EQ(f.field_log_events().size(), 3u);

  f.AppendLogEventIfNotRepeated(c);
  EXPECT_EQ(f.field_log_events().size(), 4u);
  f.AppendLogEventIfNotRepeated(c);
  EXPECT_EQ(f.field_log_events().size(), 4u);

  f.AppendLogEventIfNotRepeated(a);
  EXPECT_EQ(f.field_log_events().size(), 5u);
  f.AppendLogEventIfNotRepeated(a);
  EXPECT_EQ(f.field_log_events().size(), 5u);
}

}  // namespace
}  // namespace autofill
