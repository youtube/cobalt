// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/entity_type.h"

#include "components/autofill/core/browser/data_model/entity_type_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

TEST(AutofillAttributeTypeTest, Relationships) {
  AttributeType a = AttributeType(AttributeTypeName::kPassportName);
  EXPECT_EQ(a.entity_type(), EntityType(EntityTypeName::kPassport));
  EXPECT_EQ(a.field_type(), PASSPORT_NAME_TAG);
  EXPECT_EQ(a, AttributeType::FromFieldType(PASSPORT_NAME_TAG));
}

TEST(AutofillEntityTypeTest, Attributes) {
  using enum AttributeTypeName;
  EntityType e = EntityType(EntityTypeName::kPassport);
  EXPECT_THAT(e.attributes(),
              UnorderedElementsAre(AttributeType(kPassportName),
                                   AttributeType(kPassportNumber),
                                   AttributeType(kPassportCountry),
                                   AttributeType(kPassportExpiryDate),
                                   AttributeType(kPassportIssueDate)));
  ASSERT_FALSE(e.attributes().empty());
}

TEST(AutofillEntityTypeTest, ImportConstraints) {
  using enum AttributeTypeName;
  EntityType e = EntityType(EntityTypeName::kPassport);
  EXPECT_THAT(e.import_constraints(),
              UnorderedElementsAre(
                  UnorderedElementsAre(AttributeType(kPassportNumber),
                                       AttributeType(kPassportName)),
                  UnorderedElementsAre(AttributeType(kPassportNumber),
                                       AttributeType(kPassportCountry)),
                  UnorderedElementsAre(AttributeType(kPassportNumber),
                                       AttributeType(kPassportExpiryDate))));
}

TEST(AutofillEntityTypeTest, MergeConstraints) {
  using enum AttributeTypeName;
  EntityType e = EntityType(EntityTypeName::kPassport);
  EXPECT_THAT(e.merge_constraints(), ElementsAre(UnorderedElementsAre(
                                         AttributeType(kPassportNumber),
                                         AttributeType(kPassportExpiryDate))));
}

TEST(AutofillEntityTypeTest, NameAsString) {
  EntityType e = EntityType(EntityTypeName::kPassport);
  AttributeType a = *e.attributes().begin();
  EXPECT_EQ(e.name_as_string(), "Passport");
  EXPECT_EQ(a.name_as_string(), "Name");
}

TEST(AutofillEntityTypeTest, DisambiguationOrder) {
  using enum AttributeTypeName;
  auto lt = [](AttributeTypeName lhs, AttributeTypeName rhs) {
    return AttributeType::DisambiguationOrder(AttributeType(lhs),
                                              AttributeType(rhs));
  };
  EXPECT_TRUE(lt(kPassportName, kPassportCountry));
  EXPECT_TRUE(lt(kPassportCountry, kPassportExpiryDate));
  EXPECT_TRUE(lt(kPassportCountry, kPassportIssueDate));
  EXPECT_TRUE(lt(kPassportCountry, kPassportNumber));
  EXPECT_FALSE(lt(kPassportNumber, kPassportIssueDate));
}

TEST(AutofillEntityTypeTest, Syncable) {
  using enum EntityTypeName;
  EXPECT_FALSE(EntityType(kPassport).syncable());
  EXPECT_TRUE(EntityType(kLoyaltyCard).syncable());
}

TEST(AutofillEntityTypeTest, EntityGetNameForI18n) {
  using enum EntityTypeName;
  EntityType a = EntityType(kPassport);
  EntityType b = EntityType(kDriversLicense);
  EXPECT_EQ(a.GetNameForI18n(), u"Passport");
  EXPECT_EQ(b.GetNameForI18n(), u"Driver's license");
}

TEST(AutofillEntityTypeTest, AttributeGetNameForI18n) {
  using enum AttributeTypeName;
  AttributeType a = AttributeType(kPassportCountry);
  AttributeType b = AttributeType(kVehicleLicensePlate);
  AttributeType c = AttributeType(kDriversLicenseExpirationDate);
  EXPECT_EQ(a.GetNameForI18n(), u"Country");
  EXPECT_EQ(b.GetNameForI18n(), u"License plate");
  EXPECT_EQ(c.GetNameForI18n(), u"Expiration date");
}

}  // namespace
}  // namespace autofill
