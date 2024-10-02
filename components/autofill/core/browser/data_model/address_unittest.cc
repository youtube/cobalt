// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/address.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

class AddressTest : public testing::Test {
 public:
  AddressTest() { CountryNames::SetLocaleString("en-US"); }
};

// Test that country data can be properly returned as either a country code or a
// localized country name.
TEST_F(AddressTest, GetCountry) {
  Address address;
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));

  // Make sure that nothing breaks when the country code is missing.
  std::u16string country =
      address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(std::u16string(), country);

  address.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), u"US", "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(u"United States", country);
  country = address.GetInfo(
      AutofillType(HtmlFieldType::kCountryName, HtmlFieldMode::kNone), "en-US");
  EXPECT_EQ(u"United States", country);
  country = address.GetInfo(
      AutofillType(HtmlFieldType::kCountryCode, HtmlFieldMode::kNone), "en-US");
  EXPECT_EQ(u"US", country);

  address.SetRawInfo(ADDRESS_HOME_COUNTRY, u"CA");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(u"Canada", country);
  country = address.GetInfo(
      AutofillType(HtmlFieldType::kCountryName, HtmlFieldMode::kNone), "en-US");
  EXPECT_EQ(u"Canada", country);
  country = address.GetInfo(
      AutofillType(HtmlFieldType::kCountryCode, HtmlFieldMode::kNone), "en-US");
  EXPECT_EQ(u"CA", country);
}

// Test that country data can be properly returned as either a country code or a
// full country name that can even be localized.
TEST_F(AddressTest, SetHtmlCountryCodeTypeWithFullCountryName) {
  Address address;
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));

  // Create an autofill type from HtmlFieldType::kCountryCode.
  AutofillType autofill_type(HtmlFieldType::kCountryCode, HtmlFieldMode::kNone);

  // Test that the country value can be set and retrieved if it is not
  // a country code but a full country name.
  address.SetInfo(autofill_type, u"Germany", "en-US");
  std::u16string actual_country =
      address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  std::u16string actual_country_code = address.GetInfo(
      AutofillType(HtmlFieldType::kCountryCode, HtmlFieldMode::kNone), "en-US");
  EXPECT_EQ(u"Germany", actual_country);
  EXPECT_EQ(u"DE", actual_country_code);

  // Reset the country and verify that the reset works as expected.
  address.SetInfo(autofill_type, u"", "en-US");
  actual_country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  actual_country_code = address.GetInfo(
      AutofillType(HtmlFieldType::kCountryCode, HtmlFieldMode::kNone), "en-US");
  EXPECT_EQ(u"", actual_country);
  EXPECT_EQ(u"", actual_country_code);

  // Test that the country value can be set and retrieved if it is not
  // a country code but a full country name with a non-standard locale.
  address.SetInfo(autofill_type, u"deutschland", "de");
  actual_country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  actual_country_code = address.GetInfo(
      AutofillType(HtmlFieldType::kCountryCode, HtmlFieldMode::kNone), "en-US");
  EXPECT_EQ(u"Germany", actual_country);
  EXPECT_EQ(u"DE", actual_country_code);

  // Reset the country.
  address.SetInfo(autofill_type, u"", "en-US");

  // Test that the country is still stored correctly with a supplied
  // country code.
  address.SetInfo(autofill_type, u"DE", "en-US");
  actual_country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  actual_country_code = address.GetInfo(
      AutofillType(HtmlFieldType::kCountryCode, HtmlFieldMode::kNone), "en-US");
  EXPECT_EQ(u"DE", actual_country_code);
  EXPECT_EQ(u"Germany", actual_country);
}

