// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/mobile_label_formatter.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/ui/label_formatter_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace autofill {
namespace {

ServerFieldTypeSet GetContactOnlyFieldTypes() {
  return {NO_SERVER_DATA, NAME_FIRST, NAME_LAST, EMAIL_ADDRESS,
          PHONE_HOME_WHOLE_NUMBER};
}

ServerFieldTypeSet GetAddressOnlyFieldTypes() {
  return {NO_SERVER_DATA,     NAME_FIRST,
          NAME_LAST,          ADDRESS_HOME_LINE1,
          ADDRESS_HOME_LINE2, ADDRESS_HOME_DEPENDENT_LOCALITY,
          ADDRESS_HOME_CITY,  ADDRESS_HOME_STATE,
          ADDRESS_HOME_ZIP,   ADDRESS_HOME_COUNTRY};
}

ServerFieldTypeSet GetAddressPlusEmailFieldTypes() {
  return {NO_SERVER_DATA,
          NAME_FIRST,
          NAME_LAST,
          EMAIL_ADDRESS,
          ADDRESS_HOME_LINE1,
          ADDRESS_HOME_LINE2,
          ADDRESS_HOME_DEPENDENT_LOCALITY,
          ADDRESS_HOME_CITY,
          ADDRESS_HOME_STATE,
          ADDRESS_HOME_ZIP,
          ADDRESS_HOME_COUNTRY};
}

ServerFieldTypeSet GetAddressPlusContactFieldTypes() {
  return {NO_SERVER_DATA,
          NAME_FIRST,
          NAME_LAST,
          EMAIL_ADDRESS,
          ADDRESS_HOME_LINE1,
          ADDRESS_HOME_LINE2,
          ADDRESS_HOME_DEPENDENT_LOCALITY,
          ADDRESS_HOME_CITY,
          ADDRESS_HOME_STATE,
          ADDRESS_HOME_ZIP,
          ADDRESS_HOME_COUNTRY,
          PHONE_HOME_WHOLE_NUMBER};
}

AutofillProfile GetProfileA() {
  AutofillProfile profile;
  test::SetProfileInfo(&profile, "firstA", "middleA", "lastA",
                       "emailA@gmail.com", "", "address1A", "address2A",
                       "cityA", "MA", "02113", "US", "16176660000");
  return profile;
}

AutofillProfile GetProfileB() {
  AutofillProfile profile;
  test::SetProfileInfo(&profile, "firstB", "middleB", "lastB",
                       "emailB@gmail.com", "", "address1B", "address2B",
                       "cityB", "NY", "12224", "US", "15185550000");
  return profile;
}

TEST(MobileLabelFormatterTest, GetLabelsWithMissingProfiles) {
  const std::vector<const AutofillProfile*> profiles{};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", NAME_FIRST, {NAME_FIRST, NAME_LAST, EMAIL_ADDRESS});
  EXPECT_TRUE(formatter->GetLabels().empty());
}

TEST(MobileLabelFormatterTest, GetLabelsForUnfocusedAddress_ShowOne) {
  std::map<std::string, std::string> parameters;
  parameters[features::kAutofillUseMobileLabelDisambiguationParameterName] =
      features::kAutofillUseMobileLabelDisambiguationParameterShowOne;

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kAutofillUseMobileLabelDisambiguation, parameters);

  AutofillProfile profileA = GetProfileA();
  AutofillProfile profileB = GetProfileB();
  AutofillProfile profileC;
  test::SetProfileInfo(&profileC, "firstC", "middleC", "lastC", "", "", "", "",
                       "", "", "", "US", "");
  const std::vector<const AutofillProfile*> profiles{&profileA, &profileB,
                                                     &profileC};

  // Tests that the street address is shown when the form contains a street
  // address field and the user is not focused on it.
  std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", NAME_FIRST, GetAddressOnlyFieldTypes());
  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(u"address1A, address2A", u"address1B, address2B",
                          std::u16string()));

  // Tests that the non street address is shown when the form's only address
  // fields are non street address fields and the user is not focused on any of
  // them.
  formatter = LabelFormatter::Create(profiles, "en-US", NAME_FIRST,
                                     {NAME_FIRST, NAME_LAST, ADDRESS_HOME_ZIP});
  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(u"02113", u"12224", std::u16string()));

  // Like the previous test, but without name.
  formatter = LabelFormatter::Create(
      profiles, "en-US", EMAIL_ADDRESS,
      {ADDRESS_HOME_CITY, ADDRESS_HOME_STATE, EMAIL_ADDRESS});
  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(u"cityA, MA", u"cityB, NY", std::u16string()));

  // Tests that addresses are not shown when the form does not contain an
  // address field.
  formatter = LabelFormatter::Create(profiles, "en-US", NAME_FIRST,
                                     GetContactOnlyFieldTypes());
  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(u"(617) 666-0000", u"(518) 555-0000", std::u16string()));
}

