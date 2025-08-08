// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/placeholder_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {
namespace {

constexpr char16_t kPreFilledValue[] = u"pre-filled";

class PlaceholderMetricsTest : public AutofillMetricsBaseTest,
                               public testing::Test {
 public:
  void SetUp() override { SetUpHelper(); }
  void TearDown() override { TearDownHelper(); }

 protected:
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList placeholders_features_;
};

TEST_F(PlaceholderMetricsTest, EmitsUmaAutofillPreFilledFieldStatus) {
  placeholders_features_.InitAndEnableFeature(
      features::kAutofillOverwritePlaceholdersOnly);

  test::FormDescription form_description = {
      .fields = {{.role = NAME_FIRST,
                  .heuristic_type = NAME_FIRST,
                  .value = kPreFilledValue},
                 {.role = NAME_LAST, .heuristic_type = NAME_LAST}}};
  FormData form = test::GetFormData(form_description);

  // Simulate page load.
  autofill_manager().AddSeenForm(form,
                                 test::GetHeuristicTypes(form_description),
                                 test::GetServerTypes(form_description),
                                 /*preserve_values_in_form_structure=*/true);
  // Simluate interacting with the form.
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
  // Get cached form and modify fields.
  FormStructure* cached_form;
  AutofillField* cached_triggering_field;
  ASSERT_TRUE(autofill_manager().GetCachedFormAndField(
      form.global_id(), form.fields()[0].global_id(), &cached_form,
      &cached_triggering_field));
  cached_form->fields()[1]->set_may_use_prefilled_placeholder(false);
  FillTestProfile(form);
  SubmitForm(form);

  ResetDriverToCommitMetrics();
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("Autofill.PreFilledFieldStatus.Address"),
      BucketsAre(
          base::Bucket(AutofillPreFilledFieldStatus::kPreFilledOnPageLoad, 1),
          base::Bucket(AutofillPreFilledFieldStatus::kEmptyOnPageLoad, 1)));
}

TEST_F(PlaceholderMetricsTest,
       EmitsUmaAutofillPreFilledFieldStatusByFieldType) {
  placeholders_features_.InitAndEnableFeature(
      features::kAutofillOverwritePlaceholdersOnly);

  test::FormDescription form_description = {
      .fields = {{.role = NAME_FIRST,
                  .heuristic_type = NAME_FIRST,
                  .value = kPreFilledValue},
                 {.role = NAME_LAST, .heuristic_type = NAME_LAST}}};
  FormData form = test::GetFormData(form_description);

  // Simulate page load.
  autofill_manager().AddSeenForm(form,
                                 test::GetHeuristicTypes(form_description),
                                 test::GetServerTypes(form_description),
                                 /*preserve_values_in_form_structure=*/true);
  // Simluate interacting with the form.
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
  // Get cached form and modify fields.
  FormStructure* cached_form;
  AutofillField* cached_triggering_field;
  ASSERT_TRUE(autofill_manager().GetCachedFormAndField(
      form.global_id(), form.fields()[0].global_id(), &cached_form,
      &cached_triggering_field));
  cached_form->fields()[1]->set_may_use_prefilled_placeholder(false);
  FillTestProfile(form);
  SubmitForm(form);

  ResetDriverToCommitMetrics();
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "Autofill.PreFilledFieldStatus.ByFieldType"),
      BucketsAre(base::Bucket(/*NAME_FIRST: Pre-filled on page load*/ 48, 1),
                 base::Bucket(/*NAME_LAST: Empty on page load*/ 81, 1)));
}

