/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/cssom/css_computed_style_data.h"

#include <limits>

#include "base/lazy_instance.h"
#include "base/string_util.h"
#include "cobalt/cssom/css_computed_style_declaration.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"

namespace cobalt {
namespace cssom {

CSSComputedStyleData::PropertySetMatcher::PropertySetMatcher(
    const PropertyKeyVector& properties)
    : properties_(properties) {
  for (size_t i = 0; i < properties_.size(); ++i) {
    PropertyKey property_key = properties_[i];
    DCHECK_GT(property_key, kNoneProperty);
    DCHECK_LE(property_key, kMaxLonghandPropertyKey);
    properties_bitset_.set(property_key);
  }
}

bool CSSComputedStyleData::PropertySetMatcher::DoDeclaredPropertiesMatch(
    const scoped_refptr<const CSSComputedStyleData>& lhs,
    const scoped_refptr<const CSSComputedStyleData>& rhs) const {
  LonghandPropertiesBitset lhs_properties_bitset =
      lhs->declared_properties_ & properties_bitset_;
  LonghandPropertiesBitset rhs_properties_bitset =
      rhs->declared_properties_ & properties_bitset_;
  if (lhs_properties_bitset != rhs_properties_bitset) {
    return false;
  } else if (lhs_properties_bitset.none()) {
    return true;
  }
  for (size_t i = 0; i < properties_.size(); ++i) {
    PropertyKey property_key = properties_[i];
    if (lhs_properties_bitset[property_key] &&
        !lhs->declared_property_values_.find(property_key)
             ->second->Equals(
                 *rhs->declared_property_values_.find(property_key)->second)) {
      return false;
    }
  }
  return true;
}

CSSComputedStyleData::CSSComputedStyleData()
    : has_declared_inherited_properties_(false) {}

CSSComputedStyleData::~CSSComputedStyleData() {}

unsigned int CSSComputedStyleData::length() const {
  // Computed style declarations have all known longhand properties.
  return kMaxLonghandPropertyKey + 1;
}

const char* CSSComputedStyleData::Item(unsigned int index) const {
  if (index >= length()) return NULL;
  return GetPropertyName(GetLexicographicalLonghandPropertyKey(index));
}

const scoped_refptr<PropertyValue>&
CSSComputedStyleData::GetPropertyValueReference(PropertyKey key) const {
  DCHECK_GT(key, kNoneProperty);
  DCHECK_LE(key, kMaxLonghandPropertyKey);

  // If the property's value is explicitly declared, then simply return it.
  if (declared_properties_[key]) {
    return declared_property_values_.find(key)->second;
  }

  // Otherwise, if the property is inherited and the parent has inherited
  // properties, then retrieve the parent's value for the property.
  if (parent_computed_style_declaration_ &&
      GetPropertyInheritance(key) == kInheritedYes &&
      parent_computed_style_declaration_->HasInheritedProperties()) {
    return parent_computed_style_declaration_
        ->GetInheritedPropertyValueReference(key);
  }

  // For the root element, which has no parent element, the inherited value is
  // the initial value of the property.
  //   https://www.w3.org/TR/css-cascade-3/#inheriting
  return GetComputedInitialValue(key);
}

scoped_refptr<PropertyValue>&
CSSComputedStyleData::GetDeclaredPropertyValueReference(PropertyKey key) {
  DCHECK(declared_properties_[key]);
  return declared_property_values_.find(key)->second;
}

namespace {
struct NonTrivialStaticFields {
  NonTrivialStaticFields()
      : block_keyword_value(KeywordValue::GetBlock()),
        zero_length_value(new LengthValue(0.0f, kPixelsUnit)) {}

