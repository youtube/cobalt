/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef CSSOM_PROPERTY_DEFINITIONS_H_
#define CSSOM_PROPERTY_DEFINITIONS_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/containers/small_map.h"
#include "base/memory/ref_counted.h"
#include "cobalt/cssom/property_value.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace cssom {

// WARNING: When adding a new property, add a SetPropertyDefinition() entry in
// NonTrivialGlobalVariables(), and add an entry in GetPropertyKey().
// The property may also need to be added to a Web API idl file, with a
// corresponding getter and setter implementation.
// Additionally, support may need to be added in the css_parser module.

enum PropertyKey {
  kNoneProperty = -1,

  // All supported longhand property values are listed here.
  kAnimationDelayProperty,
  kAnimationDirectionProperty,
  kAnimationDurationProperty,
  kAnimationFillModeProperty,
  kAnimationIterationCountProperty,
  kAnimationNameProperty,
  kAnimationTimingFunctionProperty,
  kBackgroundColorProperty,
  kBackgroundImageProperty,
  kBackgroundPositionProperty,
  kBackgroundRepeatProperty,
  kBackgroundSizeProperty,
  kBorderBottomColorProperty,
  kBorderBottomStyleProperty,
  kBorderBottomWidthProperty,
  kBorderLeftColorProperty,
  kBorderLeftStyleProperty,
  kBorderLeftWidthProperty,
  kBorderRadiusProperty,
  kBorderRightColorProperty,
  kBorderRightStyleProperty,
  kBorderRightWidthProperty,
  kBorderTopColorProperty,
  kBorderTopStyleProperty,
  kBorderTopWidthProperty,
  kBottomProperty,
  kBoxShadowProperty,
  kColorProperty,
  kContentProperty,
  kDisplayProperty,
  kFontFamilyProperty,
  kFontSizeProperty,
  kFontStyleProperty,
  kFontWeightProperty,
  kHeightProperty,
  kLeftProperty,
  kLineHeightProperty,
  kMarginBottomProperty,
  kMarginLeftProperty,
  kMarginRightProperty,
  kMarginTopProperty,
  kMaxHeightProperty,
  kMaxWidthProperty,
  kMinHeightProperty,
  kMinWidthProperty,
  kOpacityProperty,
  kOverflowProperty,
  kOverflowWrapProperty,
  kPaddingBottomProperty,
  kPaddingLeftProperty,
  kPaddingRightProperty,
  kPaddingTopProperty,
  kPositionProperty,
  kRightProperty,
  kTextAlignProperty,
  kTextIndentProperty,
  kTextOverflowProperty,
  kTextShadowProperty,
  kTextTransformProperty,
  kTopProperty,
  kTransformProperty,
  kTransitionDelayProperty,
  kTransitionDurationProperty,
  kTransitionPropertyProperty,
  kTransitionTimingFunctionProperty,
  kVerticalAlignProperty,
  kVisibilityProperty,
  kWhiteSpaceProperty,
  kWidthProperty,
  kZIndexProperty,
  kMaxLonghandPropertyKey = kZIndexProperty,

  // All other supported property values, such as shorthand property values or
  // aliases are listed here.
  kAllProperty,
  kSrcProperty,           // property for @font-face at-rule
  kUnicodeRangeProperty,  // property for @font-face at-rule
  kWordWrapProperty,      // alias for kOverflowWrap

  // Shorthand properties
  kFirstShorthandPropertyKey,
  kAnimationProperty = kFirstShorthandPropertyKey,
  kBackgroundProperty,
  kBorderBottomProperty,
  kBorderColorProperty,
  kBorderLeftProperty,
  kBorderProperty,
  kBorderRightProperty,
  kBorderStyleProperty,
  kBorderTopProperty,
  kBorderWidthProperty,
  kFontProperty,
  kMarginProperty,
  kPaddingProperty,
  kTransitionProperty,
  kMaxShorthandPropertyKey = kTransitionProperty,

  kMaxEveryPropertyKey = kMaxShorthandPropertyKey,
};

enum Inherited {
  kInheritedNo,
  kInheritedYes,
};

enum Animatable {
  kAnimatableNo,
  kAnimatableYes,
};

// NOTE: The array size of SmallMap and the decision to use std::map as the
// underlying container type are based on extensive performance testing with
// ***REMOVED***. Do not change these unless additional profiling data justifies it.
typedef base::SmallMap<std::map<PropertyKey, GURL>, 1> GURLMap;

const char* GetPropertyName(PropertyKey key);

const scoped_refptr<PropertyValue>& GetPropertyInitialValue(PropertyKey key);

Inherited GetPropertyInheritance(PropertyKey key);

Animatable GetPropertyAnimatable(PropertyKey key);

typedef std::vector<PropertyKey> AnimatablePropertyList;
const AnimatablePropertyList& GetAnimatableProperties();

PropertyKey GetPropertyKey(const std::string& property_name);

bool IsShorthandProperty(PropertyKey key);

// While we don't need LonghandPropertySet to be sorted, we use std::set here
// instead of base::hash_set because Linux gives a compiler error when iterating
// over hash_set values of enum type.
typedef std::set<PropertyKey> LonghandPropertySet;
const LonghandPropertySet& ExpandShorthandProperty(PropertyKey key);

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_PROPERTY_DEFINITIONS_H_