TEST(MobileLabelFormatterTest,
     GetLabelsForFocusedAddress_MultipleProfiles_ShowOne) {
  std::map<std::string, std::string> parameters;
  parameters[features::kAutofillUseMobileLabelDisambiguationParameterName] =
      features::kAutofillUseMobileLabelDisambiguationParameterShowOne;

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kAutofillUseMobileLabelDisambiguation, parameters);

  AutofillProfile profileA = GetProfileA();

  // Tests that a street is shown when a form contains an unfocused street
  // address and a focused non street address.
  AutofillProfile profileB = GetProfileB();
  std::vector<const AutofillProfile*> profiles{&profileA, &profileB};

  std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", ADDRESS_HOME_ZIP, GetAddressPlusContactFieldTypes());
  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(u"address1A, address2A", u"address1B, address2B"));

  // Tests that a non street address is shown when a form contains only
  // non focused street address fields and a focused non street address.
  formatter = LabelFormatter::Create(
      profiles, "en-US", ADDRESS_HOME_ZIP,
      {ADDRESS_HOME_CITY, ADDRESS_HOME_STATE, ADDRESS_HOME_ZIP});
  EXPECT_THAT(formatter->GetLabels(), ElementsAre(u"cityA, MA", u"cityB, NY"));

  // Tests that a non street address is not shown when a form contains
  // non focused street address fields and another kind of field and also has a
  // focused non street address.
  formatter = LabelFormatter::Create(
      profiles, "en-US", ADDRESS_HOME_CITY,
      {ADDRESS_HOME_CITY, ADDRESS_HOME_STATE, EMAIL_ADDRESS});
  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(u"emailA@gmail.com", u"emailB@gmail.com"));

  // Tests that a phone number is shown when the address cannot be shown and
  // there are different phone numbers.
  profileB = GetProfileA();
  profileB.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"15185550000", "en-US");
  profiles = {&profileA, &profileB};

  formatter = LabelFormatter::Create(profiles, "en-US", ADDRESS_HOME_LINE1,
                                     GetAddressPlusContactFieldTypes());
  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(u"(617) 666-0000", u"(518) 555-0000"));

  // Tests that an email address is shown when the address cannot be shown and
  // there are different email addresses.
  profileB = GetProfileA();
  profileB.SetInfo(EMAIL_ADDRESS, u"emailB@gmail.com", "en-US");
  profiles = {&profileA, &profileB};

  formatter = LabelFormatter::Create(profiles, "en-US", ADDRESS_HOME_LINE1,
                                     GetAddressPlusContactFieldTypes());
  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(u"emailA@gmail.com", u"emailB@gmail.com"));

  // Tests that a name is shown when the address cannot be shown and there are
  // different names.
  profileB = GetProfileA();
  profileB.SetInfo(NAME_FIRST, u"firstB", "en-US");
  profileB.SetInfo(NAME_LAST, u"lastB", "en-US");
  profiles = {&profileA, &profileB};

  formatter = LabelFormatter::Create(profiles, "en-US", ADDRESS_HOME_LINE1,
                                     GetAddressPlusContactFieldTypes());
  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(u"firstA lastA", u"firstB lastB"));

  // Tests that a phone number is shown when the address cannot be shown, when
  // profiles have the same data for unfocused form fields, and when the form
  // has a phone number field.
  profileB = GetProfileA();
  profiles = {&profileA, &profileB};
  formatter = LabelFormatter::Create(profiles, "en-US", ADDRESS_HOME_LINE1,
                                     GetAddressPlusContactFieldTypes());
  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(u"(617) 666-0000", u"(617) 666-0000"));

  // Tests that an email address is shown when the address cannot be shown, when
  // profiles have the same data for unfocused form fields, and when the form
  // does not have a phone number field, but has an email address field.
  profileB = GetProfileA();
  profiles = {&profileA, &profileB};
  formatter = LabelFormatter::Create(profiles, "en-US", ADDRESS_HOME_LINE1,
                                     GetAddressPlusEmailFieldTypes());
  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(u"emailA@gmail.com", u"emailA@gmail.com"));

  // Tests that a name is shown when the address cannot be shown, when profiles
  // have the same data for unfocused form fields, and when the form does not
  // have a phone number or email address field, but has a name field.
  profileB = GetProfileA();
  profiles = {&profileA, &profileB};
  formatter = LabelFormatter::Create(profiles, "en-US", ADDRESS_HOME_LINE1,
                                     GetAddressOnlyFieldTypes());
  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(u"firstA lastA", u"firstA lastA"));
}

