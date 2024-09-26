// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/profile_import_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::Bucket;
using ::base::BucketsAre;

namespace autofill::autofill_metrics {

namespace {

using AddressImportRequirements = AddressProfileImportRequirementMetric;

struct AddressProfileImportRequirementExpectations {
  AddressImportRequirements requirement;
  bool fulfilled;
};

// For a single submission, test if the right bucket was filled.
void TestAddressProfileImportRequirements(
    base::HistogramTester* histogram_tester,
    const std::vector<AddressProfileImportRequirementExpectations>&
        expectations) {
  std::string histogram = "Autofill.AddressProfileImportRequirements";

  for (auto& expectation : expectations) {
    histogram_tester->ExpectBucketCount(histogram, expectation.requirement,
                                        expectation.fulfilled ? 1 : 0);
  }
}

// For country specific address field requirements.
void TestAddressProfileImportCountrySpecificFieldRequirements(
    base::HistogramTester* histogram_tester,
    AddressProfileImportCountrySpecificFieldRequirementsMetric metric) {
  std::string histogram =
      "Autofill.AddressProfileImportCountrySpecificFieldRequirements";

  // Test that the right bucket was populated.
  histogram_tester->ExpectBucketCount(histogram, metric, 1);
}

}  // namespace

class AutofillProfileImportMetricsTest : public AutofillMetricsBaseTest,
                                         public testing::Test {
 public:
  void SetUp() override { SetUpHelper(); }
  void TearDown() override { TearDownHelper(); }
};

// Test that the ProfileImportStatus logs a no import.
TEST_F(AutofillProfileImportMetricsTest, ProfileImportStatus_NoImport) {
  // Set up our form data. Since a ZIP code is required for US profiles, this
  // import fails.
  FormData form = GetAndAddSeenForm(
      {.description_for_logging = "ProfileImportStatus_NoImport",
       .fields = {
           {.role = NAME_FULL, .value = u"Elvis Aaron Presley"},
           {.role = ADDRESS_HOME_LINE1, .value = u"3734 Elvis Presley Blvd."},
           {.role = ADDRESS_HOME_CITY, .value = u"New York"},
           {.role = PHONE_HOME_CITY_AND_NUMBER, .value = u"2345678901"},
           {.role = ADDRESS_HOME_STATE, .value = u"Invalid State"},
           {.role = ADDRESS_HOME_COUNTRY, .value = u"USA"}}});

  FillTestProfile(form);
  base::HistogramTester histogram_tester;
  SubmitForm(form);
  using Metric = AddressProfileImportStatusMetric;
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.AddressProfileImportStatus"),
      BucketsAre(Bucket(Metric::kRegularImport, 0),
                 Bucket(Metric::kNoImport, 1),
                 Bucket(Metric::kSectionUnionImport, 0)));
}

// Test that the ProfileImportStatus logs a regular import.
TEST_F(AutofillProfileImportMetricsTest, ProfileImportStatus_RegularImport) {
  // Set up our form data.
  FormData form = GetAndAddSeenForm(
      {.description_for_logging = "ProfileImportStatus_RegularImport",
       .fields = {
           {.role = NAME_FULL, .value = u"Elvis Aaron Presley"},
           {.role = ADDRESS_HOME_LINE1, .value = u"3734 Elvis Presley Blvd."},
           {.role = ADDRESS_HOME_CITY, .value = u"New York"},
           {.role = PHONE_HOME_CITY_AND_NUMBER, .value = u"2345678901"},
           {.role = ADDRESS_HOME_STATE, .value = u"CA"},
           {.role = ADDRESS_HOME_ZIP, .value = u"37373"},
           {.role = ADDRESS_HOME_COUNTRY, .value = u"USA"}}});

  FillTestProfile(form);
  base::HistogramTester histogram_tester;
  SubmitForm(form);
  using Metric = AddressProfileImportStatusMetric;
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.AddressProfileImportStatus"),
      BucketsAre(Bucket(Metric::kRegularImport, 1),
                 Bucket(Metric::kNoImport, 0),
                 Bucket(Metric::kSectionUnionImport, 0)));
}

