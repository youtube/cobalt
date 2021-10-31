// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/css_computed_style_declaration.h"

#include "cobalt/cssom/css_declared_style_data.h"
#include "cobalt/dom/dom_exception.h"

namespace cobalt {
namespace cssom {
// This returns the result of serializing a CSS declaration block.
//   https://www.w3.org/TR/cssom/#serialize-a-css-declaration-block
std::string CSSComputedStyleDeclaration::css_text(
    script::ExceptionState* exception_state) const {
  // The current implementation does not handle shorthands.
  NOTIMPLEMENTED();
  return data_ ? data_->SerializeCSSDeclarationBlock() : std::string();
}

void CSSComputedStyleDeclaration::set_css_text(
    const std::string& css_text, script::ExceptionState* exception_state) {
  dom::DOMException::Raise(dom::DOMException::kInvalidAccessErr,
                           exception_state);
}

// The length attribute must return the number of CSS declarations in the
// declarations.
//   https://www.w3.org/TR/cssom/#dom-cssstyledeclaration-length
unsigned int CSSComputedStyleDeclaration::length() const {
  // Computed style declarations have all known longhand properties.
  return kMaxLonghandPropertyKey + 1;
}

// The item(index) method must return the property name of the CSS declaration
// at position index.
//   https://www.w3.org/TR/cssom/#dom-cssstyledeclaration-item
base::Optional<std::string> CSSComputedStyleDeclaration::Item(
    unsigned int index) const {
  if (index >= length()) return base::nullopt;
  return base::Optional<std::string>(
      GetPropertyName(GetLexicographicalLonghandPropertyKey(index)));
}

std::string CSSComputedStyleDeclaration::GetDeclaredPropertyValueStringByKey(
    const PropertyKey key) const {
  if (!data_ || key == kNoneProperty) {
    return std::string();
  }
  if (key > kMaxLonghandPropertyKey) {
    // Shorthand properties are never directly stored as computed style
    // properties.
    // TODO: Implement serialization of css values, see
    // https://www.w3.org/TR/cssom-1/#serializing-css-values
    DCHECK_LE(key, kMaxEveryPropertyKey);
    NOTIMPLEMENTED();
    DLOG(WARNING) << "Unsupported property query for \"" << GetPropertyName(key)
                  << "\": Returning of property value strings is not "
                     "supported for shorthand properties.";
    return std::string();
  }
  const scoped_refptr<PropertyValue>& property_value =
      data_->GetPropertyValueReference(key);
  DCHECK(property_value);
  return property_value->ToString();
}

void CSSComputedStyleDeclaration::SetPropertyValue(
    const std::string& property_name, const std::string& property_value,
    script::ExceptionState* exception_state) {
  dom::DOMException::Raise(dom::DOMException::kInvalidAccessErr,
                           exception_state);
}

void CSSComputedStyleDeclaration::SetProperty(
    const std::string& property_name, const std::string& property_value,
    const std::string& priority, script::ExceptionState* exception_state) {
  dom::DOMException::Raise(dom::DOMException::kInvalidAccessErr,
                           exception_state);
}

void CSSComputedStyleDeclaration::SetData(
    const scoped_refptr<const CSSComputedStyleData>& data) {
  data_ = data;
  // After setting |data_|, |data_with_inherited_properties_| needs to be
  // updated. It may have changed.
  UpdateInheritedData();
}

void CSSComputedStyleDeclaration::UpdateInheritedData() {
  if (!data_) {
    // If there's no data, then there can be no data with inherited properties.
    data_with_inherited_properties_ = NULL;
  } else if (data_->has_declared_inherited_properties()) {
    // Otherwise, if the data has inherited properties, then it's also the first
    // data with inherited properties.
    data_with_inherited_properties_ = data_;
  } else {
    // Otherwise, |data_with_inherited_properties_| should be set to the parent
    // computed style's |data_with_inherited_properties_|. This is because the
    // updates always cascade down the tree and the parent is guaranteed to
    // have already been updated when the child is updated.
    const scoped_refptr<CSSComputedStyleDeclaration>&
        parent_computed_style_declaration =
            data_->GetParentComputedStyleDeclaration();
    if (parent_computed_style_declaration) {
      data_with_inherited_properties_ =
          parent_computed_style_declaration->data_with_inherited_properties_;
    } else {
      data_with_inherited_properties_ = NULL;
    }
  }
}

const scoped_refptr<PropertyValue>&
CSSComputedStyleDeclaration::GetInheritedPropertyValueReference(
    PropertyKey key) const {
  DCHECK(data_with_inherited_properties_);
  return data_with_inherited_properties_->GetPropertyValueReference(key);
}

}  // namespace cssom
}  // namespace cobalt
