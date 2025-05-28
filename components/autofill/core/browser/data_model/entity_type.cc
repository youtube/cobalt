// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/entity_type.h"

#include <optional>

#include "base/notreached.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

// AttributeType::field_type() must be injective: distinct AttributeTypes must
// be mapped to distinct FieldTypes or to UNKNOWN_TYPE.
static_assert(
    std::ranges::all_of(DenseSet<AttributeType>::all(), [](AttributeType a) {
      return std::ranges::all_of(
          DenseSet<AttributeType>::all(), [&a](AttributeType b) {
            return a == b || a.field_type() == UNKNOWN_TYPE ||
                   a.field_type() != b.field_type();
          });
    }));

// static
std::optional<AttributeType> AttributeType::FromFieldType(FieldType type) {
  // This lookup table is the inverse of AttributeType::field_type().
  static constexpr auto kTable = []() {
    std::array<std::optional<AttributeType>, MAX_VALID_FIELD_TYPE> arr{};
    for (AttributeType at : DenseSet<AttributeType>::all()) {
      FieldType ft = at.field_type();
      CHECK(ft == UNKNOWN_TYPE || !arr[ft]);
      arr[ft] = ft != UNKNOWN_TYPE ? std::optional(at) : std::nullopt;
    }
    return arr;
  }();
  return 0 <= type && type < kTable.size() ? kTable[type] : std::nullopt;
}

std::u16string AttributeType::GetNameForI18n() const {
  switch (name()) {
    case AttributeTypeName::kPassportName:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_PASSPORT_NAME_ATTRIBUTE_NAME);
    case AttributeTypeName::kPassportNumber:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_PASSPORT_NUMBER_ATTRIBUTE_NAME);
    case AttributeTypeName::kPassportCountry:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_PASSPORT_COUNTRY_ATTRIBUTE_NAME);
    case AttributeTypeName::kPassportExpiryDate:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_PASSPORT_EXPIRY_DATE_ATTRIBUTE_NAME);
    case AttributeTypeName::kPassportIssueDate:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_PASSPORT_ISSUE_DATE_ATTRIBUTE_NAME);
    case AttributeTypeName::kLoyaltyCardProgram:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_LOYALTY_CARD_PROGRAM_ATTRIBUTE_NAME);
    case AttributeTypeName::kLoyaltyCardProvider:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_LOYALTY_CARD_PROVIDER_ATTRIBUTE_NAME);
    case AttributeTypeName::kLoyaltyCardMemberId:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_LOYALTY_CARD_MEMBER_ID_ATTRIBUTE_NAME);
    case AttributeTypeName::kVehicleOwner:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_VEHICLE_OWNER_ATTRIBUTE_NAME);
    case AttributeTypeName::kVehicleLicensePlate:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_VEHICLE_LICENSE_PLATE_ATTRIBUTE_NAME);
    case AttributeTypeName::kVehicleVin:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_VEHICLE_VIN_ATTRIBUTE_NAME);
    case AttributeTypeName::kVehicleMake:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_VEHICLE_MAKE_ATTRIBUTE_NAME);
    case AttributeTypeName::kVehicleModel:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_VEHICLE_MODEL_ATTRIBUTE_NAME);
    case AttributeTypeName::kDriversLicenseName:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_DRIVERS_LICENSE_NAME_ATTRIBUTE_NAME);
    case AttributeTypeName::kDriversLicenseRegion:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_DRIVERS_LICENSE_REGION_ATTRIBUTE_NAME);
    case AttributeTypeName::kDriversLicenseNumber:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_DRIVERS_LICENSE_NUMBER_ATTRIBUTE_NAME);
    case AttributeTypeName::kDriversLicenseExpirationDate:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_DRIVERS_LICENSE_EXPIRATION_DATE_ATTRIBUTE_NAME);
    case AttributeTypeName::kDriversLicenseIssueDate:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_DRIVERS_LICENSE_ISSUE_DATE_ATTRIBUTE_NAME);
  }
  NOTREACHED();
}

// static
bool EntityType::ImportOrder(const EntityType& lhs, const EntityType& rhs) {
  auto rank = [](const EntityType& t) constexpr {
    // Lower values indicate a higher priority.
    // For a deterministic behavior, distinct types should have distinct ranks.
    switch (t.name()) {
      case EntityTypeName::kPassport:
        return 1;
      case EntityTypeName::kLoyaltyCard:
        return 2;
      case EntityTypeName::kVehicle:
        return 3;
      case EntityTypeName::kDriversLicense:
        return 4;
    }
  };
  return rank(lhs) < rank(rhs);
}

std::u16string EntityType::GetNameForI18n() const {
  switch (name()) {
    case EntityTypeName::kPassport:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_PASSPORT_ENTITY_NAME);
    case EntityTypeName::kLoyaltyCard:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_LOYALTY_CARD_ENTITY_NAME);
    case EntityTypeName::kVehicle:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_VEHICLE_ENTITY_NAME);
    case EntityTypeName::kDriversLicense:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_DRIVERS_LICENSE_ENTITY_NAME);
  }
  NOTREACHED();
}

std::ostream& operator<<(std::ostream& os, AttributeType a) {
  return os << a.name_as_string();
}

std::ostream& operator<<(std::ostream& os, EntityType t) {
  return os << t.name_as_string();
}

}  // namespace autofill
