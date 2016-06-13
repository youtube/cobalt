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

#ifndef COBALT_CSSOM_PROPERTY_DEFINITIONS_H_
#define COBALT_CSSOM_PROPERTY_DEFINITIONS_H_

#include <bitset>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/containers/small_map.h"
#include "base/hash_tables.h"
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
  kTextDecorationColorProperty,
  kTextDecorationLineProperty,
  kTextIndentProperty,
  kTextOverflowProperty,
  kTextShadowProperty,
  kTextTransformProperty,
  kTopProperty,
  kTransformOriginProperty,
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
  kTextDecorationProperty,
  kTransitionProperty,
  kMaxShorthandPropertyKey = kTransitionProperty,

  kMaxEveryPropertyKey = kMaxShorthandPropertyKey,
  kNumLonghandProperties = kMaxLonghandPropertyKey + 1,
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

PropertyKey GetLexicographicalLonghandPropertyKey(const size_t index);

PropertyKey GetPropertyKey(const std::string& property_name);

bool IsShorthandProperty(PropertyKey key);

// While we don't need LonghandPropertySet to be sorted, we use std::set here
// instead of base::hash_set because Linux gives a compiler error when iterating
// over hash_set values of enum type.
typedef std::set<PropertyKey> LonghandPropertySet;
const LonghandPropertySet& ExpandShorthandProperty(PropertyKey key);

typedef std::bitset<kNumLonghandProperties> LonghandPropertiesBitset;
typedef std::vector<PropertyKey> PropertyKeyVector;

}  // namespace cssom
}  // namespace cobalt

// Make PropertyKey usable as key in base::hash_map.

namespace BASE_HASH_NAMESPACE {

//
// GCC-flavored hash functor.
//
#if defined(BASE_HASH_USE_HASH_STRUCT)

// Forward declaration in case <hash_fun.h> is not #include'd.
template <typename Key>
struct hash;

template <>
struct hash<cobalt::cssom::PropertyKey> {
  size_t operator()(const cobalt::cssom::PropertyKey& key) const {
    return base_hash(key);
  }

  hash<intptr_t> base_hash;
};

//
// Dinkumware-flavored hash functor.
//
#else

// Forward declaration in case <xhash> is not #include'd.
template <typename Key, typename Predicate>
class hash_compare;

template <typename Predicate>
class hash_compare<cobalt::cssom::PropertyKey, Predicate> {
 public:
  typedef hash_compare<intptr_t> BaseHashCompare;

  enum {
    bucket_size = BaseHashCompare::bucket_size,
#if !defined(COMPILER_MSVC)
    min_buckets = BaseHashCompare::min_buckets,
#endif
  };

  hash_compare() {}

  size_t operator()(const cobalt::cssom::PropertyKey& key) const {
    return base_hash_compare_(key);
  }

  bool operator()(const cobalt::cssom::PropertyKey& lhs,
                  const cobalt::cssom::PropertyKey& rhs) const {
    return base_hash_compare_(lhs, rhs);
  }

 private:
  BaseHashCompare base_hash_compare_;
};

#endif
}  // namespace BASE_HASH_NAMESPACE

#endif  // COBALT_CSSOM_PROPERTY_DEFINITIONS_H_