// Test that the ProfileImportRequirements are all counted as fulfilled for a
// 'perfect' profile import.
TEST_F(AutofillProfileImportMetricsTest,
       ProfileImportRequirements_AllFulfilled) {
  // Set up our form data.
  FormData form = GetAndAddSeenForm(
      {.description_for_logging = "ProfileImportRequirements_AllFulfilled",
       .fields = {
           {.role = NAME_FULL, .value = u"Elvis Aaron Presley"},
           {.role = ADDRESS_HOME_LINE1, .value = u"3734 Elvis Presley Blvd."},
           {.role = ADDRESS_HOME_CITY, .value = u"New York"},
           {.role = PHONE_HOME_CITY_AND_NUMBER, .value = u"2345678901"},
           {.role = ADDRESS_HOME_STATE, .value = u"CA"},
           {.role = ADDRESS_HOME_ZIP, .value = u"37373"},
           {.role = ADDRESS_HOME_COUNTRY, .value = u"USA"}}});

  FillTestProfile(form);

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  std::vector<AddressProfileImportRequirementExpectations> expectations = {
      {AddressImportRequirements::kStateValidRequirementFulfilled, true},
      {AddressImportRequirements::kStateValidRequirementViolated, false},
      {AddressImportRequirements::kEmailValidRequirementFulfilled, true},
      {AddressImportRequirements::kEmailValidRequirementViolated, false},
      {AddressImportRequirements::kZipValidRequirementFulfilled, true},
      {AddressImportRequirements::kZipValidRequirementViolated, false},
      {AddressImportRequirements::kEmailAddressUniqueRequirementFulfilled,
       true},
      {AddressImportRequirements::kEmailAddressUniqueRequirementViolated,
       false},
      {AddressImportRequirements::kNoInvalidFieldTypesRequirementFulfilled,
       true},
      {AddressImportRequirements::kNoInvalidFieldTypesRequirementViolated,
       false},
      {AddressImportRequirements::kCityRequirementFulfilled, true},
      {AddressImportRequirements::kCityRequirementViolated, false},
      {AddressImportRequirements::kZipRequirementFulfilled, true},
      {AddressImportRequirements::kZipRequirementViolated, false},
      {AddressImportRequirements::kStateRequirementFulfilled, true},
      {AddressImportRequirements::kStateRequirementViolated, false},
      {AddressImportRequirements::kOverallRequirementFulfilled, true},
      {AddressImportRequirements::kOverallRequirementViolated, false},
      {AddressImportRequirements::kLine1RequirementFulfilled, true},
      {AddressImportRequirements::kLine1RequirementViolated, false},
      {AddressImportRequirements::kZipOrStateRequirementFulfilled, true},
      {AddressImportRequirements::kZipOrStateRequirementViolated, false},
      {AddressImportRequirements::kNameRequirementFulfilled, false},
      {AddressImportRequirements::kNameRequirementViolated, false}};

  TestAddressProfileImportRequirements(&histogram_tester, expectations);

  // All country specific field requirements have been fulfilled.
  TestAddressProfileImportCountrySpecificFieldRequirements(
      &histogram_tester,
      AddressProfileImportCountrySpecificFieldRequirementsMetric::ALL_GOOD);
}

