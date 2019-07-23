// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSSOM_PROPERTY_DEFINITIONS_H_
#define COBALT_CSSOM_PROPERTY_DEFINITIONS_H_

#include <bitset>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/containers/small_map.h"
#include "base/memory/ref_counted.h"
#include "cobalt/cssom/property_value.h"
#include "url/gurl.h"

namespace cobalt {
namespace cssom {

// WARNING: When adding a new property, add a SetPropertyDefinition() entry in
// NonTrivialGlobalVariables(), and add an entry in GetPropertyKey(). The
// property also likely needs to be added to css_style_declaration.idl,
// CSSStyleDeclaration, and CSSComputedStyleData with a corresponding getter
// and setter implementation. Additionally, support may need to be added in the
// css_parser module including a test in parser_test.cc.

enum PropertyKey {
  kNoneProperty = -1,

  // All supported longhand properties are listed here.
  kAlignContentProperty,
  kAlignItemsProperty,
  kAlignSelfProperty,
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
  kBorderBottomLeftRadiusProperty,
  kBorderBottomRightRadiusProperty,
  kBorderBottomStyleProperty,
  kBorderBottomWidthProperty,
  kBorderLeftColorProperty,
  kBorderLeftStyleProperty,
  kBorderLeftWidthProperty,
  kBorderRightColorProperty,
  kBorderRightStyleProperty,
  kBorderRightWidthProperty,
  kBorderTopColorProperty,
  kBorderTopLeftRadiusProperty,
  kBorderTopRightRadiusProperty,
  kBorderTopStyleProperty,
  kBorderTopWidthProperty,
  kBottomProperty,
  kBoxShadowProperty,
  kColorProperty,
  kContentProperty,
  kDisplayProperty,
  kFilterProperty,
  kFlexBasisProperty,
  kFlexDirectionProperty,
  kFlexGrowProperty,
  kFlexShrinkProperty,
  kFlexWrapProperty,
  kFontFamilyProperty,
  kFontSizeProperty,
  kFontStyleProperty,
  kFontWeightProperty,
  kHeightProperty,
  kIntersectionObserverRootMarginProperty,
  kJustifyContentProperty,
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
  kOrderProperty,
  kOutlineColorProperty,
  kOutlineStyleProperty,
  kOutlineWidthProperty,
  kOverflowProperty,
  kOverflowWrapProperty,
  kPaddingBottomProperty,
  kPaddingLeftProperty,
  kPaddingRightProperty,
  kPaddingTopProperty,
  kPointerEventsProperty,
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

  // All other supported properties, such as shorthand properties or
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
  kBorderRadiusProperty,
  kBorderRightProperty,
  kBorderStyleProperty,
  kBorderTopProperty,
  kBorderWidthProperty,
  kFlexProperty,
  kFlexFlowProperty,
  kFontProperty,
  kMarginProperty,
  kOutlineProperty,
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

// Any property that is referenced when calculating the computed property values
// of children should have this set to true.
// NOTE: This currently occurs within
// CalculateComputedStyleContext::HandleSpecifiedValue.
enum ImpactsChildComputedStyle {
  kImpactsChildComputedStyleNo,
  kImpactsChildComputedStyleYes,
};

// Any property that is referenced during box generation should have this set to
// true.
// NOTE: This currently occurs within BoxGenerator.
enum ImpactsBoxGeneration {
  kImpactsBoxGenerationNo,
  kImpactsBoxGenerationYes,
};

// Any property that is referenced when updating the size of boxes should have
// this set to true.
// NOTE: This currently occurs within Box::UpdateSize().
enum ImpactsBoxSizes {
  kImpactsBoxSizesNo,
  kImpactsBoxSizesYes,
};

// Any property that is referenced when generating cross references should have
// this set to true.
// NOTE: This currently occurs within ContainerBox::UpdateCrossReferences().
enum ImpactsBoxCrossReferences {
  kImpactsBoxCrossReferencesNo,
  kImpactsBoxCrossReferencesYes,
};

// NOTE: The array size of SmallMap and the decision to use std::map as the
// underlying container type are based on extensive performance testing. Do not
// change these unless additional profiling data justifies it.
typedef base::small_map<std::map<PropertyKey, GURL>, 1> GURLMap;

const char* GetPropertyName(PropertyKey key);

const scoped_refptr<PropertyValue>& GetPropertyInitialValue(PropertyKey key);

Inherited GetPropertyInheritance(PropertyKey key);
Animatable GetPropertyAnimatable(PropertyKey key);
ImpactsChildComputedStyle GetPropertyImpactsChildComputedStyle(PropertyKey key);
ImpactsBoxGeneration GetPropertyImpactsBoxGeneration(PropertyKey key);
ImpactsBoxSizes GetPropertyImpactsBoxSizes(PropertyKey key);
ImpactsBoxCrossReferences GetPropertyImpactsBoxCrossReferences(PropertyKey key);

typedef std::vector<PropertyKey> PropertyKeyVector;
const PropertyKeyVector& GetAnimatableProperties();
const PropertyKeyVector& GetInheritedAnimatableProperties();

PropertyKey GetLexicographicalLonghandPropertyKey(const size_t index);

PropertyKey GetPropertyKey(const std::string& property_name);

bool IsShorthandProperty(PropertyKey key);

// While we don't need LonghandPropertySet to be sorted, we use std::set here
// instead of base::hash_set because Linux gives a compiler error when iterating
// over hash_set values of enum type.
typedef std::set<PropertyKey> LonghandPropertySet;
const LonghandPropertySet& ExpandShorthandProperty(PropertyKey key);

typedef std::bitset<kNumLonghandProperties> LonghandPropertiesBitset;

}  // namespace cssom
}  // namespace cobalt

// Make PropertyKey usable as key in base::hash_map.

namespace BASE_HASH_NAMESPACE {

//
// GCC-flavored hash functor.
//
#if defined(BASE_HASH_USE_HASH_STRUCT)

// Forward declaration in case <hash_fun.h> is not #include'd.
template <>
struct hash<cobalt::cssom::PropertyKey>;

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