// Test that we properly detect country codes appropriate for each country.
TEST_F(AddressTest, SetCountry) {
  Address address;
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));

  // Test basic conversion.
  address.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), u"United States",
                  "en-US");
  std::u16string country =
      address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(u"US", address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(u"United States", country);

  // Test basic synonym detection.
  address.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), u"USA", "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(u"US", address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(u"United States", country);

  // Test case-insensitivity.
  address.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), u"canADA", "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(u"CA", address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(u"Canada", country);

  // Test country code detection.
  address.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), u"JP", "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(u"JP", address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(u"Japan", country);

  // Test that we ignore unknown countries.
  address.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), u"Unknown", "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(std::u16string(), country);

  // Test setting the country based on an HTML field type.
  AutofillType html_type_country_code =
      AutofillType(HtmlFieldType::kCountryCode, HtmlFieldMode::kNone);
  address.SetInfo(html_type_country_code, u"US", "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(u"US", address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(u"United States", country);

  // Test case-insensitivity when setting the country based on an HTML field
  // type.
  address.SetInfo(html_type_country_code, u"cA", "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(u"CA", address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(u"Canada", country);

  // Test setting the country based on invalid data with an HTML field type.
  address.SetInfo(html_type_country_code, u"unknown", "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(std::u16string(), country);
}

// Test setting and getting the new structured address tokens
TEST_F(AddressTest, StructuredAddressTokens) {
  Address address;

  // Set the address tokens.
  address.SetRawInfo(ADDRESS_HOME_STREET_NAME, u"StreetName");
  address.SetRawInfo(ADDRESS_HOME_HOUSE_NUMBER, u"HouseNumber");
  address.SetRawInfo(ADDRESS_HOME_DEPENDENT_STREET_NAME,
                     u"DependentStreetName");
  address.SetRawInfo(ADDRESS_HOME_PREMISE_NAME, u"PremiseNmae");
  address.SetRawInfo(ADDRESS_HOME_SUBPREMISE, u"SubPremise");

  // Retrieve the tokens and verify that they are correct.
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_NAME), u"StreetName");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"HouseNumber");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_DEPENDENT_STREET_NAME),
            u"DependentStreetName");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_PREMISE_NAME), u"PremiseNmae");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_SUBPREMISE), u"SubPremise");
}

// Test that we properly match typed values to stored country data.
TEST_F(AddressTest, IsCountry) {
  Address address;
  address.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");

  const char* const kValidMatches[] = {"United States", "USA", "US",
                                       "United states", "us"};
  for (const char* valid_match : kValidMatches) {
    SCOPED_TRACE(valid_match);
    ServerFieldTypeSet matching_types;
    address.GetMatchingTypes(ASCIIToUTF16(valid_match), "US", &matching_types);
    ASSERT_EQ(1U, matching_types.size());
    EXPECT_EQ(ADDRESS_HOME_COUNTRY, *matching_types.begin());
  }

  const char* const kInvalidMatches[] = {"United", "Garbage"};
  for (const char* invalid_match : kInvalidMatches) {
    ServerFieldTypeSet matching_types;
    address.GetMatchingTypes(ASCIIToUTF16(invalid_match), "US",
                             &matching_types);
    EXPECT_EQ(0U, matching_types.size());
  }

  // Make sure that garbage values don't match when the country code is empty.
  address.SetRawInfo(ADDRESS_HOME_COUNTRY, std::u16string());
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  ServerFieldTypeSet matching_types;
  address.GetMatchingTypes(u"Garbage", "US", &matching_types);
  EXPECT_EQ(0U, matching_types.size());
}