// Test that the ProfileImportRequirements are counted correctly if only the
// ADDRESS_HOME_LINE1 is missing.
TEST_F(AutofillProfileImportMetricsTest,
       ProfileImportRequirements_MissingHomeLineOne) {
  // Set up our form data.
  FormData form = test::GetFormData(
      {.description_for_logging =
           "ProfileImportRequirements_MissingHomeLineOne",
       .fields = {{.role = NAME_FULL, .value = u"Elvis Aaron Presley"},
                  {.role = ADDRESS_HOME_LINE1, .value = u""},
                  {.role = ADDRESS_HOME_CITY, .value = u"New York"},
                  {.role = PHONE_HOME_NUMBER, .value = u"2345678901"},
                  {.role = ADDRESS_HOME_STATE, .value = u"CA"},
                  {.role = ADDRESS_HOME_ZIP, .value = u"37373"},
                  {.role = ADDRESS_HOME_COUNTRY, .value = u"USA"}}});

  std::vector<ServerFieldType> field_types = {
      NAME_FULL,           ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY,   PHONE_HOME_CITY_AND_NUMBER,
      ADDRESS_HOME_STATE,  ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY};

  autofill_manager().AddSeenForm(form, field_types);
  FillTestProfile(form);

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  std::vector<AddressProfileImportRequirementExpectations> expectations = {
      {AddressImportRequirements::kStateValidRequirementFulfilled, true},
      {AddressImportRequirements::kStateValidRequirementViolated, false},
      {AddressImportRequirements::kEmailValidRequirementFulfilled, true},
      {AddressImportRequirements::kEmailValidRequirementViolated, false},
      {AddressImportRequirements::kZipValidRequirementFulfilled, true},
      {AddressImportRequirements::kZipValidRequirementViolated, false},
      {AddressImportRequirements::kEmailAddressUniqueRequirementFulfilled,
       true},
      {AddressImportRequirements::kEmailAddressUniqueRequirementViolated,
       false},
      {AddressImportRequirements::kNoInvalidFieldTypesRequirementFulfilled,
       true},
      {AddressImportRequirements::kNoInvalidFieldTypesRequirementViolated,
       false},
      {AddressImportRequirements::kCityRequirementFulfilled, true},
      {AddressImportRequirements::kCityRequirementViolated, false},
      {AddressImportRequirements::kZipRequirementFulfilled, true},
      {AddressImportRequirements::kZipRequirementViolated, false},
      {AddressImportRequirements::kStateRequirementFulfilled, true},
      {AddressImportRequirements::kStateRequirementViolated, false},
      {AddressImportRequirements::kOverallRequirementFulfilled, false},
      {AddressImportRequirements::kOverallRequirementViolated, true},
      {AddressImportRequirements::kLine1RequirementFulfilled, false},
      {AddressImportRequirements::kLine1RequirementViolated, true},
      {AddressImportRequirements::kZipOrStateRequirementFulfilled, true},
      {AddressImportRequirements::kZipOrStateRequirementViolated, false},
      {AddressImportRequirements::kNameRequirementFulfilled, false},
      {AddressImportRequirements::kNameRequirementViolated, false}};

  TestAddressProfileImportRequirements(&histogram_tester, expectations);

  // The country specific ADDRESS_HOME_LINE1 field requirement was violated.
  TestAddressProfileImportCountrySpecificFieldRequirements(
      &histogram_tester,
      AddressProfileImportCountrySpecificFieldRequirementsMetric::
          LINE1_REQUIREMENT_VIOLATED);
}

