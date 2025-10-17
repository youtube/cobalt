// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_AUTOFILL_STRUCTURED_ADDRESS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_AUTOFILL_STRUCTURED_ADDRESS_H_

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_feature_guarded_address_component.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// The name of the street.
class StreetNameNode : public AddressComponent {
 public:
  explicit StreetNameNode(SubcomponentsList children);
  ~StreetNameNode() override;
};

// The house number. It also contains the subunit descriptor, e.g. the 'a' in
// '73a'.
class HouseNumberNode : public AddressComponent {
 public:
  explicit HouseNumberNode(SubcomponentsList children);
  ~HouseNumberNode() override;
};

// Contains the specific location in the street (e.g. street name and house
// number info.)
class StreetLocationNode : public AddressComponent {
 public:
  explicit StreetLocationNode(SubcomponentsList children);
  ~StreetLocationNode() override;
};

// The floor the apartment is located in.
class FloorNode : public AddressComponent {
 public:
  explicit FloorNode(SubcomponentsList children);
  ~FloorNode() override;
};

// The number of the apartment.
class ApartmentNode : public AddressComponent {
 public:
  explicit ApartmentNode(SubcomponentsList children);
  ~ApartmentNode() override;
};

// The SubPremise normally contains the floor and the apartment number.
class SubPremiseNode : public AddressComponent {
 public:
  explicit SubPremiseNode(SubcomponentsList children);
  ~SubPremiseNode() override;
};

// Stores the landmark of an address profile.
class LandmarkNode : public AddressComponent {
 public:
  explicit LandmarkNode(SubcomponentsList children);
  ~LandmarkNode() override;
};

// Stores the streets intersection of an address profile.
class BetweenStreetsNode : public AddressComponent {
 public:
  explicit BetweenStreetsNode(SubcomponentsList children);
  ~BetweenStreetsNode() override;
};
class BetweenStreets1Node : public AddressComponent {
 public:
  explicit BetweenStreets1Node(SubcomponentsList children);
  ~BetweenStreets1Node() override;
};
class BetweenStreets2Node : public AddressComponent {
 public:
  explicit BetweenStreets2Node(SubcomponentsList children);
  ~BetweenStreets2Node() override;
};

// Stores administrative area level 2. A sub-division of a state, e.g. a
// Municipio in Brazil or Mexico.
class AdminLevel2Node : public AddressComponent {
 public:
  explicit AdminLevel2Node(SubcomponentsList children);
  ~AdminLevel2Node() override;
};

// Stores address overflow fields in countries that assign a fixed meaning to
// overflow fields, meaning that forms follow a consistent structure that is
// typically identical across domains while also providing an option for an
// overflow field.
class AddressOverflowNode : public AddressComponent {
 public:
  explicit AddressOverflowNode(SubcomponentsList children);
  ~AddressOverflowNode() override;
};

class AddressOverflowAndLandmarkNode : public AddressComponent {
 public:
  explicit AddressOverflowAndLandmarkNode(SubcomponentsList children);
  ~AddressOverflowAndLandmarkNode() override;
};

class BetweenStreetsOrLandmarkNode : public AddressComponent {
 public:
  explicit BetweenStreetsOrLandmarkNode(SubcomponentsList children);
  ~BetweenStreetsOrLandmarkNode() override;
};

// The StreetAddress incorporates all the information specifically related to
// the street address (e.g. street location. between streets, subpremise, etc).
class StreetAddressNode : public AddressComponent {
 public:
  explicit StreetAddressNode(SubcomponentsList children);
  ~StreetAddressNode() override;

  const FieldTypeSet GetAdditionalSupportedFieldTypes() const override;

  void SetValue(std::u16string value, VerificationStatus status) override;

  void UnsetValue() override;

  std::u16string GetValueForComparison(
      const std::u16string& value,
      const AddressComponent& other) const override;

 protected:
  // Gives the component with the higher verification status precedence.
  // If the statuses are the same, the older component gets precedence if it
  // contains newlines but the newer one does not.
  bool HasNewerValuePrecedenceInMerging(
      const AddressComponent& newer_component) const override;

  std::vector<const re2::RE2*> GetParseRegularExpressionsByRelevance()
      const override;

  // Recalculates the address line after an assignment.
  void PostAssignSanitization() override;

  // Apply line-wise parsing of the street address as a fallback method.
  void ParseValueAndAssignSubcomponentsByFallbackMethod() override;

  // Implements support for getting the value of the individual address lines.
  std::u16string GetValueForOtherSupportedType(
      FieldType field_type) const override;

  // Implements support for setting the value of the individual address lines.
  void SetValueForOtherSupportedType(FieldType field_type,
                                     const std::u16string& value,
                                     const VerificationStatus& status) override;

  // Returns true of the address lines do not contain an empty line.
  bool IsValueValid() const override;

 private:
  // Calculates the address line from the street address.
  void CalculateAddressLines();

  // Returns the corresponding address line depending on `type`. Assumes that
  // `type` is ADDRESS_HOME_LINE(1|2|3).
  std::u16string GetAddressLine(FieldType type) const;

  // Holds the values of the individual address lines.
  // Must be recalculated if the value of the component changes.
  std::vector<std::u16string> address_lines_;
};

// Stores the country code of an address profile. This is not related to
// `CountryInfo` but to `Address` (see CountryInfo documentation for the
// differences).
class CountryCodeNode : public AddressComponent {
 public:
  explicit CountryCodeNode(SubcomponentsList children);
  ~CountryCodeNode() override;
};

// Stores the city of an address.
class DependentLocalityNode : public AddressComponent {
 public:
  explicit DependentLocalityNode(SubcomponentsList children);
  ~DependentLocalityNode() override;
};

// Stores the city of an address.
class CityNode : public AddressComponent {
 public:
  explicit CityNode(SubcomponentsList children);
  ~CityNode() override;
};

// Stores the state of an address.
class StateNode : public AddressComponent {
 public:
  explicit StateNode(SubcomponentsList children);
  ~StateNode() override;

  // For states we use the AlternativeStateNameMap to offer canonicalized state
  // names.
  std::optional<std::u16string> GetCanonicalizedValue() const override;

  std::u16string GetValueForComparison(
      const std::u16string& value,
      const AddressComponent& other) const override;
};

// Stores the postal code of an address.
class PostalCodeNode : public AddressComponent {
 public:
  explicit PostalCodeNode(SubcomponentsList children);
  ~PostalCodeNode() override;

  std::u16string GetValueForComparison(
      const std::u16string& value,
      const AddressComponent& other) const override;
};

// Stores the sorting code.
class SortingCodeNode : public AddressComponent {
 public:
  explicit SortingCodeNode(SubcomponentsList children);
  ~SortingCodeNode() override;
};

// Stores the overall Address that contains every other address related node.
class AddressNode : public AddressComponent {
 public:
  AddressNode();
  AddressNode(const AddressNode& other);
  explicit AddressNode(SubcomponentsList children);
  AddressNode& operator=(const AddressNode& other);
  ~AddressNode() override;

  void MigrateLegacyStructure() override;

  // Checks if the street address contains an invalid structure and wipes it if
  // necessary.
  bool WipeInvalidStructure() override;
};

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_AUTOFILL_STRUCTURED_ADDRESS_H_