// Verifies that Address::GetInfo() correctly combines address lines.
TEST_F(AddressTest, GetStreetAddress) {
  const AutofillType type = AutofillType(ADDRESS_HOME_STREET_ADDRESS);

  // Address has no address lines.
  Address address;
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE3).empty());
  EXPECT_EQ(std::u16string(), address.GetInfo(type, "en-US"));

  // Address has only line 1.
  address.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Example Ave.");
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE3).empty());
  EXPECT_EQ(u"123 Example Ave.", address.GetInfo(type, "en-US"));

  // Address has only line 2.
  address.SetRawInfo(ADDRESS_HOME_LINE1, std::u16string());
  address.SetRawInfo(ADDRESS_HOME_LINE2, u"Apt 42.");
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE3).empty());
  EXPECT_EQ(u"\nApt 42.", address.GetInfo(type, "en-US"));

  // Address has lines 1 and 2.
  address.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Example Ave.");
  address.SetRawInfo(ADDRESS_HOME_LINE2, u"Apt. 42");
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE3).empty());
  EXPECT_EQ(
      u"123 Example Ave.\n"
      u"Apt. 42",
      address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
  EXPECT_EQ(
      u"123 Example Ave.\n"
      u"Apt. 42",
      address.GetInfo(type, "en-US"));

  // A wild third line appears.
  address.SetRawInfo(ADDRESS_HOME_LINE3, u"Living room couch");
  EXPECT_EQ(u"Living room couch", address.GetRawInfo(ADDRESS_HOME_LINE3));
  EXPECT_EQ(
      u"123 Example Ave.\n"
      u"Apt. 42\n"
      u"Living room couch",
      address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));

  // The second line vanishes.
  address.SetRawInfo(ADDRESS_HOME_LINE2, std::u16string());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE3).empty());
  EXPECT_EQ(
      u"123 Example Ave.\n"
      u"\n"
      u"Living room couch",
      address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
}

// Verifies that overwriting an address with N lines with one that has fewer
// than N lines does not result in an address with blank lines at the end.
TEST_F(AddressTest, GetStreetAddressAfterOverwritingLongAddressWithShorterOne) {
  // Start with an address that has two lines.
  Address address;
  address.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Example Ave.");
  address.SetRawInfo(ADDRESS_HOME_LINE2, u"Apt. 42");

  // Now clear out the second address line.
  address.SetRawInfo(ADDRESS_HOME_LINE2, std::u16string());
  EXPECT_EQ(u"123 Example Ave.",
            address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));

  // Now clear out the first address line as well.
  address.SetRawInfo(ADDRESS_HOME_LINE1, std::u16string());
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
}

// Verifies that Address::SetRawInfo() is able to split address lines correctly.
TEST_F(AddressTest, SetRawStreetAddress) {
  const std::u16string empty_street_address;
  const std::u16string short_street_address = u"456 Nowhere Ln.";
  const std::u16string long_street_address =
      u"123 Example Ave.\n"
      u"Apt. 42\n"
      u"(The one with the blue door)";

  Address address;
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_LINE2));

  address.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, long_street_address);
  EXPECT_EQ(u"123 Example Ave.", address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(u"Apt. 42", address.GetRawInfo(ADDRESS_HOME_LINE2));
  EXPECT_EQ(long_street_address,
            address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));

  // A short address should clear out unused address lines.
  address.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, short_street_address);
  EXPECT_EQ(u"456 Nowhere Ln.", address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_LINE2));

  // An empty address should clear out all address lines.
  address.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, long_street_address);
  address.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, empty_street_address);
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_LINE2));
}

// Street addresses should be set properly.
TEST_F(AddressTest, SetStreetAddress) {
  const std::u16string empty_street_address;
  const std::u16string multi_line_street_address =
      u"789 Fancy Pkwy.\n"
      u"Unit 3.14\n"
      u"Box 9";
  const std::u16string single_line_street_address = u"123 Main, Apt 7";
  const AutofillType type = AutofillType(ADDRESS_HOME_STREET_ADDRESS);

  // Start with a non-empty address.
  Address address;
  address.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Example Ave.");
  address.SetRawInfo(ADDRESS_HOME_LINE2, u"Apt. 42");
  address.SetRawInfo(ADDRESS_HOME_LINE3, u"and a half");
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE3).empty());

  // Attempting to set a multi-line address should succeed.
  EXPECT_TRUE(address.SetInfo(type, multi_line_street_address, "en-US"));
  EXPECT_EQ(u"789 Fancy Pkwy.", address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(u"Unit 3.14", address.GetRawInfo(ADDRESS_HOME_LINE2));
  EXPECT_EQ(u"Box 9", address.GetRawInfo(ADDRESS_HOME_LINE3));

  // Setting a single line street address should clear out subsequent lines.
  EXPECT_TRUE(address.SetInfo(type, single_line_street_address, "en-US"));
  EXPECT_EQ(single_line_street_address, address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_LINE2));
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_LINE3));

  // Attempting to set an empty address should also succeed, and clear out the
  // previously stored data.
  EXPECT_TRUE(address.SetInfo(type, multi_line_street_address, "en-US"));
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE3).empty());
  EXPECT_TRUE(address.SetInfo(type, empty_street_address, "en-US"));
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_LINE2));
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_LINE3));
}