// Test that the ProfileImportRequirements are all counted as fulfilled for a
// 'perfect' profile import.
TEST_F(AutofillProfileImportMetricsTest,
       ProfileImportRequirements_AllFulfilledForNonStateCountry) {
  // Set up our form data.
  FormData form = test::GetFormData(
      {.description_for_logging =
           "ProfileImportRequirements_AllFulfilledForNonStateCountry",
       .fields = {
           {.role = NAME_FULL, .value = u"Elvis Aaron Presley"},
           {.role = ADDRESS_HOME_LINE1, .value = u"3734 Elvis Presley Blvd."},
           {.role = ADDRESS_HOME_CITY, .value = u"New York"},
           {.role = PHONE_HOME_NUMBER, .value = u"2345678901"},
           {.role = ADDRESS_HOME_STATE, .value = u""},
           {.role = ADDRESS_HOME_ZIP, .value = u"37373"},
           {.role = ADDRESS_HOME_COUNTRY, .value = u"Germany"}}});

  std::vector<ServerFieldType> field_types = {
      NAME_FULL,           ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY,   PHONE_HOME_CITY_AND_NUMBER,
      ADDRESS_HOME_STATE,  ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY};

  autofill_manager().AddSeenForm(form, field_types);
  FillTestProfile(form);

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  std::vector<AddressProfileImportRequirementExpectations> expectations = {
      {AddressImportRequirements::kStateValidRequirementFulfilled, true},
      {AddressImportRequirements::kStateValidRequirementViolated, false},
      {AddressImportRequirements::kEmailValidRequirementFulfilled, true},
      {AddressImportRequirements::kEmailValidRequirementViolated, false},
      {AddressImportRequirements::kZipValidRequirementFulfilled, true},
      {AddressImportRequirements::kZipValidRequirementViolated, false},
      {AddressImportRequirements::kEmailAddressUniqueRequirementFulfilled,
       true},
      {AddressImportRequirements::kEmailAddressUniqueRequirementViolated,
       false},
      {AddressImportRequirements::kNoInvalidFieldTypesRequirementFulfilled,
       true},
      {AddressImportRequirements::kNoInvalidFieldTypesRequirementViolated,
       false},
      {AddressImportRequirements::kCityRequirementFulfilled, true},
      {AddressImportRequirements::kCityRequirementViolated, false},
      {AddressImportRequirements::kZipRequirementFulfilled, true},
      {AddressImportRequirements::kZipRequirementViolated, false},
      {AddressImportRequirements::kStateRequirementFulfilled, true},
      {AddressImportRequirements::kStateRequirementViolated, false},
      {AddressImportRequirements::kOverallRequirementFulfilled, true},
      {AddressImportRequirements::kOverallRequirementViolated, false},
      {AddressImportRequirements::kLine1RequirementFulfilled, true},
      {AddressImportRequirements::kLine1RequirementViolated, false},
      {AddressImportRequirements::kZipOrStateRequirementFulfilled, true},
      {AddressImportRequirements::kZipOrStateRequirementViolated, false},
      {AddressImportRequirements::kNameRequirementFulfilled, false},
      {AddressImportRequirements::kNameRequirementViolated, false}};

  TestAddressProfileImportRequirements(&histogram_tester, expectations);
  // All country specific field requirements have been fulfilled.
  TestAddressProfileImportCountrySpecificFieldRequirements(
      &histogram_tester,
      AddressProfileImportCountrySpecificFieldRequirementsMetric::ALL_GOOD);
}

// Test that the ProfileImportRequirements are all counted as fulfilled for a
// completely filled profile but with invalid values.
TEST_F(AutofillProfileImportMetricsTest,
       ProfileImportRequirements_FilledButInvalidZipEmailAndState) {
  // Set up our form data.
  test::FormDescription form_description = {
      .description_for_logging =
          "ProfileImportRequirements_FilledButInvalidZipEmailAndState",
      .fields = {
          {.role = NAME_FULL, .value = u"Elvis Aaron Presley"},
          {.role = ADDRESS_HOME_LINE1, .value = u"3734 Elvis Presley Blvd."},
          {.role = ADDRESS_HOME_CITY, .value = u"New York"},
          {.role = PHONE_HOME_NUMBER, .value = u"2345678901"},
          {.role = ADDRESS_HOME_STATE, .value = u"DefNotAState"},
          {.role = ADDRESS_HOME_ZIP, .value = u"1234567890"},
          {.role = ADDRESS_HOME_COUNTRY, .value = u"USA"},
          {.role = EMAIL_ADDRESS, .value = u"test_noat_test.io"}}};

  FormData form = GetAndAddSeenForm(form_description);
  FillTestProfile(form);

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  std::vector<AddressProfileImportRequirementExpectations> expectations = {
      {AddressImportRequirements::kStateValidRequirementFulfilled, false},
      {AddressImportRequirements::kStateValidRequirementViolated, true},
      {AddressImportRequirements::kEmailValidRequirementFulfilled, false},
      {AddressImportRequirements::kEmailValidRequirementViolated, true},
      {AddressImportRequirements::kZipValidRequirementFulfilled, false},
      {AddressImportRequirements::kZipValidRequirementViolated, true},
      {AddressImportRequirements::kEmailAddressUniqueRequirementFulfilled,
       true},
      {AddressImportRequirements::kEmailAddressUniqueRequirementViolated,
       false},
      {AddressImportRequirements::kNoInvalidFieldTypesRequirementFulfilled,
       true},
      {AddressImportRequirements::kNoInvalidFieldTypesRequirementViolated,
       false},
      {AddressImportRequirements::kCityRequirementFulfilled, true},
      {AddressImportRequirements::kCityRequirementViolated, false},
      {AddressImportRequirements::kZipRequirementFulfilled, true},
      {AddressImportRequirements::kZipRequirementViolated, false},
      {AddressImportRequirements::kStateRequirementFulfilled, true},
      {AddressImportRequirements::kStateRequirementViolated, false},
      {AddressImportRequirements::kOverallRequirementFulfilled, false},
      {AddressImportRequirements::kOverallRequirementViolated, true},
      {AddressImportRequirements::kLine1RequirementFulfilled, true},
      {AddressImportRequirements::kLine1RequirementViolated, false},
      {AddressImportRequirements::kZipOrStateRequirementFulfilled, true},
      {AddressImportRequirements::kZipOrStateRequirementViolated, false},
      {AddressImportRequirements::kNameRequirementFulfilled, false},
      {AddressImportRequirements::kNameRequirementViolated, false}};

  TestAddressProfileImportRequirements(&histogram_tester, expectations);

  // All country specific field requirements have been fulfilled.
  TestAddressProfileImportCountrySpecificFieldRequirements(
      &histogram_tester,
      AddressProfileImportCountrySpecificFieldRequirementsMetric::ALL_GOOD);
}