TEST_F(PlaceholderMetricsTest, EmitsUmaAutofillPreFilledFieldClassifications) {
  placeholders_features_.InitAndEnableFeature(
      features::kAutofillOverwritePlaceholdersOnly);

  test::FormDescription form_description = {
      .fields = {{.role = NAME_FIRST,
                  .heuristic_type = NAME_FIRST,
                  .value = u"pre-filled"},
                 {.role = NAME_LAST,
                  .heuristic_type = NAME_LAST,
                  .value = u"pre-filled"}}};
  FormData form = test::GetFormData(form_description);

  // Simulate page load.
  autofill_manager().AddSeenForm(form,
                                 test::GetHeuristicTypes(form_description),
                                 test::GetServerTypes(form_description),
                                 /*preserve_values_in_form_structure=*/true);
  // Simluate interacting with the form.
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
  // Get cached form and modify fields.
  FormStructure* cached_form;
  AutofillField* cached_triggering_field;
  ASSERT_TRUE(autofill_manager().GetCachedFormAndField(
      form.global_id(), form.fields()[0].global_id(), &cached_form,
      &cached_triggering_field));
  cached_form->fields()[1]->set_may_use_prefilled_placeholder(false);
  // Fill form.
  FillTestProfile(form);
  // Submit form.
  SubmitForm(form);

  ResetDriverToCommitMetrics();
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "Autofill.PreFilledFieldClassifications.Address"),
      BucketsAre(
          base::Bucket(AutofillPreFilledFieldClassifications::kClassified, 1),
          base::Bucket(AutofillPreFilledFieldClassifications::kNotClassified,
                       1)));
}

TEST_F(PlaceholderMetricsTest,
       EmitsUmaAutofillPreFilledFieldClassificationsQuality) {
  placeholders_features_.InitWithFeatures(
      {features::kAutofillOverwritePlaceholdersOnly,
       features::kAutofillSkipPreFilledFields},
      {});
  test::FormDescription form_description = {
      .fields = {
          {.role = ADDRESS_HOME_CITY, .heuristic_type = ADDRESS_HOME_CITY},
          {.role = NAME_FIRST,
           .heuristic_type = NAME_FIRST,
           .value = u"kPlaceholderValueNotChanged"},
          {.role = NAME_LAST,
           .heuristic_type = NAME_LAST,
           .value = u"kPlaceholderValueChanged"},
          {.role = ADDRESS_HOME_LINE1,
           .heuristic_type = ADDRESS_HOME_LINE1,
           .value = u"kMeaningfullyPreFilledValueChanged"},
          {.role = ADDRESS_HOME_COUNTRY,
           .heuristic_type = ADDRESS_HOME_COUNTRY,
           .value = u"kMeaningfullyPreFilledValueNotChanged"}}};
  FormData form = test::GetFormData(form_description);

  // Simulate page load.
  autofill_manager().AddSeenForm(form,
                                 test::GetHeuristicTypes(form_description),
                                 test::GetServerTypes(form_description),
                                 /*preserve_values_in_form_structure=*/true);
  // Simluate interacting with the form.
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
  // Get cached form and modify fields.
  FormStructure* cached_form;
  AutofillField* cached_triggering_field;
  ASSERT_TRUE(autofill_manager().GetCachedFormAndField(
      form.global_id(), form.fields()[0].global_id(), &cached_form,
      &cached_triggering_field));
  cached_form->field(1)->set_may_use_prefilled_placeholder(true);
  cached_form->field(2)->set_may_use_prefilled_placeholder(true);
  cached_form->field(3)->set_may_use_prefilled_placeholder(false);
  cached_form->field(4)->set_may_use_prefilled_placeholder(false);
  test_api(form).field(2).set_value(u"changed");
  test_api(form).field(3).set_value(u"changed");
  SubmitForm(form);

  ResetDriverToCommitMetrics();
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "Autofill.PreFilledFieldClassificationsQuality.Address"),
      BucketsAre(base::Bucket(AutofillPreFilledFieldClassificationsQuality::
                                  kPlaceholderValueNotChanged,
                              1),
                 base::Bucket(AutofillPreFilledFieldClassificationsQuality::
                                  kPlaceholderValueChanged,
                              1),
                 base::Bucket(AutofillPreFilledFieldClassificationsQuality::
                                  kMeaningfullyPreFilledValueNotChanged,
                              1),
                 base::Bucket(AutofillPreFilledFieldClassificationsQuality::
                                  kMeaningfullyPreFilledValueChanged,
                              1)));
}

}  // namespace
}  // namespace autofill::autofill_metrics