TEST(MobileLabelFormatterTest,
     GetLabelsForFocusedAddress_UniqueProfile_ShowOne) {
  std::map<std::string, std::string> parameters;
  parameters[features::kAutofillUseMobileLabelDisambiguationParameterName] =
      features::kAutofillUseMobileLabelDisambiguationParameterShowOne;

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kAutofillUseMobileLabelDisambiguation, parameters);

  AutofillProfile profileA = GetProfileA();
  std::vector<const AutofillProfile*> profiles{&profileA};

  // Tests that the second most important piece of data, phone, is shown when
  // the form has an unfocused form field corresponding to this data and the
  // most important piece cannot be shown.
  std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", ADDRESS_HOME_LINE1, GetAddressPlusContactFieldTypes());
  EXPECT_THAT(formatter->GetLabels(), ElementsAre(u"(617) 666-0000"));

  // Tests that the third most important piece of data, email, is shown when
  // the form has an unfocused form field corresponding to this data and the
  // two most important pieces of data cannot be shown.
  formatter = LabelFormatter::Create(profiles, "en-US", ADDRESS_HOME_LINE1,
                                     GetAddressPlusEmailFieldTypes());
  EXPECT_THAT(formatter->GetLabels(), ElementsAre(u"emailA@gmail.com"));

  // Tests that the least important piece of data, name, is shown when the form
  // has an unfocused form field corresponding to this data and the more
  // important pieces of data cannot be shown.
  formatter = LabelFormatter::Create(profiles, "en-US", ADDRESS_HOME_LINE1,
                                     GetAddressOnlyFieldTypes());
  EXPECT_THAT(formatter->GetLabels(), ElementsAre(u"firstA lastA"));
}

TEST(MobileLabelFormatterTest, GetLabels_DistinctProfiles_ShowAll) {
  std::map<std::string, std::string> parameters;
  parameters[features::kAutofillUseMobileLabelDisambiguationParameterName] =
      features::kAutofillUseMobileLabelDisambiguationParameterShowAll;

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kAutofillUseMobileLabelDisambiguation, parameters);

  AutofillProfile profileA = GetProfileA();
  AutofillProfile profileB = GetProfileB();
  AutofillProfile profileC;
  test::SetProfileInfo(&profileC, "firstC", "middleC", "lastC", "", "", "", "",
                       "", "", "", "US", "");
  const std::vector<const AutofillProfile*> profiles{&profileA, &profileB,
                                                     &profileC};

  // Tests that unfocused data that is not the same across profiles is shown in
  // the label for forms with addresses.
  std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", NAME_FIRST, GetAddressPlusContactFieldTypes());
  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(u"address1A, address2A, (617) 666-0000, emailA@gmail.com",
                  u"address1B, address2B, (518) 555-0000, emailB@gmail.com",
                  std::u16string()));

  // Like the previous test, but focuses on an address field rather than a name
  // field to check that the name is correctly added to the label.
  formatter = LabelFormatter::Create(profiles, "en-US", ADDRESS_HOME_LINE1,
                                     GetAddressPlusContactFieldTypes());
  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(u"firstA, (617) 666-0000, emailA@gmail.com",
                  u"firstB, (518) 555-0000, emailB@gmail.com", u"firstC"));

  // Tests that unfocused data that is not the same across profiles is shown in
  // the label for forms with non street addresses and without street addresses.
  formatter = LabelFormatter::Create(profiles, "en-US", NAME_FIRST,
                                     {NAME_FIRST, NAME_LAST, ADDRESS_HOME_ZIP});
  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(u"02113", u"12224", std::u16string()));

  // Like the previous test, but focuses on an address field rather than a name
  // field to check that the name is correctly added to the label.
  formatter = LabelFormatter::Create(profiles, "en-US", ADDRESS_HOME_ZIP,
                                     {NAME_FIRST, NAME_LAST, ADDRESS_HOME_ZIP});
  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(u"firstA", u"firstB", u"firstC"));

  // Tests that unfocused data that is not the same across profiles is shown in
  // the label for forms without addresses.
  formatter = LabelFormatter::Create(profiles, "en-US", NAME_FIRST,
                                     GetContactOnlyFieldTypes());
  EXPECT_THAT(
      formatter->GetLabels(),
      ElementsAre(u"(617) 666-0000, emailA@gmail.com",
                  u"(518) 555-0000, emailB@gmail.com", std::u16string()));

  // Like the previous test, but focuses on a phone field rather than a name
  // field to check that the name is correctly added to the label.
  formatter = LabelFormatter::Create(profiles, "en-US", PHONE_HOME_WHOLE_NUMBER,
                                     GetContactOnlyFieldTypes());
  EXPECT_THAT(formatter->GetLabels(),
              ElementsAre(u"firstA, emailA@gmail.com",
                          u"firstB, emailB@gmail.com", u"firstC"));
}