// Test that the ProfileImportRequirements are all counted as fulfilled for a
// profile with multiple email addresses.
TEST_F(AutofillProfileImportMetricsTest,
       ProfileImportRequirements_NonUniqueEmail) {
  // Set up our form data.
  FormData form = test::GetFormData(
      {.description_for_logging = "ProfileImportRequirements_NonUniqueEmail",
       .fields = {
           {.role = NAME_FULL, .value = u"Elvis Aaron Presley"},
           {.role = ADDRESS_HOME_LINE1, .value = u"3734 Elvis Presley Blvd."},
           {.role = ADDRESS_HOME_CITY, .value = u"New York"},
           {.role = PHONE_HOME_CITY_AND_NUMBER, .value = u"2345678901"},
           {.role = ADDRESS_HOME_STATE, .value = u"CA"},
           {.role = ADDRESS_HOME_ZIP, .value = u"37373"},
           {.role = ADDRESS_HOME_COUNTRY, .value = u"USA"},
           {.role = EMAIL_ADDRESS, .value = u"test_noat_test.io"},
           {.role = EMAIL_ADDRESS, .value = u"not_test@test.io"}}});

  std::vector<ServerFieldType> field_types = {NAME_FULL,
                                              ADDRESS_HOME_LINE1,
                                              ADDRESS_HOME_CITY,
                                              PHONE_HOME_CITY_AND_NUMBER,
                                              ADDRESS_HOME_STATE,
                                              ADDRESS_HOME_ZIP,
                                              ADDRESS_HOME_COUNTRY,
                                              EMAIL_ADDRESS,
                                              EMAIL_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);
  FillTestProfile(form);

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  std::vector<AddressProfileImportRequirementExpectations> expectations = {
      {AddressImportRequirements::kStateValidRequirementFulfilled, true},
      {AddressImportRequirements::kStateValidRequirementViolated, false},
      {AddressImportRequirements::kEmailValidRequirementFulfilled, true},
      {AddressImportRequirements::kEmailValidRequirementViolated, false},
      {AddressImportRequirements::kZipValidRequirementFulfilled, true},
      {AddressImportRequirements::kZipValidRequirementViolated, false},
      {AddressImportRequirements::kEmailAddressUniqueRequirementFulfilled,
       false},
      {AddressImportRequirements::kEmailAddressUniqueRequirementViolated, true},
      {AddressImportRequirements::kNoInvalidFieldTypesRequirementFulfilled,
       true},
      {AddressImportRequirements::kNoInvalidFieldTypesRequirementViolated,
       false},
      {AddressImportRequirements::kCityRequirementFulfilled, true},
      {AddressImportRequirements::kCityRequirementViolated, false},
      {AddressImportRequirements::kZipRequirementFulfilled, true},
      {AddressImportRequirements::kZipRequirementViolated, false},
      {AddressImportRequirements::kStateRequirementFulfilled, true},
      {AddressImportRequirements::kStateRequirementViolated, false},
      {AddressImportRequirements::kOverallRequirementFulfilled, false},
      {AddressImportRequirements::kOverallRequirementViolated, true},
      {AddressImportRequirements::kLine1RequirementFulfilled, true},
      {AddressImportRequirements::kLine1RequirementViolated, false},
      {AddressImportRequirements::kZipOrStateRequirementFulfilled, true},
      {AddressImportRequirements::kZipOrStateRequirementViolated, false},
      {AddressImportRequirements::kNameRequirementFulfilled, false},
      {AddressImportRequirements::kNameRequirementViolated, false}};

  TestAddressProfileImportRequirements(&histogram_tester, expectations);

  // All country specific field requirements have been fulfilled.
  TestAddressProfileImportCountrySpecificFieldRequirements(
      &histogram_tester,
      AddressProfileImportCountrySpecificFieldRequirementsMetric::ALL_GOOD);
}