// Verifies that Address::SetInfio() rejects setting data for
// ADDRESS_HOME_STREET_ADDRESS if the data has any interior blank lines.
TEST_F(AddressTest, SetStreetAddressRejectsAddressesWithInteriorBlankLines) {
  // Start with a non-empty address.
  Address address;
  address.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Example Ave.");
  address.SetRawInfo(ADDRESS_HOME_LINE2, u"Apt. 42");
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS).empty());

  // Attempting to set an address with interior blank lines should fail, and
  // clear out the previously stored address.
  EXPECT_FALSE(address.SetInfo(AutofillType(ADDRESS_HOME_STREET_ADDRESS),
                               u"Address line 1\n"
                               u"\n"
                               u"Address line 3",
                               "en-US"));
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_LINE2));
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
}

// Verifies that Address::SetInfio() rejects setting data for
// ADDRESS_HOME_STREET_ADDRESS if the data has any leading blank lines.
TEST_F(AddressTest, SetStreetAddressRejectsAddressesWithLeadingBlankLines) {
  // Start with a non-empty address.
  Address address;
  address.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Example Ave.");
  address.SetRawInfo(ADDRESS_HOME_LINE2, u"Apt. 42");
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS).empty());

  // Attempting to set an address with leading blank lines should fail, and
  // clear out the previously stored address.
  EXPECT_FALSE(address.SetInfo(AutofillType(ADDRESS_HOME_STREET_ADDRESS),
                               u"\n"
                               u"Address line 2"
                               u"Address line 3",
                               "en-US"));
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_LINE2));
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
}

// Verifies that Address::SetInfio() rejects setting data for
// ADDRESS_HOME_STREET_ADDRESS if the data has any trailing blank lines.
TEST_F(AddressTest, SetStreetAddressRejectsAddressesWithTrailingBlankLines) {
  // Start with a non-empty address.
  Address address;
  address.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Example Ave.");
  address.SetRawInfo(ADDRESS_HOME_LINE2, u"Apt. 42");
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS).empty());

  // Attempting to set an address with leading blank lines should fail, and
  // clear out the previously stored address.
  EXPECT_FALSE(address.SetInfo(AutofillType(ADDRESS_HOME_STREET_ADDRESS),
                               u"Address line 1"
                               u"Address line 2"
                               u"\n",
                               "en-US"));
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_LINE2));
  EXPECT_EQ(std::u16string(), address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
}

// Verifies that the merging-related methods for structured addresses are
// implemented correctly. This is not a test of the merging logic itself.
TEST_F(AddressTest, TestMergeStructuredAddresses) {
  Address address1;
  Address address2;

  // Two empty addresses are mergeable by default.
  EXPECT_TRUE(address1.IsStructuredAddressMergeable(address2));

  // The two zip codes have a is-substring relation and are mergeable.
  address1.SetRawInfo(ADDRESS_HOME_ZIP, u"12345");
  address2.SetRawInfo(ADDRESS_HOME_ZIP, u"1234");
  EXPECT_TRUE(address2.IsStructuredAddressMergeable(address1));
  EXPECT_TRUE(address1.IsStructuredAddressMergeable(address2));

  // The merging should maintain the value because address2 is not more
  // recently used.
  address1.MergeStructuredAddress(address2,
                                  /*newer_use_more_recently_used=*/false);
  EXPECT_EQ(address1.GetRawInfo(ADDRESS_HOME_ZIP), u"12345");

  // Once it is more recently used, the value from address2 should be copied
  // into address1.
  address1.MergeStructuredAddress(address2,
                                  /*newer_use_more_recently_used=*/true);
  EXPECT_EQ(address1.GetRawInfo(ADDRESS_HOME_ZIP), u"1234");

  // With a second incompatible ZIP code the addresses are not mergeable
  // anymore.
  Address address3;
  address3.SetRawInfo(ADDRESS_HOME_ZIP, u"67890");
  EXPECT_FALSE(address1.IsStructuredAddressMergeable(address3));
}