  const scoped_refptr<PropertyValue> block_keyword_value;
  const scoped_refptr<PropertyValue> zero_length_value;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonTrivialStaticFields);
};

base::LazyInstance<NonTrivialStaticFields> non_trivial_static_fields =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

const scoped_refptr<PropertyValue>&
CSSComputedStyleData::GetComputedInitialValue(PropertyKey key) const {
  switch (key) {
    case kBorderTopColorProperty:
    case kBorderRightColorProperty:
    case kBorderBottomColorProperty:
    case kBorderLeftColorProperty:
    case kTextDecorationColorProperty:
      // Note that border color and text decoration color are not inherited.
      // The initial value of border color is 'currentColor' which means the
      // border color is the same as the value of the 'color' property.
      //    https://www.w3.org/TR/css3-color/#currentcolor
      return color();

    case kBorderTopWidthProperty:
    case kBorderRightWidthProperty:
    case kBorderBottomWidthProperty:
    case kBorderLeftWidthProperty:
      // If the border style is 'none' or 'hidden', border width would be 0.
      //   https://www.w3.org/TR/css3-background/#border-width
      if (IsBorderStyleNoneOrHiddenForAnEdge(key)) {
        return non_trivial_static_fields.Get().zero_length_value;
      }
      break;

    case kDisplayProperty:
      // The initial value of "display" (inline) become "block" if "position" is
      // "absolute" or "fixed".
      //    https://www.w3.org/TR/CSS21/visuren.html#dis-pos-flo
      if (position() == KeywordValue::GetAbsolute() ||
          position() == KeywordValue::GetFixed()) {
        return non_trivial_static_fields.Get().block_keyword_value;
      }
      break;

    case kAllProperty:
    case kAnimationDelayProperty:
    case kAnimationDirectionProperty:
    case kAnimationDurationProperty:
    case kAnimationFillModeProperty:
    case kAnimationIterationCountProperty:
    case kAnimationNameProperty:
    case kAnimationProperty:
    case kAnimationTimingFunctionProperty:
    case kBackgroundColorProperty:
    case kBackgroundImageProperty:
    case kBackgroundPositionProperty:
    case kBackgroundProperty:
    case kBackgroundRepeatProperty:
    case kBackgroundSizeProperty:
    case kBorderBottomStyleProperty:
    case kBorderBottomProperty:
    case kBorderColorProperty:
    case kBorderLeftProperty:
    case kBorderLeftStyleProperty:
    case kBorderProperty:
    case kBorderRightProperty:
    case kBorderRightStyleProperty:
    case kBorderStyleProperty:
    case kBorderTopProperty:
    case kBorderTopStyleProperty:
    case kBorderWidthProperty:
    case kBorderRadiusProperty:
    case kBottomProperty:
    case kBoxShadowProperty:
    case kColorProperty:
    case kContentProperty:
    case kFontFamilyProperty:
    case kFontProperty:
    case kFontStyleProperty:
    case kFontWeightProperty:
    case kFontSizeProperty:
    case kHeightProperty:
    case kLeftProperty:
    case kLineHeightProperty:
    case kMarginBottomProperty:
    case kMarginLeftProperty:
    case kMarginProperty:
    case kMarginRightProperty:
    case kMarginTopProperty:
    case kMaxHeightProperty:
    case kMaxWidthProperty:
    case kMinHeightProperty:
    case kMinWidthProperty:
    case kNoneProperty:
    case kOpacityProperty:
    case kOverflowProperty:
    case kOverflowWrapProperty:
    case kPaddingBottomProperty:
    case kPaddingLeftProperty:
    case kPaddingProperty:
    case kPaddingRightProperty:
    case kPaddingTopProperty:
    case kPositionProperty:
    case kRightProperty:
    case kSrcProperty:
    case kTextAlignProperty:
    case kTextDecorationLineProperty:
    case kTextDecorationProperty:
    case kTextIndentProperty:
    case kTextOverflowProperty:
    case kTextShadowProperty:
    case kTextTransformProperty:
    case kTopProperty:
    case kTransformOriginProperty:
    case kTransformProperty:
    case kTransitionDelayProperty:
    case kTransitionDurationProperty:
    case kTransitionProperty:
    case kTransitionPropertyProperty:
    case kTransitionTimingFunctionProperty:
    case kUnicodeRangeProperty:
    case kVerticalAlignProperty:
    case kVisibilityProperty:
    case kWhiteSpaceProperty:
    case kWidthProperty:
    case kWordWrapProperty:
    case kZIndexProperty:
      break;
  }

  return GetPropertyInitialValue(key);
}

bool CSSComputedStyleData::IsBorderStyleNoneOrHiddenForAnEdge(
    PropertyKey key) const {
  scoped_refptr<PropertyValue> border_style;
  if (key == kBorderTopWidthProperty) {
    border_style = border_top_style();
  } else if (key == kBorderRightWidthProperty) {
    border_style = border_right_style();
  } else if (key == kBorderBottomWidthProperty) {
    border_style = border_bottom_style();
  } else {
    DCHECK_EQ(key, kBorderLeftWidthProperty);
    border_style = border_left_style();
  }

  if (border_style == KeywordValue::GetNone() ||
      border_style == KeywordValue::GetHidden()) {
    return true;
  }

  return false;
}

void CSSComputedStyleData::SetPropertyValue(
    const PropertyKey key, const scoped_refptr<PropertyValue>& value) {
  DCHECK_GT(key, kNoneProperty);
  DCHECK_LE(key, kMaxLonghandPropertyKey);

  if (value) {
    declared_properties_.set(key, true);
    declared_property_values_[key] = value;
    // Only set |has_declared_inherited_properties_| if the property is
    // inherited and the the value isn't explicitly set to "inherit". If it is
    // set to "inherit", then the value is simply a copy of the parent's value,
    // which doesn't necessitate the node being included in the inheritance
    // tree, as it doesn't provide new information.
    // NOTE: Declaring a value of "inherit" on an inherited property is used for
    // transitions, which need to know the original value of the property (which
    // would otherwise be lost when the parent changed).
    has_declared_inherited_properties_ =
        has_declared_inherited_properties_ ||
        (GetPropertyInheritance(key) == kInheritedYes &&
         value != KeywordValue::GetInherit());
  } else if (declared_properties_[key]) {
    declared_properties_.set(key, false);
    declared_property_values_.erase(key);
  }
}

void CSSComputedStyleData::AssignFrom(const CSSComputedStyleData& rhs) {
  declared_properties_ = rhs.declared_properties_;
  declared_property_values_ = rhs.declared_property_values_;
  has_declared_inherited_properties_ = rhs.has_declared_inherited_properties_;
  declared_properties_inherited_from_parent_ =
      rhs.declared_properties_inherited_from_parent_;
  parent_computed_style_declaration_ = rhs.parent_computed_style_declaration_;
}

std::string CSSComputedStyleData::SerializeCSSDeclarationBlock() const {
  // All longhand properties that are supported CSS properties, in
  // lexicographical order, with the value being the resolved value.
  //   https://www.w3.org/TR/2013/WD-cssom-20131205/#dom-window-getcomputedstyle

  // TODO: Return the resolved value instead of the computed value. See
  // https://www.w3.org/TR/2013/WD-cssom-20131205/#resolved-value.
  std::string serialized_text;
  for (size_t index = 0; index <= kMaxLonghandPropertyKey; ++index) {
    PropertyKey key = GetLexicographicalLonghandPropertyKey(index);
    if (!serialized_text.empty()) {
      serialized_text.push_back(' ');
    }
    serialized_text.append(GetPropertyName(key));
    serialized_text.append(": ");
    serialized_text.append(GetPropertyValue(key)->ToString());
    serialized_text.push_back(';');
  }
  return serialized_text;
}

void CSSComputedStyleData::AddDeclaredPropertyInheritedFromParent(
    PropertyKey key) {
  declared_properties_inherited_from_parent_.push_back(key);
}

bool CSSComputedStyleData::AreDeclaredPropertiesInheritedFromParentValid()
    const {
  // If there are no declared properties inherited from the parent, then it's
  // impossible for them to be invalid.
  if (declared_properties_inherited_from_parent_.size() == 0) {
    return true;
  }

  if (!parent_computed_style_declaration_) {
    return false;
  }

  const scoped_refptr<const CSSComputedStyleData>& parent_computed_style_data =
      parent_computed_style_declaration_->data();
  if (!parent_computed_style_data) {
    return false;
  }

  // Verify that the parent's data is valid.
  DCHECK(parent_computed_style_data
             ->AreDeclaredPropertiesInheritedFromParentValid());

  // Walk the declared properties inherited from the parent. They're invalid if
  // any no longer match the parent's value.
  for (PropertyKeyVector::const_iterator iter =
           declared_properties_inherited_from_parent_.begin();
       iter != declared_properties_inherited_from_parent_.end(); ++iter) {
    if (!GetPropertyValueReference(*iter)->Equals(
            *parent_computed_style_data->GetPropertyValueReference(*iter))) {
      return false;
    }
  }
  return true;
}

bool CSSComputedStyleData::DoDeclaredPropertiesMatch(
    const scoped_refptr<const CSSComputedStyleData>& other) const {
  // If the bitsets don't match, then there's no need to check the values;
  // the declared properties are guaranteed to not match.
  if (declared_properties_ != other->declared_properties_) {
    return false;
  }

  // Verify that the same number of declared property values exist within the
  // two CSSComputedStyleData objects. This should be guaranteed by the
  // bitsets matching.
  DCHECK_EQ(declared_property_values_.size(),
            other->declared_property_values_.size());

  // Walk the two lists of declared property values looking for any keys or
  // values that don't match.
  PropertyValues::const_iterator iter1 = declared_property_values_.begin();
  PropertyValues::const_iterator iter2 =
      other->declared_property_values_.begin();
  for (; iter1 != declared_property_values_.end(); ++iter1, ++iter2) {
    if (iter1->first != iter2->first ||
        !iter1->second->Equals(*iter2->second)) {
      return false;
    }
  }

  return true;
}

void CSSComputedStyleData::SetParentComputedStyleDeclaration(
    const scoped_refptr<CSSComputedStyleDeclaration>&
        parent_computed_style_declaration) {
  parent_computed_style_declaration_ = parent_computed_style_declaration;
}

const scoped_refptr<CSSComputedStyleDeclaration>&
CSSComputedStyleData::GetParentComputedStyleDeclaration() const {
  return parent_computed_style_declaration_;
}

}  // namespace cssom
}  // namespace cobalt