// Test the correct ProfileImportRequirements logging if multiple fields are
// missing.
TEST_F(AutofillProfileImportMetricsTest,
       ProfileImportRequirements_OnlyAddressLineOne) {
  // Set up our form data.
  FormData form = test::GetFormData(
      {.description_for_logging =
           "ProfileImportRequirements_OnlyAddressLineOne",
       .fields = {
           {.role = NAME_FULL, .value = u"Elvis Aaron Presley"},
           {.role = ADDRESS_HOME_LINE1, .value = u"3734 Elvis Presley Blvd."},
           {.role = ADDRESS_HOME_CITY, .value = u""},
           {.role = PHONE_HOME_NUMBER, .value = u""},
           {.role = ADDRESS_HOME_STATE, .value = u""},
           {.role = ADDRESS_HOME_ZIP, .value = u""},
           {.role = ADDRESS_HOME_COUNTRY, .value = u""}}});

  std::vector<ServerFieldType> field_types = {
      NAME_FULL,           ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY,   PHONE_HOME_CITY_AND_NUMBER,
      ADDRESS_HOME_STATE,  ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY};

  autofill_manager().AddSeenForm(form, field_types);
  FillTestProfile(form);

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  std::vector<AddressProfileImportRequirementExpectations> expectations = {
      {AddressImportRequirements::kStateValidRequirementFulfilled, true},
      {AddressImportRequirements::kStateValidRequirementViolated, false},
      {AddressImportRequirements::kEmailValidRequirementFulfilled, true},
      {AddressImportRequirements::kEmailValidRequirementViolated, false},
      {AddressImportRequirements::kZipValidRequirementFulfilled, true},
      {AddressImportRequirements::kZipValidRequirementViolated, false},
      {AddressImportRequirements::kEmailAddressUniqueRequirementFulfilled,
       true},
      {AddressImportRequirements::kEmailAddressUniqueRequirementViolated,
       false},
      {AddressImportRequirements::kNoInvalidFieldTypesRequirementFulfilled,
       true},
      {AddressImportRequirements::kNoInvalidFieldTypesRequirementViolated,
       false},
      {AddressImportRequirements::kCityRequirementFulfilled, false},
      {AddressImportRequirements::kCityRequirementViolated, true},
      {AddressImportRequirements::kZipRequirementFulfilled, false},
      {AddressImportRequirements::kZipRequirementViolated, true},
      {AddressImportRequirements::kStateRequirementFulfilled, false},
      {AddressImportRequirements::kStateRequirementViolated, true},
      {AddressImportRequirements::kOverallRequirementFulfilled, false},
      {AddressImportRequirements::kOverallRequirementViolated, true},
      {AddressImportRequirements::kLine1RequirementFulfilled, true},
      {AddressImportRequirements::kLine1RequirementViolated, false},
      {AddressImportRequirements::kZipOrStateRequirementFulfilled, true},
      {AddressImportRequirements::kZipOrStateRequirementViolated, false},
      {AddressImportRequirements::kNameRequirementFulfilled, false},
      {AddressImportRequirements::kNameRequirementViolated, false}};

  TestAddressProfileImportRequirements(&histogram_tester, expectations);

  // All country specific field requirements have been fulfilled.
  TestAddressProfileImportCountrySpecificFieldRequirements(
      &histogram_tester,
      AddressProfileImportCountrySpecificFieldRequirementsMetric::
          ZIP_STATE_CITY_REQUIREMENT_VIOLATED);
}