// Tests that if only one of the structured addresses in a merge operation has
// country information, it is used as their common country during comparison and
// for rewriting rules.
TEST_F(AddressTest, TestMergeStructuredAddressesMissingCountry) {
  Address address1;
  Address address2;

  address1.SetRawInfo(ADDRESS_HOME_COUNTRY, u"GB");
  address1.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_ADDRESS,
                                            u"1 Trafalgar Square",
                                            VerificationStatus::kUserVerified);

  address2.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_ADDRESS,
                                            u"1 Trafalgar Square",
                                            VerificationStatus::kObserved);

  // |address1| and |address2|'s street address are not trivially the same,
  // because their verification status differs. But they should still be
  // mergeable, regardless if one of them has a country or not. This is not
  // trivially given, because country-specific rewriting rules are applied
  // during the merge process.
  EXPECT_TRUE(address1.IsStructuredAddressMergeable(address2));
  EXPECT_TRUE(address2.IsStructuredAddressMergeable(address1));
}

// Tests the retrieval of the structured address.
TEST_F(AddressTest, TestGettingTheStructuredAddress) {
  // Create the address and set a test value.
  Address address1;
  address1.SetRawInfo(ADDRESS_HOME_ZIP, u"12345");

  // Get the structured address and verify that it has the same test value set.
  AddressNode structured_address = address1.GetStructuredAddress();
  EXPECT_EQ(structured_address.GetValueForType(ADDRESS_HOME_ZIP), u"12345");
}

// For structured address, test that the structured information is wiped
// correctly when the unstructured street address changes.
TEST_F(AddressTest, ResetStructuredTokens) {
  Address address;
  // Set a structured address line and call the finalization routine.
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_ADDRESS,
                                           u"Erika-Mann-Str 12",
                                           VerificationStatus::kUserVerified);
  address.FinalizeAfterImport();

  // Verify that structured tokens have been assigned correctly.
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_NAME), u"Erika-Mann-Str");
  EXPECT_EQ(address.GetVerificationStatus(ADDRESS_HOME_STREET_NAME),
            VerificationStatus::kParsed);
  ASSERT_EQ(address.GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"12");
  EXPECT_EQ(address.GetVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER),
            VerificationStatus::kParsed);

  // Lift the verification status of the house number to be |kObserved|.
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER, u"12",
                                           VerificationStatus::kObserved);
  EXPECT_EQ(address.GetVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER),
            VerificationStatus::kObserved);

  // Now, set a new unstructured street address that has the same tokens in a
  // different order.
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_ADDRESS,
                                           u"12 Erika-Mann-Str",
                                           VerificationStatus::kUserVerified);

  // After this operation, the structure should be maintained including the
  // observed status of the house number.
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_NAME), u"Erika-Mann-Str");
  EXPECT_EQ(address.GetVerificationStatus(ADDRESS_HOME_STREET_NAME),
            VerificationStatus::kParsed);
  ASSERT_EQ(address.GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"12");
  EXPECT_EQ(address.GetVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER),
            VerificationStatus::kObserved);

  // Now set a different street address.
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_ADDRESS,
                                           u"Marienplatz",
                                           VerificationStatus::kUserVerified);

  // The set address is not parsable and the this should unset both the street
  // name and the house number.
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_NAME), u"");
  EXPECT_EQ(address.GetVerificationStatus(ADDRESS_HOME_STREET_NAME),
            VerificationStatus::kNoStatus);
  ASSERT_EQ(address.GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"");
  EXPECT_EQ(address.GetVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER),
            VerificationStatus::kNoStatus);
}

}  // namespace autofill