TEST(MobileLabelFormatterTest, GetDefaultLabel_ShowAll) {
  std::map<std::string, std::string> parameters;
  parameters[features::kAutofillUseMobileLabelDisambiguationParameterName] =
      features::kAutofillUseMobileLabelDisambiguationParameterShowAll;

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kAutofillUseMobileLabelDisambiguation, parameters);

  AutofillProfile profileA = GetProfileA();
  const std::vector<const AutofillProfile*> profiles{&profileA};

  // Tests that the most important piece of data, address, is shown when the
  // form has an unfocused form field corresponding to this data.
  std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      profiles, "en-US", NAME_FIRST, GetAddressPlusContactFieldTypes());
  EXPECT_THAT(formatter->GetLabels(), ElementsAre(u"address1A, address2A"));

  // Tests that the most important piece of data, address, is shown when the
  // form has an unfocused form field corresponding to this data.
  formatter = LabelFormatter::Create(profiles, "en-US", NAME_FIRST,
                                     {NAME_FIRST, NAME_LAST, ADDRESS_HOME_ZIP});
  EXPECT_THAT(formatter->GetLabels(), ElementsAre(u"02113"));

  // Tests that the second most important piece of data, phone, is shown when
  // the form has an unfocused form field corresponding to this data and the
  // most important piece cannot be shown.
  formatter = LabelFormatter::Create(profiles, "en-US", ADDRESS_HOME_LINE1,
                                     GetAddressPlusContactFieldTypes());
  EXPECT_THAT(formatter->GetLabels(), ElementsAre(u"(617) 666-0000"));

  // Tests that the third most important piece of data, email, is shown when
  // the form has an unfocused form field corresponding to this data and the
  // two more important pieces of data cannot be shown.
  formatter = LabelFormatter::Create(profiles, "en-US", PHONE_HOME_WHOLE_NUMBER,
                                     GetContactOnlyFieldTypes());
  EXPECT_THAT(formatter->GetLabels(), ElementsAre(u"emailA@gmail.com"));

  // Tests that the least important piece of data, name, is shown when
  // the form has an unfocused form field corresponding to this data and the
  // more important pieces of data cannot be shown.
  formatter =
      LabelFormatter::Create(profiles, "en-US", PHONE_HOME_WHOLE_NUMBER,
                             {NAME_FIRST, NAME_LAST, PHONE_HOME_WHOLE_NUMBER});
  EXPECT_THAT(formatter->GetLabels(), ElementsAre(u"firstA lastA"));

  // Tests that a non street address is shown when a form contains only
  // non focused street address fields and a focused non street address.
  formatter = LabelFormatter::Create(
      profiles, "en-US", ADDRESS_HOME_ZIP,
      {ADDRESS_HOME_CITY, ADDRESS_HOME_STATE, ADDRESS_HOME_ZIP});
  EXPECT_THAT(formatter->GetLabels(), ElementsAre(u"cityA, MA"));

  // Tests that a non street address is not shown when a form contains
  // non focused street address fields and another kind of field and also has a
  // focused non street address.
  formatter = LabelFormatter::Create(
      profiles, "en-US", ADDRESS_HOME_CITY,
      {ADDRESS_HOME_CITY, ADDRESS_HOME_STATE, EMAIL_ADDRESS});
  EXPECT_THAT(formatter->GetLabels(), ElementsAre(u"emailA@gmail.com"));
}

}  // namespace
}  // namespace autofill
