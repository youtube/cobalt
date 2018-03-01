// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_CSSOM_CSS_DECLARED_STYLE_DATA_H_
#define COBALT_CSSOM_CSS_DECLARED_STYLE_DATA_H_

#include <bitset>
#include <functional>
#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/small_map.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/unused.h"
#include "cobalt/cssom/css_declaration_data.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

// CSSDeclaredStyleData which has PropertyValue type properties only used
// internally and it is not exposed to JavaScript.
class CSSDeclaredStyleData : public CSSDeclarationData {
 public:
  CSSDeclaredStyleData();

  // NOTE: The array size of base::SmallMap is based on extensive testing. Do
  // not change it unless additional profiling data justifies it.
  typedef base::SmallMap<std::map<PropertyKey, scoped_refptr<PropertyValue> >,
                         8, std::equal_to<PropertyKey> >
      PropertyValues;

  // The length attribute must return the number of CSS declarations in the
  // declarations.
  //   https://www.w3.org/TR/cssom/#dom-cssstyledeclaration-length
  unsigned int length() const;

  // The item(index) method must return the property name of the CSS declaration
  // at position index.
  //  https://www.w3.org/TR/cssom/#dom-cssstyledeclaration-item
  const char* Item(unsigned int index) const;

  // From CSSDeclarationData
  //
  bool IsSupportedPropertyKey(PropertyKey key) const override;

  scoped_refptr<PropertyValue> GetPropertyValue(PropertyKey key) const override;

  void SetPropertyValueAndImportance(
      PropertyKey key, const scoped_refptr<PropertyValue>& property_value,
      bool important) override;

  // Sets the specified property value to NULL and importance to false.  This
  // may be called on longhand properties as well as shorthand properties.
  void ClearPropertyValueAndImportance(PropertyKey key) override;

  // Rest of public methods.

  // Returns true if the property is explicitly declared in this style, as
  // opposed to implicitly inheriting from its parent or the initial value.
  bool IsDeclared(const PropertyKey key) const {
    return declared_properties_[key];
  }

  // This returns the value of the given property if that value is declared, or
  // an empty string if the property does not have a declared value.
  // Declarations in linked ancestor styles will not be considered.
  std::string GetPropertyValueString(const PropertyKey key) const;

  void AssignFrom(const CSSDeclaredStyleData& rhs);

  bool IsAnyDeclaredPropertyImportant() const {
    return important_properties_.any();
  }

  bool IsDeclaredPropertyImportant(PropertyKey key) const {
    return important_properties_[key];
  }

  // This returns the result of serializing a CSS declaration block.
  // The current implementation does not handle shorthands.
  //   https://www.w3.org/TR/cssom/#serialize-a-css-declaration-block
  std::string SerializeCSSDeclarationBlock() const;

  bool operator==(const CSSDeclaredStyleData& that) const;

  const PropertyValues& declared_property_values() const {
    return declared_property_values_;
  }

 private:
  // Helper function that can be called by ClearPropertyValueAndImportance().
  void ClearPropertyValueAndImportanceForLonghandProperty(PropertyKey key);

  PropertyValues::const_iterator Find(PropertyKey key) const;
  PropertyValues::iterator Find(PropertyKey key);

  LonghandPropertiesBitset declared_properties_;
  PropertyValues declared_property_values_;
  LonghandPropertiesBitset important_properties_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_DECLARED_STYLE_DATA_H_