// Test that the ProfileImportRequirements are all counted as fulfilled, except
// for the name requirement which was violated.
TEST_F(AutofillProfileImportMetricsTest,
       ProfileImportRequirements_AllFulfilledButName) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillRequireNameForProfileImport);
  //  Set up our form data.
  FormData form = GetAndAddSeenForm(
      {.description_for_logging = "ProfileImportRequirements_AllButName",
       .fields = {
           {.role = NAME_FULL, .value = u""},
           {.role = ADDRESS_HOME_LINE1, .value = u"3734 Elvis Presley Blvd."},
           {.role = ADDRESS_HOME_CITY, .value = u"New York"},
           {.role = PHONE_HOME_CITY_AND_NUMBER, .value = u"2345678901"},
           {.role = ADDRESS_HOME_STATE, .value = u"CA"},
           {.role = ADDRESS_HOME_ZIP, .value = u"37373"},
           {.role = ADDRESS_HOME_COUNTRY, .value = u"USA"}}});
  FillTestProfile(form);
  base::HistogramTester histogram_tester;
  SubmitForm(form);
  std::vector<AddressProfileImportRequirementExpectations> expectations = {
      {AddressImportRequirements::kStateValidRequirementFulfilled, true},
      {AddressImportRequirements::kStateValidRequirementViolated, false},
      {AddressImportRequirements::kEmailValidRequirementFulfilled, true},
      {AddressImportRequirements::kEmailValidRequirementViolated, false},
      {AddressImportRequirements::kZipValidRequirementFulfilled, true},
      {AddressImportRequirements::kZipValidRequirementViolated, false},
      {AddressImportRequirements::kEmailAddressUniqueRequirementFulfilled,
       true},
      {AddressImportRequirements::kEmailAddressUniqueRequirementViolated,
       false},
      {AddressImportRequirements::kNoInvalidFieldTypesRequirementFulfilled,
       true},
      {AddressImportRequirements::kNoInvalidFieldTypesRequirementViolated,
       false},
      {AddressImportRequirements::kCityRequirementFulfilled, true},
      {AddressImportRequirements::kCityRequirementViolated, false},
      {AddressImportRequirements::kZipRequirementFulfilled, true},
      {AddressImportRequirements::kZipRequirementViolated, false},
      {AddressImportRequirements::kStateRequirementFulfilled, true},
      {AddressImportRequirements::kStateRequirementViolated, false},
      {AddressImportRequirements::kOverallRequirementFulfilled, false},
      {AddressImportRequirements::kOverallRequirementViolated, true},
      {AddressImportRequirements::kLine1RequirementFulfilled, true},
      {AddressImportRequirements::kLine1RequirementViolated, false},
      {AddressImportRequirements::kZipOrStateRequirementFulfilled, true},
      {AddressImportRequirements::kZipOrStateRequirementViolated, false},
      {AddressImportRequirements::kNameRequirementFulfilled, false},
      {AddressImportRequirements::kNameRequirementViolated, true}};
  TestAddressProfileImportRequirements(&histogram_tester, expectations);
}

}  // namespace autofill::autofill_metrics
