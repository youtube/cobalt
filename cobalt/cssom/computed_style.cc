// Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/cssom/computed_style.h"

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/cssom/absolute_url_value.h"
#include "cobalt/cssom/calc_value.h"
#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/cssom/css_computed_style_declaration.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/linear_gradient_value.h"
#include "cobalt/cssom/matrix_function.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/property_value_visitor.h"
#include "cobalt/cssom/radial_gradient_value.h"
#include "cobalt/cssom/rotate_function.h"
#include "cobalt/cssom/scale_function.h"
#include "cobalt/cssom/shadow_value.h"
#include "cobalt/cssom/transform_function.h"
#include "cobalt/cssom/transform_function_list_value.h"
#include "cobalt/cssom/transform_function_visitor.h"
#include "cobalt/cssom/translate_function.h"
#include "cobalt/cssom/url_value.h"

namespace cobalt {
namespace cssom {

namespace {

scoped_refptr<LengthValue> ProvideAbsoluteLength(
    const scoped_refptr<LengthValue>& specified_length,
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size,
    const math::Size& viewport_size) {
  switch (specified_length->unit()) {
    // "px" is an absolute unit.
    //   https://www.w3.org/TR/css3-values/#absolute-lengths
    case kPixelsUnit:
      return scoped_refptr<LengthValue>(specified_length);

    // "em" equals to the computed value of the "font-size" property of
    // the element on which it is used.
    //   https://www.w3.org/TR/css3-values/#font-relative-lengths
    case kFontSizesAkaEmUnit: {
      DCHECK_EQ(kPixelsUnit, computed_font_size->unit());

      return new LengthValue(
          computed_font_size->value() * specified_length->value(), kPixelsUnit);
    }

    // "rem" equals to the computed value of font-size on the root element.
    //   https://www.w3.org/TR/css3-values/#font-relative-lengths
    case kRootElementFontSizesAkaRemUnit: {
      DCHECK_EQ(kPixelsUnit, root_computed_font_size->unit());

      return new LengthValue(
          root_computed_font_size->value() * specified_length->value(),
          kPixelsUnit);
    }

    // "vw" equal to 1% of the width of the initial containing block.
    //   https://www.w3.org/TR/css3-values/#viewport-relative-lengths
    case kViewportWidthPercentsAkaVwUnit: {
      return new LengthValue(
          viewport_size.width() * specified_length->value() / 100.0f,
          kPixelsUnit);
    }

    // "vh" equal to 1% of the height of the initial containing block.
    //   https://www.w3.org/TR/css3-values/#viewport-relative-lengths
    case kViewportHeightPercentsAkaVhUnit: {
      return new LengthValue(
          viewport_size.height() * specified_length->value() / 100.0f,
          kPixelsUnit);
    }
  }
  NOTREACHED();
  return NULL;
}

// For values that can be either lengths or percentages, this function will
// return a value that, if the original value was a length, is now
// absolutized.
scoped_refptr<PropertyValue> ProvideAbsoluteLengthIfNonNullLength(
    const scoped_refptr<PropertyValue>& specified_value,
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size,
    const math::Size& viewport_size) {
  if (!specified_value) {
    return specified_value;
  }

  if (specified_value->GetTypeId() == base::GetTypeId<LengthValue>()) {
    LengthValue* value_as_length =
        base::polymorphic_downcast<LengthValue*>(specified_value.get());

    return ProvideAbsoluteLength(value_as_length, computed_font_size,
                                 root_computed_font_size, viewport_size);
  } else {
    return specified_value;
  }
}

// Applices ProvideAbsoluteLengthIfNonNullLength() for every item in a
// PropertyValueList.
scoped_refptr<PropertyListValue> ProvideAbsoluteLengthsForNonNullLengthsInList(
    const scoped_refptr<PropertyListValue>& specified_value,
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size,
    const math::Size& viewport_size) {
  scoped_ptr<PropertyListValue::Builder> builder(
      new PropertyListValue::Builder());
  builder->reserve(specified_value->value().size());

  for (PropertyListValue::Builder::const_iterator iter =
           specified_value->value().begin();
       iter != specified_value->value().end(); ++iter) {
    builder->push_back(ProvideAbsoluteLengthIfNonNullLength(
        *iter, computed_font_size, root_computed_font_size, viewport_size));
  }

  return new PropertyListValue(builder.Pass());
}

// Computed value: absolute length;
//                 '0' if the border style is 'none' or 'hidden'.
//   https://www.w3.org/TR/css3-background/#border-width
class ComputedBorderWidthProvider : public NotReachedPropertyValueVisitor {
 public:
  ComputedBorderWidthProvider(const LengthValue* computed_font_size,
                              const LengthValue* root_computed_font_size,
                              const math::Size& viewport_size,
                              const PropertyValue* border_style);

  void VisitLength(LengthValue* length) override;

  const scoped_refptr<PropertyValue>& computed_border_width() const {
    return computed_border_width_;
  }

 private:
  const LengthValue* computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;
  const PropertyValue* border_style_;

  scoped_refptr<PropertyValue> computed_border_width_;

  DISALLOW_COPY_AND_ASSIGN(ComputedBorderWidthProvider);
};

ComputedBorderWidthProvider::ComputedBorderWidthProvider(
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size, const math::Size& viewport_size,
    const PropertyValue* border_style)
    : computed_font_size_(computed_font_size),
      root_computed_font_size_(root_computed_font_size),
      viewport_size_(viewport_size),
      border_style_(border_style) {}

void ComputedBorderWidthProvider::VisitLength(LengthValue* specified_length) {
  if (border_style_ == KeywordValue::GetNone().get() ||
      border_style_ == KeywordValue::GetHidden().get()) {
    computed_border_width_ = new LengthValue(0, kPixelsUnit);
  } else {
    DCHECK_EQ(border_style_, KeywordValue::GetSolid().get());
    computed_border_width_ =
        ProvideAbsoluteLength(specified_length, computed_font_size_,
                              root_computed_font_size_, viewport_size_);
  }
}

// Computed value: numeric weight value.
//   https://www.w3.org/TR/css3-fonts/#font-weight-prop
class ComputedFontWeightProvider : public NotReachedPropertyValueVisitor {
 public:
  ComputedFontWeightProvider() {}

  void VisitFontWeight(FontWeightValue* weight) override;

  const scoped_refptr<FontWeightValue>& computed_font_weight() const {
    return computed_font_weight_;
  }

 private:
  scoped_refptr<FontWeightValue> computed_font_weight_;

  DISALLOW_COPY_AND_ASSIGN(ComputedFontWeightProvider);
};

// TODO: Support bolder and lighter. Match the weight with font face.
// Quite often there are only a few weights available for a particular font
// family. When a weight is specified for which no face exists, a face with a
// nearby weight is used.
//   https://www.w3.org/TR/css3-fonts/#font-matching-algorithm
void ComputedFontWeightProvider::VisitFontWeight(
    FontWeightValue* specified_weight) {
  computed_font_weight_ = specified_weight;
}

// Computed value: absolute length.
//   https://www.w3.org/TR/css3-fonts/#font-size-prop
class ComputedFontSizeProvider : public NotReachedPropertyValueVisitor {
 public:
  ComputedFontSizeProvider(const LengthValue* parent_computed_font_size,
                           const LengthValue* root_computed_font_size,
                           const math::Size& viewport_size);

  void VisitLength(LengthValue* length) override;
  void VisitPercentage(PercentageValue* percentage) override;

  const scoped_refptr<LengthValue>& computed_font_size() const {
    return computed_font_size_;
  }

 private:
  const LengthValue* parent_computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;

  scoped_refptr<LengthValue> computed_font_size_;

  DISALLOW_COPY_AND_ASSIGN(ComputedFontSizeProvider);
};

ComputedFontSizeProvider::ComputedFontSizeProvider(
    const LengthValue* parent_computed_font_size,
    const LengthValue* root_computed_font_size, const math::Size& viewport_size)
    : parent_computed_font_size_(parent_computed_font_size),
      root_computed_font_size_(root_computed_font_size),
      viewport_size_(viewport_size) {}

void ComputedFontSizeProvider::VisitLength(LengthValue* specified_length) {
  // "em" on "font-size" is calculated relatively to the inherited value
  // of "font-size".
  //   https://www.w3.org/TR/css3-values/#font-relative-lengths
  computed_font_size_ =
      ProvideAbsoluteLength(specified_length, parent_computed_font_size_,
                            root_computed_font_size_, viewport_size_);
}

void ComputedFontSizeProvider::VisitPercentage(
    PercentageValue* specified_percentage) {
  // A percentage value specifies an absolute font size relative to the parent
  // element's fonts size.
  //   https://www.w3.org/TR/css3-fonts/#percentage-size-value
  computed_font_size_ = new LengthValue(
      parent_computed_font_size_->value() * specified_percentage->value(),
      kPixelsUnit);
}

// Computed value: for length and percentage the absolute value;
//                 otherwise as specified.
//   https://www.w3.org/TR/CSS21/visudet.html#line-height
class ComputedLineHeightProvider : public NotReachedPropertyValueVisitor {
 public:
  ComputedLineHeightProvider(const LengthValue* computed_font_size,
                             const LengthValue* root_computed_font_size,
                             const math::Size& viewport_size);

  void VisitKeyword(KeywordValue* keyword) override;
  void VisitLength(LengthValue* length) override;
  void VisitNumber(NumberValue* number) override;
  void VisitPercentage(PercentageValue* percentage) override;

  const scoped_refptr<PropertyValue>& computed_line_height() const {
    return computed_line_height_;
  }

 private:
  const LengthValue* computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;

  scoped_refptr<PropertyValue> computed_line_height_;

  DISALLOW_COPY_AND_ASSIGN(ComputedLineHeightProvider);
};

ComputedLineHeightProvider::ComputedLineHeightProvider(
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size, const math::Size& viewport_size)
    : computed_font_size_(computed_font_size),
      root_computed_font_size_(root_computed_font_size),
      viewport_size_(viewport_size) {}

void ComputedLineHeightProvider::VisitLength(LengthValue* specified_length) {
  computed_line_height_ =
      ProvideAbsoluteLength(specified_length, computed_font_size_,
                            root_computed_font_size_, viewport_size_);
}

void ComputedLineHeightProvider::VisitNumber(NumberValue* specified_number) {
  // The computed value is the same as the specified value.
  //   https://www.w3.org/TR/CSS2/visudet.html#line-height
  computed_line_height_ = specified_number;
}

void ComputedLineHeightProvider::VisitPercentage(PercentageValue* percentage) {
  // The computed value of the property is this percentage multiplied by the
  // element's computed font size. Negative values are illegal.
  //   https://www.w3.org/TR/CSS21/visudet.html#line-height
  computed_line_height_ = new LengthValue(
      computed_font_size_->value() * percentage->value(), kPixelsUnit);
}

void ComputedLineHeightProvider::VisitKeyword(KeywordValue* keyword) {
  switch (keyword->value()) {
    case KeywordValue::kNormal:
      computed_line_height_ = keyword;
      break;

    case KeywordValue::kAbsolute:
    case KeywordValue::kAlternate:
    case KeywordValue::kAlternateReverse:
    case KeywordValue::kAuto:
    case KeywordValue::kBackwards:
    case KeywordValue::kBaseline:
    case KeywordValue::kBlock:
    case KeywordValue::kBoth:
    case KeywordValue::kBottom:
    case KeywordValue::kBreakWord:
    case KeywordValue::kCenter:
    case KeywordValue::kClip:
    case KeywordValue::kContain:
    case KeywordValue::kCover:
    case KeywordValue::kCurrentColor:
    case KeywordValue::kCursive:
    case KeywordValue::kEllipsis:
    case KeywordValue::kEnd:
    case KeywordValue::kEquirectangular:
    case KeywordValue::kFantasy:
    case KeywordValue::kForwards:
    case KeywordValue::kFixed:
    case KeywordValue::kHidden:
    case KeywordValue::kInfinite:
    case KeywordValue::kInherit:
    case KeywordValue::kInitial:
    case KeywordValue::kInline:
    case KeywordValue::kInlineBlock:
    case KeywordValue::kLeft:
    case KeywordValue::kLineThrough:
    case KeywordValue::kMiddle:
    case KeywordValue::kMonoscopic:
    case KeywordValue::kMonospace:
    case KeywordValue::kNone:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kPreLine:
    case KeywordValue::kPreWrap:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStart:
    case KeywordValue::kStatic:
    case KeywordValue::kStereoscopicLeftRight:
    case KeywordValue::kStereoscopicTopBottom:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
      NOTREACHED();
  }
}

// Computed value: the percentage as specified or the absolute length.
//   https://www.w3.org/TR/CSS21/box.html#margin-properties
//   https://www.w3.org/TR/CSS21/box.html#padding-properties
class ComputedMarginOrPaddingEdgeProvider
    : public NotReachedPropertyValueVisitor {
 public:
  ComputedMarginOrPaddingEdgeProvider(
      const LengthValue* computed_font_size,
      const LengthValue* root_computed_font_size,
      const math::Size& viewport_size);

  void VisitKeyword(KeywordValue* keyword) override;
  void VisitLength(LengthValue* length) override;
  void VisitPercentage(PercentageValue* percentage) override;

  const scoped_refptr<PropertyValue>& computed_margin_or_padding_edge() const {
    return computed_margin_or_padding_edge_;
  }

 private:
  const LengthValue* computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;

  scoped_refptr<PropertyValue> computed_margin_or_padding_edge_;

  DISALLOW_COPY_AND_ASSIGN(ComputedMarginOrPaddingEdgeProvider);
};

ComputedMarginOrPaddingEdgeProvider::ComputedMarginOrPaddingEdgeProvider(
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size, const math::Size& viewport_size)
    : computed_font_size_(computed_font_size),
      root_computed_font_size_(root_computed_font_size),
      viewport_size_(viewport_size) {}

void ComputedMarginOrPaddingEdgeProvider::VisitKeyword(KeywordValue* keyword) {
  switch (keyword->value()) {
    case KeywordValue::kAuto:
      computed_margin_or_padding_edge_ = keyword;
      break;

    case KeywordValue::kAbsolute:
    case KeywordValue::kAlternate:
    case KeywordValue::kAlternateReverse:
    case KeywordValue::kBackwards:
    case KeywordValue::kBaseline:
    case KeywordValue::kBlock:
    case KeywordValue::kBoth:
    case KeywordValue::kBottom:
    case KeywordValue::kBreakWord:
    case KeywordValue::kCenter:
    case KeywordValue::kClip:
    case KeywordValue::kContain:
    case KeywordValue::kCover:
    case KeywordValue::kCurrentColor:
    case KeywordValue::kCursive:
    case KeywordValue::kEllipsis:
    case KeywordValue::kEnd:
    case KeywordValue::kEquirectangular:
    case KeywordValue::kFantasy:
    case KeywordValue::kFixed:
    case KeywordValue::kForwards:
    case KeywordValue::kHidden:
    case KeywordValue::kInfinite:
    case KeywordValue::kInherit:
    case KeywordValue::kInitial:
    case KeywordValue::kInline:
    case KeywordValue::kInlineBlock:
    case KeywordValue::kLeft:
    case KeywordValue::kLineThrough:
    case KeywordValue::kMiddle:
    case KeywordValue::kMonoscopic:
    case KeywordValue::kMonospace:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNone:
    case KeywordValue::kNormal:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kPreLine:
    case KeywordValue::kPreWrap:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStart:
    case KeywordValue::kStatic:
    case KeywordValue::kStereoscopicLeftRight:
    case KeywordValue::kStereoscopicTopBottom:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
      NOTREACHED();
  }
}

void ComputedMarginOrPaddingEdgeProvider::VisitLength(
    LengthValue* specified_length) {
  computed_margin_or_padding_edge_ =
      ProvideAbsoluteLength(specified_length, computed_font_size_,
                            root_computed_font_size_, viewport_size_);
}

void ComputedMarginOrPaddingEdgeProvider::VisitPercentage(
    PercentageValue* percentage) {
  computed_margin_or_padding_edge_ = percentage;
}

class ComputedPositionOffsetProvider : public NotReachedPropertyValueVisitor {
 public:
  ComputedPositionOffsetProvider(const LengthValue* computed_font_size,
                                 const LengthValue* root_computed_font_size,
                                 const math::Size& viewport_size);

  void VisitKeyword(KeywordValue* keyword) override;
  void VisitLength(LengthValue* length) override;
  void VisitPercentage(PercentageValue* percentage) override;

  const scoped_refptr<PropertyValue>& computed_position_offset() const {
    return computed_position_offset_;
  }

 private:
  const LengthValue* computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;

  scoped_refptr<PropertyValue> computed_position_offset_;

  DISALLOW_COPY_AND_ASSIGN(ComputedPositionOffsetProvider);
};

void ComputedPositionOffsetProvider::VisitLength(
    LengthValue* specified_length) {
  computed_position_offset_ =
      ProvideAbsoluteLength(specified_length, computed_font_size_,
                            root_computed_font_size_, viewport_size_);
}

void ComputedPositionOffsetProvider::VisitKeyword(KeywordValue* keyword) {
  switch (keyword->value()) {
    case KeywordValue::kAuto:
      computed_position_offset_ = keyword;
      break;

    case KeywordValue::kAbsolute:
    case KeywordValue::kAlternate:
    case KeywordValue::kAlternateReverse:
    case KeywordValue::kBackwards:
    case KeywordValue::kBaseline:
    case KeywordValue::kBlock:
    case KeywordValue::kBoth:
    case KeywordValue::kBottom:
    case KeywordValue::kBreakWord:
    case KeywordValue::kCenter:
    case KeywordValue::kClip:
    case KeywordValue::kContain:
    case KeywordValue::kCover:
    case KeywordValue::kCurrentColor:
    case KeywordValue::kCursive:
    case KeywordValue::kEllipsis:
    case KeywordValue::kEnd:
    case KeywordValue::kEquirectangular:
    case KeywordValue::kFantasy:
    case KeywordValue::kFixed:
    case KeywordValue::kForwards:
    case KeywordValue::kHidden:
    case KeywordValue::kInfinite:
    case KeywordValue::kInherit:
    case KeywordValue::kInitial:
    case KeywordValue::kInline:
    case KeywordValue::kInlineBlock:
    case KeywordValue::kLeft:
    case KeywordValue::kLineThrough:
    case KeywordValue::kMiddle:
    case KeywordValue::kMonoscopic:
    case KeywordValue::kMonospace:
    case KeywordValue::kNone:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNormal:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kPreLine:
    case KeywordValue::kPreWrap:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStart:
    case KeywordValue::kStatic:
    case KeywordValue::kStereoscopicLeftRight:
    case KeywordValue::kStereoscopicTopBottom:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
      NOTREACHED();
  }
}

void ComputedPositionOffsetProvider::VisitPercentage(
    PercentageValue* percentage) {
  computed_position_offset_ = percentage;
}

ComputedPositionOffsetProvider::ComputedPositionOffsetProvider(
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size, const math::Size& viewport_size)
    : computed_font_size_(computed_font_size),
      root_computed_font_size_(root_computed_font_size),
      viewport_size_(viewport_size) {}

// Computed value: the percentage or "auto" or the absolute length.
//   https://www.w3.org/TR/CSS21/visudet.html#the-height-property
class ComputedHeightProvider : public NotReachedPropertyValueVisitor {
 public:
  ComputedHeightProvider(const PropertyValue* parent_computed_height,
                         const PropertyValue* parent_computed_top,
                         const PropertyValue* parent_computed_bottom,
                         const LengthValue* computed_font_size,
                         const LengthValue* root_computed_font_size,
                         const math::Size& viewport_size, bool out_of_flow);

  void VisitKeyword(KeywordValue* keyword) override;
  void VisitLength(LengthValue* length) override;
  void VisitPercentage(PercentageValue* percentage) override;

  const scoped_refptr<PropertyValue>& computed_height() const {
    return computed_height_;
  }

 private:
  const PropertyValue* parent_computed_height_;
  const PropertyValue* parent_computed_top_;
  const PropertyValue* parent_computed_bottom_;
  const LengthValue* computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;
  bool out_of_flow_;

  scoped_refptr<PropertyValue> computed_height_;

  DISALLOW_COPY_AND_ASSIGN(ComputedHeightProvider);
};

ComputedHeightProvider::ComputedHeightProvider(
    const PropertyValue* parent_computed_height,
    const PropertyValue* parent_computed_top,
    const PropertyValue* parent_computed_bottom,
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size, const math::Size& viewport_size,
    bool out_of_flow)
    : parent_computed_height_(parent_computed_height),
      parent_computed_top_(parent_computed_top),
      parent_computed_bottom_(parent_computed_bottom),
      computed_font_size_(computed_font_size),
      root_computed_font_size_(root_computed_font_size),
      viewport_size_(viewport_size),
      out_of_flow_(out_of_flow) {}

void ComputedHeightProvider::VisitLength(LengthValue* specified_length) {
  computed_height_ =
      ProvideAbsoluteLength(specified_length, computed_font_size_,
                            root_computed_font_size_, viewport_size_);
}

void ComputedHeightProvider::VisitKeyword(KeywordValue* keyword) {
  switch (keyword->value()) {
    case KeywordValue::kAuto:
      computed_height_ = keyword;
      break;

    case KeywordValue::kAbsolute:
    case KeywordValue::kAlternate:
    case KeywordValue::kAlternateReverse:
    case KeywordValue::kBackwards:
    case KeywordValue::kBaseline:
    case KeywordValue::kBlock:
    case KeywordValue::kBoth:
    case KeywordValue::kBottom:
    case KeywordValue::kBreakWord:
    case KeywordValue::kCenter:
    case KeywordValue::kClip:
    case KeywordValue::kContain:
    case KeywordValue::kCover:
    case KeywordValue::kCurrentColor:
    case KeywordValue::kCursive:
    case KeywordValue::kEllipsis:
    case KeywordValue::kEnd:
    case KeywordValue::kEquirectangular:
    case KeywordValue::kFantasy:
    case KeywordValue::kFixed:
    case KeywordValue::kForwards:
    case KeywordValue::kHidden:
    case KeywordValue::kInfinite:
    case KeywordValue::kInherit:
    case KeywordValue::kInitial:
    case KeywordValue::kInline:
    case KeywordValue::kInlineBlock:
    case KeywordValue::kLeft:
    case KeywordValue::kLineThrough:
    case KeywordValue::kMiddle:
    case KeywordValue::kMonospace:
    case KeywordValue::kMonoscopic:
    case KeywordValue::kNone:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNormal:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kPreLine:
    case KeywordValue::kPreWrap:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStart:
    case KeywordValue::kStatic:
    case KeywordValue::kStereoscopicLeftRight:
    case KeywordValue::kStereoscopicTopBottom:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
      NOTREACHED();
  }
}

void ComputedHeightProvider::VisitPercentage(PercentageValue* percentage) {
  const scoped_refptr<PropertyValue>& auto_value = KeywordValue::GetAuto();

  // If the height of the containing block is not specified explicitly
  // (i.e., it depends on content height), and this element is not absolutely
  // positioned, the value computes to "auto".
  //   https://www.w3.org/TR/CSS21/visudet.html#the-height-property
  computed_height_ = (parent_computed_height_ == auto_value &&
                      (parent_computed_top_ == auto_value ||
                       parent_computed_bottom_ == auto_value) &&
                      !out_of_flow_)
                         ? auto_value
                         : percentage;
}

// Computed value: the percentage or "auto" or the absolute length.
//   https://www.w3.org/TR/CSS2/visudet.html#propdef-max-height
class ComputedMaxHeightProvider : public NotReachedPropertyValueVisitor {
 public:
  ComputedMaxHeightProvider(const PropertyValue* parent_computed_max_height,
                            const LengthValue* computed_font_size,
                            const LengthValue* root_computed_font_size,
                            const math::Size& viewport_size, bool out_of_flow);

  void VisitKeyword(KeywordValue* keyword) override;
  void VisitLength(LengthValue* length) override;
  void VisitPercentage(PercentageValue* percentage) override;

  const scoped_refptr<PropertyValue>& computed_max_height() const {
    return computed_max_height_;
  }

 private:
  const PropertyValue* parent_computed_max_height_;
  const LengthValue* computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;
  bool out_of_flow_;

  scoped_refptr<PropertyValue> computed_max_height_;

  DISALLOW_COPY_AND_ASSIGN(ComputedMaxHeightProvider);
};

ComputedMaxHeightProvider::ComputedMaxHeightProvider(
    const PropertyValue* parent_computed_max_height,
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size, const math::Size& viewport_size,
    bool out_of_flow)
    : parent_computed_max_height_(parent_computed_max_height),
      computed_font_size_(computed_font_size),
      root_computed_font_size_(root_computed_font_size),
      viewport_size_(viewport_size),
      out_of_flow_(out_of_flow) {}

void ComputedMaxHeightProvider::VisitLength(LengthValue* specified_length) {
  computed_max_height_ =
      ProvideAbsoluteLength(specified_length, computed_font_size_,
                            root_computed_font_size_, viewport_size_);
}

void ComputedMaxHeightProvider::VisitKeyword(KeywordValue* keyword) {
  switch (keyword->value()) {
    case KeywordValue::kAuto:
    case KeywordValue::kNone:
      computed_max_height_ = keyword;
      break;

    case KeywordValue::kAbsolute:
    case KeywordValue::kAlternate:
    case KeywordValue::kAlternateReverse:
    case KeywordValue::kBackwards:
    case KeywordValue::kBaseline:
    case KeywordValue::kBlock:
    case KeywordValue::kBoth:
    case KeywordValue::kBottom:
    case KeywordValue::kBreakWord:
    case KeywordValue::kCenter:
    case KeywordValue::kClip:
    case KeywordValue::kContain:
    case KeywordValue::kCover:
    case KeywordValue::kCursive:
    case KeywordValue::kCurrentColor:
    case KeywordValue::kEllipsis:
    case KeywordValue::kEnd:
    case KeywordValue::kEquirectangular:
    case KeywordValue::kFantasy:
    case KeywordValue::kFixed:
    case KeywordValue::kForwards:
    case KeywordValue::kHidden:
    case KeywordValue::kInfinite:
    case KeywordValue::kInherit:
    case KeywordValue::kInitial:
    case KeywordValue::kInline:
    case KeywordValue::kInlineBlock:
    case KeywordValue::kLeft:
    case KeywordValue::kLineThrough:
    case KeywordValue::kMiddle:
    case KeywordValue::kMonoscopic:
    case KeywordValue::kMonospace:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNormal:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kPreLine:
    case KeywordValue::kPreWrap:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStart:
    case KeywordValue::kStatic:
    case KeywordValue::kStereoscopicLeftRight:
    case KeywordValue::kStereoscopicTopBottom:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
      NOTREACHED();
  }
}

void ComputedMaxHeightProvider::VisitPercentage(PercentageValue* percentage) {
  const scoped_refptr<PropertyValue>& auto_value = KeywordValue::GetAuto();
  const scoped_refptr<PropertyValue>& none_value = KeywordValue::GetNone();

  // If the max_height of the containing block is not specified explicitly
  // (i.e., it depends on content max_height), and this element is not
  // absolutely positioned, the percentage value is treated as 'none'.
  //   https://www.w3.org/TR/CSS2/visudet.html#propdef-max-height
  computed_max_height_ =
      (parent_computed_max_height_ == auto_value && !out_of_flow_) ? none_value
                                                                   : percentage;
}

// Computed value: the percentage or "auto" or the absolute length.
//   https://www.w3.org/TR/CSS2/visudet.html#propdef-min-height
class ComputedMinHeightProvider : public NotReachedPropertyValueVisitor {
 public:
  ComputedMinHeightProvider(const PropertyValue* parent_computed_min_max_height,
                            const LengthValue* computed_font_size,
                            const LengthValue* root_computed_font_size,
                            const math::Size& viewport_size, bool out_of_flow);

  void VisitKeyword(KeywordValue* keyword) override;
  void VisitLength(LengthValue* length) override;
  void VisitPercentage(PercentageValue* percentage) override;

  const scoped_refptr<PropertyValue>& computed_min_height() const {
    return computed_min_height_;
  }

 private:
  const PropertyValue* parent_computed_min_max_height_;
  const LengthValue* computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;
  bool out_of_flow_;

  scoped_refptr<PropertyValue> computed_min_height_;

  DISALLOW_COPY_AND_ASSIGN(ComputedMinHeightProvider);
};

ComputedMinHeightProvider::ComputedMinHeightProvider(
    const PropertyValue* parent_computed_min_max_height,
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size, const math::Size& viewport_size,
    bool out_of_flow)
    : parent_computed_min_max_height_(parent_computed_min_max_height),
      computed_font_size_(computed_font_size),
      root_computed_font_size_(root_computed_font_size),
      viewport_size_(viewport_size),
      out_of_flow_(out_of_flow) {}

void ComputedMinHeightProvider::VisitLength(LengthValue* specified_length) {
  computed_min_height_ =
      ProvideAbsoluteLength(specified_length, computed_font_size_,
                            root_computed_font_size_, viewport_size_);
}

void ComputedMinHeightProvider::VisitKeyword(KeywordValue* keyword) {
  switch (keyword->value()) {
    case KeywordValue::kAuto:
      computed_min_height_ = keyword;
      break;

    case KeywordValue::kAbsolute:
    case KeywordValue::kAlternate:
    case KeywordValue::kAlternateReverse:
    case KeywordValue::kBackwards:
    case KeywordValue::kBaseline:
    case KeywordValue::kBlock:
    case KeywordValue::kBoth:
    case KeywordValue::kBottom:
    case KeywordValue::kBreakWord:
    case KeywordValue::kCenter:
    case KeywordValue::kClip:
    case KeywordValue::kContain:
    case KeywordValue::kCover:
    case KeywordValue::kCurrentColor:
    case KeywordValue::kCursive:
    case KeywordValue::kEllipsis:
    case KeywordValue::kEquirectangular:
    case KeywordValue::kEnd:
    case KeywordValue::kFantasy:
    case KeywordValue::kFixed:
    case KeywordValue::kForwards:
    case KeywordValue::kHidden:
    case KeywordValue::kInfinite:
    case KeywordValue::kInherit:
    case KeywordValue::kInitial:
    case KeywordValue::kInline:
    case KeywordValue::kInlineBlock:
    case KeywordValue::kLeft:
    case KeywordValue::kLineThrough:
    case KeywordValue::kMiddle:
    case KeywordValue::kMonoscopic:
    case KeywordValue::kMonospace:
    case KeywordValue::kNone:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNormal:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kPreLine:
    case KeywordValue::kPreWrap:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStart:
    case KeywordValue::kStatic:
    case KeywordValue::kStereoscopicLeftRight:
    case KeywordValue::kStereoscopicTopBottom:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
      NOTREACHED();
  }
}

void ComputedMinHeightProvider::VisitPercentage(PercentageValue* percentage) {
  const scoped_refptr<PropertyValue>& auto_value = KeywordValue::GetAuto();

  // If the min_height of the containing block is not specified explicitly
  // (i.e., it depends on content min_height), and this element is not
  // absolutely positioned, the percentage value is treated as '0'.
  //   https://www.w3.org/TR/CSS2/visudet.html#propdef-min-height
  if (parent_computed_min_max_height_ == auto_value && !out_of_flow_) {
    computed_min_height_ = new LengthValue(0, kPixelsUnit);
  } else {
    computed_min_height_ = percentage;
  }
}

// Computed value: the percentage or "auto" as specified or the absolute length.
//   https://www.w3.org/TR/CSS21/visudet.html#the-width-property
class ComputedWidthProvider : public NotReachedPropertyValueVisitor {
 public:
  ComputedWidthProvider(const LengthValue* computed_font_size,
                        const LengthValue* root_computed_font_size,
                        const math::Size& viewport_size);

  void VisitKeyword(KeywordValue* keyword) override;
  void VisitLength(LengthValue* length) override;
  void VisitPercentage(PercentageValue* percentage) override;

  const scoped_refptr<PropertyValue>& computed_width() const {
    return computed_width_;
  }

 private:
  const LengthValue* computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;

  scoped_refptr<PropertyValue> computed_width_;

  DISALLOW_COPY_AND_ASSIGN(ComputedWidthProvider);
};

ComputedWidthProvider::ComputedWidthProvider(
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size, const math::Size& viewport_size)
    : computed_font_size_(computed_font_size),
      root_computed_font_size_(root_computed_font_size),
      viewport_size_(viewport_size) {}

void ComputedWidthProvider::VisitLength(LengthValue* specified_length) {
  computed_width_ =
      ProvideAbsoluteLength(specified_length, computed_font_size_,
                            root_computed_font_size_, viewport_size_);
}

void ComputedWidthProvider::VisitKeyword(KeywordValue* keyword) {
  switch (keyword->value()) {
    case KeywordValue::kAuto:
      computed_width_ = keyword;
      break;

    case KeywordValue::kAbsolute:
    case KeywordValue::kAlternate:
    case KeywordValue::kAlternateReverse:
    case KeywordValue::kBackwards:
    case KeywordValue::kBaseline:
    case KeywordValue::kBlock:
    case KeywordValue::kBoth:
    case KeywordValue::kBottom:
    case KeywordValue::kBreakWord:
    case KeywordValue::kCenter:
    case KeywordValue::kClip:
    case KeywordValue::kContain:
    case KeywordValue::kCover:
    case KeywordValue::kCurrentColor:
    case KeywordValue::kCursive:
    case KeywordValue::kEllipsis:
    case KeywordValue::kEnd:
    case KeywordValue::kEquirectangular:
    case KeywordValue::kFantasy:
    case KeywordValue::kFixed:
    case KeywordValue::kForwards:
    case KeywordValue::kHidden:
    case KeywordValue::kInfinite:
    case KeywordValue::kInherit:
    case KeywordValue::kInitial:
    case KeywordValue::kInline:
    case KeywordValue::kInlineBlock:
    case KeywordValue::kLeft:
    case KeywordValue::kLineThrough:
    case KeywordValue::kMiddle:
    case KeywordValue::kMonoscopic:
    case KeywordValue::kMonospace:
    case KeywordValue::kNone:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNormal:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kPreLine:
    case KeywordValue::kPreWrap:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStart:
    case KeywordValue::kStatic:
    case KeywordValue::kStereoscopicLeftRight:
    case KeywordValue::kStereoscopicTopBottom:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
      NOTREACHED();
  }
}

void ComputedWidthProvider::VisitPercentage(PercentageValue* percentage) {
  computed_width_ = percentage;
}

// Computed value: the percentage or "auto" as specified or the absolute length.
//   https://www.w3.org/TR/CSS2/visudet.html#min-max-widths
class ComputedMinMaxWidthProvider : public NotReachedPropertyValueVisitor {
 public:
  ComputedMinMaxWidthProvider(PropertyValue* parent_computed_min_max_height,
                              const LengthValue* computed_font_size,
                              const LengthValue* root_computed_font_size,
                              const math::Size& viewport_size);

  void VisitKeyword(KeywordValue* keyword) override;
  void VisitLength(LengthValue* length) override;
  void VisitPercentage(PercentageValue* percentage) override;

  const scoped_refptr<PropertyValue>& computed_min_max_width() const {
    return computed_min_max_width_;
  }

 private:
  PropertyValue* parent_computed_min_max_width_;
  const LengthValue* computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;

  scoped_refptr<PropertyValue> computed_min_max_width_;

  DISALLOW_COPY_AND_ASSIGN(ComputedMinMaxWidthProvider);
};

ComputedMinMaxWidthProvider::ComputedMinMaxWidthProvider(
    PropertyValue* parent_computed_min_max_width,
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size, const math::Size& viewport_size)
    : parent_computed_min_max_width_(parent_computed_min_max_width),
      computed_font_size_(computed_font_size),
      root_computed_font_size_(root_computed_font_size),
      viewport_size_(viewport_size) {}

void ComputedMinMaxWidthProvider::VisitLength(LengthValue* specified_length) {
  computed_min_max_width_ =
      ProvideAbsoluteLength(specified_length, computed_font_size_,
                            root_computed_font_size_, viewport_size_);
}

void ComputedMinMaxWidthProvider::VisitKeyword(KeywordValue* keyword) {
  switch (keyword->value()) {
    case KeywordValue::kAuto:
    case KeywordValue::kNone:
      computed_min_max_width_ = keyword;
      break;

    case KeywordValue::kAbsolute:
    case KeywordValue::kAlternate:
    case KeywordValue::kAlternateReverse:
    case KeywordValue::kBackwards:
    case KeywordValue::kBaseline:
    case KeywordValue::kBlock:
    case KeywordValue::kBoth:
    case KeywordValue::kBottom:
    case KeywordValue::kBreakWord:
    case KeywordValue::kCenter:
    case KeywordValue::kClip:
    case KeywordValue::kContain:
    case KeywordValue::kCover:
    case KeywordValue::kCurrentColor:
    case KeywordValue::kCursive:
    case KeywordValue::kEllipsis:
    case KeywordValue::kEnd:
    case KeywordValue::kEquirectangular:
    case KeywordValue::kFantasy:
    case KeywordValue::kFixed:
    case KeywordValue::kForwards:
    case KeywordValue::kHidden:
    case KeywordValue::kInfinite:
    case KeywordValue::kInherit:
    case KeywordValue::kInitial:
    case KeywordValue::kInline:
    case KeywordValue::kInlineBlock:
    case KeywordValue::kLeft:
    case KeywordValue::kLineThrough:
    case KeywordValue::kMiddle:
    case KeywordValue::kMonospace:
    case KeywordValue::kMonoscopic:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNormal:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kPreLine:
    case KeywordValue::kPreWrap:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStart:
    case KeywordValue::kStatic:
    case KeywordValue::kStereoscopicLeftRight:
    case KeywordValue::kStereoscopicTopBottom:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
      NOTREACHED();
  }
}

class ComputedLengthIsNegativeProvider : public DefaultingPropertyValueVisitor {
 public:
  ComputedLengthIsNegativeProvider() : computed_length_is_negative_(false) {}

  void VisitLength(LengthValue* length_value) override {
    switch (length_value->unit()) {
      case kPixelsUnit:
      case kFontSizesAkaEmUnit:
      case kRootElementFontSizesAkaRemUnit:
      case kViewportWidthPercentsAkaVwUnit:
      case kViewportHeightPercentsAkaVhUnit:
        computed_length_is_negative_ = length_value->value() < 0;
        break;
    }
  }

  void VisitDefault(PropertyValue* property_value) override {
    UNREFERENCED_PARAMETER(property_value);
  }

  bool computed_length_is_negative() { return computed_length_is_negative_; }

 private:
  bool computed_length_is_negative_;
};

void ComputedMinMaxWidthProvider::VisitPercentage(PercentageValue* percentage) {
  ComputedLengthIsNegativeProvider computed_length_is_negative_provider;
  parent_computed_min_max_width_->Accept(&computed_length_is_negative_provider);
  // If the containing block's width is negative, the used value is zero.
  //   https://www.w3.org/TR/CSS2/visudet.html#min-max-widths
  if (computed_length_is_negative_provider.computed_length_is_negative()) {
    computed_min_max_width_ = new LengthValue(0, kPixelsUnit);
  } else {
    computed_min_max_width_ = percentage;
  }
}

namespace {

// Helper class for |ComputedBackgroundPositionProvider| and
// |ComputedTransformOriginProvider| to resolve the computed value of position
// part.
//   https://www.w3.org/TR/css3-background/#the-background-position
//   https://www.w3.org/TR/css3-transforms/#propdef-transform-origin
class ComputedPositionHelper {
 public:
  ComputedPositionHelper(const LengthValue* computed_font_size,
                         const LengthValue* root_computed_font_size,
                         const math::Size& viewport_size);

  // Forwards the call on to the appropriate method depending on the number
  // of parameters in |input_position_builder|.
  void ComputePosition(const PropertyListValue::Builder& input_position_builder,
                       PropertyListValue::Builder* output_position_builder);

  // Only resolve the first value of |input_position_builder|, other than the
  // first value would be ignored.
  void ComputeOneValuePosition(
      const PropertyListValue::Builder& input_position_builder,
      PropertyListValue::Builder* output_position_builder);

  // Only resolve the first two values of |input_position_builder|, other than
  // the first two values would be ignored.
  void ComputeTwoValuesPosition(
      const PropertyListValue::Builder& input_position_builder,
      PropertyListValue::Builder* output_position_builder);

  // Only resolve three or four values of |input_position_builder|.
  void ComputeThreeOrFourValuesPosition(
      const PropertyListValue::Builder& input_position_builder,
      PropertyListValue::Builder* output_position_builder);

 private:
  enum Direction {
    kHorizontal,
    kVertical,
    kCenter,
    kNone,
  };

  struct OriginInfo {
    OriginInfo(float origin_as_percentage, int offset_multiplier,
               Direction direction)
        : origin_as_percentage(origin_as_percentage),
          offset_multiplier(offset_multiplier),
          direction(direction) {}

    float origin_as_percentage;
    int offset_multiplier;
    Direction direction;
  };

  const OriginInfo ConvertToOriginInfo(
      const scoped_refptr<PropertyValue>& keyword) const;

  scoped_refptr<CalcValue> ProvideCalcValueFromOriginAndOffset(
      OriginInfo* origin_info, const scoped_refptr<PropertyValue>& offset);

  void FillPositionBuilderFromOriginAndOffset(
      const scoped_refptr<PropertyValue>& origin,
      const scoped_refptr<PropertyValue>& offset,
      PropertyListValue::Builder* position_builder);

  const LengthValue* computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;

  DISALLOW_COPY_AND_ASSIGN(ComputedPositionHelper);
};

ComputedPositionHelper::ComputedPositionHelper(
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size, const math::Size& viewport_size)
    : computed_font_size_(computed_font_size),
      root_computed_font_size_(root_computed_font_size),
      viewport_size_(viewport_size) {}

void ComputedPositionHelper::ComputePosition(
    const PropertyListValue::Builder& input_position_builder,
    PropertyListValue::Builder* output_position_builder) {
  switch (input_position_builder.size()) {
    case 1:
      ComputeOneValuePosition(input_position_builder, output_position_builder);
      break;
    case 2:
      ComputeTwoValuesPosition(input_position_builder, output_position_builder);
      break;
    case 3:  // fall-through
    case 4:
      ComputeThreeOrFourValuesPosition(input_position_builder,
                                       output_position_builder);
      break;
  }
}

// If only one value is specified, the second value is assumed to be center.
void ComputedPositionHelper::ComputeOneValuePosition(
    const PropertyListValue::Builder& input_position_builder,
    PropertyListValue::Builder* output_position_builder) {
  DCHECK_GE(input_position_builder.size(), 1u);

  PropertyListValue::Builder position_builder;
  position_builder.push_back(input_position_builder[0]);
  position_builder.push_back(KeywordValue::GetCenter());

  ComputeTwoValuesPosition(position_builder, output_position_builder);
}

// If two position values are given, a length or percentage as the
// first value represents the horizontal position (or offset) and a length or
// percentage as the second value represents the vertical position (or offset).
void ComputedPositionHelper::ComputeTwoValuesPosition(
    const PropertyListValue::Builder& input_position_builder,
    PropertyListValue::Builder* output_position_builder) {
  DCHECK_GE(input_position_builder.size(), 2u);

  for (size_t i = 0; i < 2; ++i) {
    scoped_refptr<PropertyValue> current_value = input_position_builder[i];

    if (current_value->GetTypeId() == base::GetTypeId<CalcValue>()) {
      // If it is already a CalcValue, nothing needs to be done.
      (*output_position_builder)[i] = current_value;
    } else if (current_value->GetTypeId() == base::GetTypeId<KeywordValue>()) {
      FillPositionBuilderFromOriginAndOffset(current_value, NULL,
                                             output_position_builder);
    } else {
      OriginInfo default_origin = OriginInfo(0.0f, 1, kNone);
      (*output_position_builder)[i] =
          ProvideCalcValueFromOriginAndOffset(&default_origin, current_value);
    }
  }
}

// If three values are given, then there are two cases:
// 1. <KeywordValue Length/Percentage KeywordValue>
// 2. <KeywordValue KeywordValue Length/Percentage>
// If four values are given, then each <percentage> or <length> represents
// an offset and must be preceded by a keyword, which specifies from which
// edge the offset is given. Keyword cannot be 'center'. The pattern is
// <KeywordValue Length/Percentage KeywordValue Length/Percentage>
void ComputedPositionHelper::ComputeThreeOrFourValuesPosition(
    const PropertyListValue::Builder& input_position_builder,
    PropertyListValue::Builder* output_position_builder) {
  DCHECK_GT(input_position_builder.size(), 2u);
  DCHECK_LE(input_position_builder.size(), 4u);

  for (size_t i = 0; i < input_position_builder.size(); ++i) {
    scoped_refptr<PropertyValue> previous_value =
        (i == 0) ? NULL : input_position_builder[i - 1];

    scoped_refptr<PropertyValue> current_value = input_position_builder[i];

    if (current_value->GetTypeId() == base::GetTypeId<KeywordValue>()) {
      FillPositionBuilderFromOriginAndOffset(current_value, NULL,
                                             output_position_builder);
    } else {
      DCHECK(previous_value);
      DCHECK(previous_value->GetTypeId() == base::GetTypeId<KeywordValue>());
      FillPositionBuilderFromOriginAndOffset(previous_value, current_value,
                                             output_position_builder);
    }
  }
}

// 1) 'top' computes to '0%' for the vertical position if one or two values are
//     given, otherwise specifies the top edge as the origin for the next
//     offset.
// 2) 'right' computes to '100%' for the horizontal position if one or two
//     values are given, otherwise specifies the right edge as the origin for
//     the next offset.
// 3) 'bottom' computes to '100%' for the vertical position if one or two values
//     are given, otherwise specifies the bottom edge as the origin for the
//     the next offset.
// 4) 'left' computes to '0%' for the horizontal position if one or two values
//     are given, otherwise specifies the left edge as the origin for the next
//     offset.
// 5) 'center' computes to '50%' (left 50%) for the horizontal position if
//     horizontal position is not specified, or '50%' (right 50%) for the
//     vertical position is not specified.
const ComputedPositionHelper::OriginInfo
ComputedPositionHelper::ConvertToOriginInfo(
    const scoped_refptr<PropertyValue>& keyword) const {
  DCHECK(keyword->GetTypeId() == base::GetTypeId<KeywordValue>());

  if (keyword == KeywordValue::GetLeft()) {
    return OriginInfo(0.0f, 1, kHorizontal);
  } else if (keyword == KeywordValue::GetRight()) {
    return OriginInfo(1.0f, -1, kHorizontal);
  } else if (keyword == KeywordValue::GetTop()) {
    return OriginInfo(0.0f, 1, kVertical);
  } else if (keyword == KeywordValue::GetBottom()) {
    return OriginInfo(1.0f, -1, kVertical);
  } else {
    return OriginInfo(0.5f, 1, kCenter);
  }
}

// If the |offset| is specified, the |origin| specifies from which edge
// the offset is given. Otherwise, the |origin| indicates the corresponding
// percentage value to the upper left corner for the horizontal/vertical
// position. The horizontal and vertical values are stored as CalcValue in
// computed style. eg: (background-position: bottom 20px left 40%;) would be
// computed as Calc(0px, 40%), Calc(-20px, 100%)
scoped_refptr<CalcValue>
ComputedPositionHelper::ProvideCalcValueFromOriginAndOffset(
    OriginInfo* origin_info, const scoped_refptr<PropertyValue>& offset) {
  DCHECK(origin_info);

  if (!offset) {
    return new CalcValue(
        new PercentageValue(origin_info->origin_as_percentage));
  }

  scoped_refptr<LengthValue> length_value;
  scoped_refptr<PercentageValue> percentage_value;
  if (offset->GetTypeId() == base::GetTypeId<LengthValue>()) {
    scoped_refptr<LengthValue> length_provider = ProvideAbsoluteLength(
        base::polymorphic_downcast<LengthValue*>(offset.get()),
        computed_font_size_, root_computed_font_size_, viewport_size_);
    length_value = new LengthValue(
        origin_info->offset_multiplier * length_provider->value(),
        length_provider->unit());
    percentage_value = new PercentageValue(origin_info->origin_as_percentage);

    return new CalcValue(length_value, percentage_value);
  } else {
    DCHECK(offset->GetTypeId() == base::GetTypeId<PercentageValue>());
    PercentageValue* percentage =
        base::polymorphic_downcast<PercentageValue*>(offset.get());
    percentage_value = new PercentageValue(origin_info->origin_as_percentage +
                                           origin_info->offset_multiplier *
                                               percentage->value());

    return new CalcValue(percentage_value);
  }
}

void ComputedPositionHelper::FillPositionBuilderFromOriginAndOffset(
    const scoped_refptr<PropertyValue>& origin,
    const scoped_refptr<PropertyValue>& offset,
    PropertyListValue::Builder* output_position_builder) {
  DCHECK(origin->GetTypeId() == base::GetTypeId<KeywordValue>());

  OriginInfo origin_info = ConvertToOriginInfo(origin);
  switch (origin_info.direction) {
    case kHorizontal: {
      (*output_position_builder)[0] =
          ProvideCalcValueFromOriginAndOffset(&origin_info, offset);
      break;
    }
    case kVertical: {
      (*output_position_builder)[1] =
          ProvideCalcValueFromOriginAndOffset(&origin_info, offset);
      break;
    }
    case kCenter: {
      if (!(*output_position_builder)[0]) {
        (*output_position_builder)[0] =
            ProvideCalcValueFromOriginAndOffset(&origin_info, offset);
      }
      if (!(*output_position_builder)[1]) {
        (*output_position_builder)[1] =
            ProvideCalcValueFromOriginAndOffset(&origin_info, offset);
      }
      break;
    }
    case kNone:  // fall-through
      NOTREACHED();
      break;
  }
}

}  // namespace

// Absolutizes the value of "background-image" property.
// Computed value: as specified, but with URIs made absolute.
//   https://www.w3.org/TR/css3-background/#the-background-image
class ComputedBackgroundImageSingleLayerProvider
    : public NotReachedPropertyValueVisitor {
 public:
  ComputedBackgroundImageSingleLayerProvider(
      const GURL& base_url, const LengthValue* computed_font_size,
      const LengthValue* root_computed_font_size,
      const math::Size& viewport_size)
      : base_url_(base_url),
        computed_font_size_(computed_font_size),
        root_computed_font_size_(root_computed_font_size),
        viewport_size_(viewport_size) {}

  void VisitKeyword(KeywordValue* keyword) override;
  void VisitLinearGradient(LinearGradientValue* linear_gradient_value) override;
  void VisitRadialGradient(RadialGradientValue* radial_gradient_value) override;
  void VisitURL(URLValue* url_value) override;

  const scoped_refptr<PropertyValue>& computed_background_image() const {
    return computed_background_image_;
  }

 private:
  const GURL base_url_;
  const LengthValue* computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;

  scoped_refptr<PropertyValue> computed_background_image_;
};

void ComputedBackgroundImageSingleLayerProvider::VisitKeyword(
    KeywordValue* keyword) {
  switch (keyword->value()) {
    case KeywordValue::kNone:
      computed_background_image_ = keyword;
      break;
    case KeywordValue::kAbsolute:
    case KeywordValue::kAlternate:
    case KeywordValue::kAlternateReverse:
    case KeywordValue::kAuto:
    case KeywordValue::kBackwards:
    case KeywordValue::kBaseline:
    case KeywordValue::kBlock:
    case KeywordValue::kBoth:
    case KeywordValue::kBottom:
    case KeywordValue::kBreakWord:
    case KeywordValue::kCenter:
    case KeywordValue::kClip:
    case KeywordValue::kContain:
    case KeywordValue::kCover:
    case KeywordValue::kCurrentColor:
    case KeywordValue::kCursive:
    case KeywordValue::kEllipsis:
    case KeywordValue::kEnd:
    case KeywordValue::kEquirectangular:
    case KeywordValue::kFantasy:
    case KeywordValue::kFixed:
    case KeywordValue::kForwards:
    case KeywordValue::kHidden:
    case KeywordValue::kInfinite:
    case KeywordValue::kInherit:
    case KeywordValue::kInitial:
    case KeywordValue::kInline:
    case KeywordValue::kInlineBlock:
    case KeywordValue::kLeft:
    case KeywordValue::kLineThrough:
    case KeywordValue::kMiddle:
    case KeywordValue::kMonoscopic:
    case KeywordValue::kMonospace:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNormal:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kPreLine:
    case KeywordValue::kPreWrap:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStart:
    case KeywordValue::kStatic:
    case KeywordValue::kStereoscopicLeftRight:
    case KeywordValue::kStereoscopicTopBottom:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
      NOTREACHED();
      break;
  }
}

namespace {
ColorStopList ComputeColorStopList(const ColorStopList& color_stops,
                                   const LengthValue* computed_font_size,
                                   const LengthValue* root_computed_font_size,
                                   const math::Size& viewport_size) {
  ColorStopList computed_color_stops;
  computed_color_stops.reserve(color_stops.size());

  for (ColorStopList::const_iterator iter = color_stops.begin();
       iter != color_stops.end(); ++iter) {
    const ColorStop& color_stop = **iter;

    computed_color_stops.push_back(new ColorStop(
        color_stop.rgba(), ProvideAbsoluteLengthIfNonNullLength(
                               color_stop.position(), computed_font_size,
                               root_computed_font_size, viewport_size)));
  }

  return computed_color_stops.Pass();
}
}  // namespace

void ComputedBackgroundImageSingleLayerProvider::VisitLinearGradient(
    LinearGradientValue* linear_gradient_value) {
  // We must walk through the list of color stop positions and absolutize the
  // any length values.
  ColorStopList computed_color_stops = ComputeColorStopList(
      linear_gradient_value->color_stop_list(), computed_font_size_,
      root_computed_font_size_, viewport_size_);

  if (linear_gradient_value->angle_in_radians()) {
    computed_background_image_ =
        new LinearGradientValue(*linear_gradient_value->angle_in_radians(),
                                computed_color_stops.Pass());
  } else {
    computed_background_image_ = new LinearGradientValue(
        *linear_gradient_value->side_or_corner(), computed_color_stops.Pass());
  }
}

namespace {
scoped_refptr<PropertyListValue> CalculateComputedRadialGradientPosition(
    const scoped_refptr<PropertyListValue>& specified_position,
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size,
    const math::Size& viewport_size) {
  if (!specified_position) {
    // If no position is specified, we default to 'center'.
    scoped_ptr<PropertyListValue::Builder> builder(
        new PropertyListValue::Builder(2, new PercentageValue(0.5f)));
    return new PropertyListValue(builder.Pass());
  }

  size_t size = specified_position->value().size();
  DCHECK_GE(size, 1u);
  DCHECK_LE(size, 4u);

  ComputedPositionHelper position_helper(
      computed_font_size, root_computed_font_size, viewport_size);
  scoped_ptr<PropertyListValue::Builder> computed_position_builder(
      new PropertyListValue::Builder(2, scoped_refptr<PropertyValue>()));
  position_helper.ComputePosition(specified_position->value(),
                                  computed_position_builder.get());

  return new PropertyListValue(computed_position_builder.Pass());
}
}  // namespace

void ComputedBackgroundImageSingleLayerProvider::VisitRadialGradient(
    RadialGradientValue* radial_gradient_value) {
  // We must walk through the list of color stop positions and absolutize the
  // any length values.
  ColorStopList computed_color_stops = ComputeColorStopList(
      radial_gradient_value->color_stop_list(), computed_font_size_,
      root_computed_font_size_, viewport_size_);

  // The center of the gradient must be absolutized if it is a length.
  scoped_refptr<PropertyListValue> computed_position =
      CalculateComputedRadialGradientPosition(
          radial_gradient_value->position(), computed_font_size_,
          root_computed_font_size_, viewport_size_);

  // If the radial gradient specifies size values, they must also be absolutized
  // if they are lengths.
  if (radial_gradient_value->size_value()) {
    computed_background_image_ = new RadialGradientValue(
        radial_gradient_value->shape(),
        ProvideAbsoluteLengthsForNonNullLengthsInList(
            radial_gradient_value->size_value(), computed_font_size_,
            root_computed_font_size_, viewport_size_),
        computed_position, computed_color_stops.Pass());
  } else {
    computed_background_image_ = new RadialGradientValue(
        radial_gradient_value->shape(), *radial_gradient_value->size_keyword(),
        computed_position, computed_color_stops.Pass());
  }
}

void ComputedBackgroundImageSingleLayerProvider::VisitURL(URLValue* url_value) {
  if (url_value->value().empty()) {
    // No need to convert URLValue into AbsoluteURLValue.
    computed_background_image_ = KeywordValue::GetNone();
    return;
  }

  GURL absolute_url;
  if (url_value->is_absolute()) {
    absolute_url = GURL(url_value->value());
  } else {
    absolute_url = url_value->Resolve(base_url_);
  }

  if (!absolute_url.is_valid()) {
    DLOG(WARNING) << "Invalid url: " << absolute_url.spec();
    // No further process is needed if the url is invalid.
    computed_background_image_ = KeywordValue::GetNone();
    return;
  }

  computed_background_image_ = new AbsoluteURLValue(absolute_url);
}

class ComputedBackgroundImageProvider : public NotReachedPropertyValueVisitor {
 public:
  ComputedBackgroundImageProvider(const GURL& base_url,
                                  const LengthValue* computed_font_size,
                                  const LengthValue* root_computed_font_size,
                                  const math::Size& viewport_size)
      : base_url_(base_url),
        computed_font_size_(computed_font_size),
        root_computed_font_size_(root_computed_font_size),
        viewport_size_(viewport_size) {}

  void VisitPropertyList(PropertyListValue* property_list_value) override;

  const scoped_refptr<PropertyValue>& computed_background_image() const {
    return computed_background_image_;
  }

 private:
  const GURL base_url_;
  const LengthValue* computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;

  scoped_refptr<PropertyValue> computed_background_image_;
};

void ComputedBackgroundImageProvider::VisitPropertyList(
    PropertyListValue* property_list_value) {
  scoped_ptr<PropertyListValue::Builder> builder(
      new PropertyListValue::Builder());
  builder->reserve(property_list_value->value().size());

  ComputedBackgroundImageSingleLayerProvider single_layer_provider(
      base_url_, computed_font_size_, root_computed_font_size_, viewport_size_);
  for (size_t i = 0; i < property_list_value->value().size(); ++i) {
    property_list_value->value()[i]->Accept(&single_layer_provider);
    scoped_refptr<PropertyValue> computed_background_image =
        single_layer_provider.computed_background_image();
    builder->push_back(computed_background_image);
  }

  computed_background_image_ = new PropertyListValue(builder.Pass());
}

class ComputedBackgroundSizeSingleValueProvider
    : public NotReachedPropertyValueVisitor {
 public:
  ComputedBackgroundSizeSingleValueProvider(
      const LengthValue* computed_font_size,
      const LengthValue* root_computed_font_size,
      const math::Size& viewport_size);

  void VisitLength(LengthValue* length) override;
  void VisitPercentage(PercentageValue* percentage) override;
  void VisitKeyword(KeywordValue* keyword) override;

  const scoped_refptr<PropertyValue>& computed_background_size() const {
    return computed_background_size_;
  }

 private:
  const LengthValue* computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;

  scoped_refptr<PropertyValue> computed_background_size_;

  DISALLOW_COPY_AND_ASSIGN(ComputedBackgroundSizeSingleValueProvider);
};

ComputedBackgroundSizeSingleValueProvider::
    ComputedBackgroundSizeSingleValueProvider(
        const LengthValue* computed_font_size,
        const LengthValue* root_computed_font_size,
        const math::Size& viewport_size)
    : computed_font_size_(computed_font_size),
      root_computed_font_size_(root_computed_font_size),
      viewport_size_(viewport_size) {}

void ComputedBackgroundSizeSingleValueProvider::VisitLength(
    LengthValue* length) {
  computed_background_size_ = ProvideAbsoluteLength(
      length, computed_font_size_, root_computed_font_size_, viewport_size_);
}

void ComputedBackgroundSizeSingleValueProvider::VisitPercentage(
    PercentageValue* percentage) {
  computed_background_size_ = percentage;
}

void ComputedBackgroundSizeSingleValueProvider::VisitKeyword(
    KeywordValue* keyword) {
  switch (keyword->value()) {
    case KeywordValue::kAuto:
    case KeywordValue::kContain:
    case KeywordValue::kCover:
      computed_background_size_ = keyword;
      break;

    case KeywordValue::kAbsolute:
    case KeywordValue::kAlternate:
    case KeywordValue::kAlternateReverse:
    case KeywordValue::kBackwards:
    case KeywordValue::kBaseline:
    case KeywordValue::kBlock:
    case KeywordValue::kBoth:
    case KeywordValue::kBottom:
    case KeywordValue::kBreakWord:
    case KeywordValue::kCenter:
    case KeywordValue::kClip:
    case KeywordValue::kCurrentColor:
    case KeywordValue::kCursive:
    case KeywordValue::kEllipsis:
    case KeywordValue::kEnd:
    case KeywordValue::kEquirectangular:
    case KeywordValue::kFantasy:
    case KeywordValue::kFixed:
    case KeywordValue::kForwards:
    case KeywordValue::kHidden:
    case KeywordValue::kInfinite:
    case KeywordValue::kInherit:
    case KeywordValue::kInitial:
    case KeywordValue::kInline:
    case KeywordValue::kInlineBlock:
    case KeywordValue::kLeft:
    case KeywordValue::kLineThrough:
    case KeywordValue::kMiddle:
    case KeywordValue::kMonoscopic:
    case KeywordValue::kMonospace:
    case KeywordValue::kNone:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNormal:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kPreLine:
    case KeywordValue::kPreWrap:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStart:
    case KeywordValue::kStatic:
    case KeywordValue::kStereoscopicLeftRight:
    case KeywordValue::kStereoscopicTopBottom:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
      NOTREACHED();
  }
}

// ComputedBackgroundPositionProvider provides a property list which has two
// CalcValue. Each of CalcValue has two parts <percentage> and <length>.
// <percentage> and <length> values here represent an offset of the top left
// corner of the background image from the top left corner of the background
// positioning area.
//   https://www.w3.org/TR/css3-background/#the-background-position
class ComputedBackgroundPositionProvider
    : public NotReachedPropertyValueVisitor {
 public:
  ComputedBackgroundPositionProvider(const LengthValue* computed_font_size,
                                     const LengthValue* root_computed_font_size,
                                     const math::Size& viewport_size);

  void VisitPropertyList(PropertyListValue* property_list_value) override;

  const scoped_refptr<PropertyValue>& computed_background_position() const {
    return computed_background_position_;
  }

 private:
  const LengthValue* computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;

  scoped_refptr<PropertyValue> computed_background_position_;

  DISALLOW_COPY_AND_ASSIGN(ComputedBackgroundPositionProvider);
};

ComputedBackgroundPositionProvider::ComputedBackgroundPositionProvider(
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size, const math::Size& viewport_size)
    : computed_font_size_(computed_font_size),
      root_computed_font_size_(root_computed_font_size),
      viewport_size_(viewport_size) {}

void ComputedBackgroundPositionProvider::VisitPropertyList(
    PropertyListValue* property_list_value) {
  size_t size = property_list_value->value().size();
  DCHECK_GE(size, 1u);
  DCHECK_LE(size, 4u);

  ComputedPositionHelper position_helper(
      computed_font_size_, root_computed_font_size_, viewport_size_);
  scoped_ptr<PropertyListValue::Builder> background_position_builder(
      new PropertyListValue::Builder(2, scoped_refptr<PropertyValue>()));

  position_helper.ComputePosition(property_list_value->value(),
                                  background_position_builder.get());

  computed_background_position_ =
      new PropertyListValue(background_position_builder.Pass());
}

class ComputedBackgroundSizeProvider : public NotReachedPropertyValueVisitor {
 public:
  ComputedBackgroundSizeProvider(const LengthValue* computed_font_size,
                                 const LengthValue* root_computed_font_size,
                                 const math::Size& viewport_size);

  void VisitKeyword(KeywordValue* keyword) override;
  void VisitPropertyList(PropertyListValue* property_list_value) override;

  const scoped_refptr<PropertyValue>& computed_background_size() const {
    return computed_background_size_;
  }

 private:
  const LengthValue* computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;

  scoped_refptr<PropertyValue> computed_background_size_;

  DISALLOW_COPY_AND_ASSIGN(ComputedBackgroundSizeProvider);
};

ComputedBackgroundSizeProvider::ComputedBackgroundSizeProvider(
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size, const math::Size& viewport_size)
    : computed_font_size_(computed_font_size),
      root_computed_font_size_(root_computed_font_size),
      viewport_size_(viewport_size) {}

void ComputedBackgroundSizeProvider::VisitKeyword(KeywordValue* keyword) {
  computed_background_size_ = keyword;
}

void ComputedBackgroundSizeProvider::VisitPropertyList(
    PropertyListValue* property_list_value) {
  ComputedBackgroundSizeSingleValueProvider left_value_provider(
      computed_font_size_, root_computed_font_size_, viewport_size_);
  property_list_value->value()[0]->Accept(&left_value_provider);

  ComputedBackgroundSizeSingleValueProvider right_value_provider(
      computed_font_size_, root_computed_font_size_, viewport_size_);
  property_list_value->value()[1]->Accept(&right_value_provider);

  scoped_ptr<PropertyListValue::Builder> builder(
      new PropertyListValue::Builder());
  builder->reserve(2);
  builder->push_back(left_value_provider.computed_background_size());
  builder->push_back(right_value_provider.computed_background_size());
  computed_background_size_ = new PropertyListValue(builder.Pass());
}

//    https://www.w3.org/TR/css3-background/#border-radius
class ComputedBorderRadiusProvider : public NotReachedPropertyValueVisitor {
 public:
  ComputedBorderRadiusProvider(const LengthValue* computed_font_size,
                               const LengthValue* root_computed_font_size,
                               const math::Size& viewport_size);

  void VisitLength(LengthValue* specified_length);
  void VisitPercentage(PercentageValue* percentage);

  const scoped_refptr<PropertyValue>& computed_border_radius() const {
    return computed_border_radius_;
  }

 private:
  const LengthValue* computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;

  scoped_refptr<PropertyValue> computed_border_radius_;

  DISALLOW_COPY_AND_ASSIGN(ComputedBorderRadiusProvider);
};

ComputedBorderRadiusProvider::ComputedBorderRadiusProvider(
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size, const math::Size& viewport_size)
    : computed_font_size_(computed_font_size),
      root_computed_font_size_(root_computed_font_size),
      viewport_size_(viewport_size) {}

void ComputedBorderRadiusProvider::VisitLength(LengthValue* specified_length) {
  computed_border_radius_ =
      ProvideAbsoluteLength(specified_length, computed_font_size_,
                            root_computed_font_size_, viewport_size_);
}

void ComputedBorderRadiusProvider::VisitPercentage(
    PercentageValue* percentage) {
  computed_border_radius_ = percentage;
}

// Computed value: any <length> made absolute; any specified color computed;
// otherwise as specified.
//   https://www.w3.org/TR/css3-background/#box-shadow
//   https://www.w3.org/TR/css-text-decor-3/#text-shadow-property
class ComputedShadowProvider : public NotReachedPropertyValueVisitor {
 public:
  ComputedShadowProvider(const LengthValue* computed_font_size,
                         const LengthValue* root_computed_font_size,
                         const math::Size& viewport_size,
                         const RGBAColorValue* computed_color);

  void VisitKeyword(KeywordValue* keyword) override;
  void VisitPropertyList(PropertyListValue* property_list_value) override;

  const scoped_refptr<PropertyValue>& computed_shadow() const {
    return computed_shadow_;
  }

 private:
  const LengthValue* computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;
  const RGBAColorValue* computed_color_;

  scoped_refptr<PropertyValue> computed_shadow_;

  DISALLOW_COPY_AND_ASSIGN(ComputedShadowProvider);
};

ComputedShadowProvider::ComputedShadowProvider(
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size, const math::Size& viewport_size,
    const RGBAColorValue* computed_color)
    : computed_font_size_(computed_font_size),
      root_computed_font_size_(root_computed_font_size),
      viewport_size_(viewport_size),
      computed_color_(computed_color) {}

void ComputedShadowProvider::VisitKeyword(KeywordValue* keyword) {
  switch (keyword->value()) {
    case KeywordValue::kNone:
      computed_shadow_ = keyword;
      break;

    case KeywordValue::kAbsolute:
    case KeywordValue::kAlternate:
    case KeywordValue::kAlternateReverse:
    case KeywordValue::kAuto:
    case KeywordValue::kBackwards:
    case KeywordValue::kBaseline:
    case KeywordValue::kBlock:
    case KeywordValue::kBoth:
    case KeywordValue::kBottom:
    case KeywordValue::kBreakWord:
    case KeywordValue::kCenter:
    case KeywordValue::kClip:
    case KeywordValue::kContain:
    case KeywordValue::kCover:
    case KeywordValue::kCurrentColor:
    case KeywordValue::kCursive:
    case KeywordValue::kEllipsis:
    case KeywordValue::kEnd:
    case KeywordValue::kEquirectangular:
    case KeywordValue::kFantasy:
    case KeywordValue::kForwards:
    case KeywordValue::kFixed:
    case KeywordValue::kHidden:
    case KeywordValue::kInfinite:
    case KeywordValue::kInherit:
    case KeywordValue::kInitial:
    case KeywordValue::kInline:
    case KeywordValue::kInlineBlock:
    case KeywordValue::kLeft:
    case KeywordValue::kLineThrough:
    case KeywordValue::kMiddle:
    case KeywordValue::kMonoscopic:
    case KeywordValue::kMonospace:
    case KeywordValue::kNormal:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kPreLine:
    case KeywordValue::kPreWrap:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStart:
    case KeywordValue::kStatic:
    case KeywordValue::kStereoscopicLeftRight:
    case KeywordValue::kStereoscopicTopBottom:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
      NOTREACHED();
  }
}

void ComputedShadowProvider::VisitPropertyList(
    PropertyListValue* property_list_value) {
  scoped_ptr<PropertyListValue::Builder> builder(
      new PropertyListValue::Builder());
  builder->reserve(property_list_value->value().size());

  for (size_t i = 0; i < property_list_value->value().size(); ++i) {
    ShadowValue* shadow_value = base::polymorphic_downcast<ShadowValue*>(
        property_list_value->value()[i].get());

    scoped_refptr<LengthValue> computed_lengths[ShadowValue::kMaxLengths];
    for (int j = 0; j < ShadowValue::kMaxLengths; ++j) {
      LengthValue* specified_length = shadow_value->lengths()[j].get();
      if (specified_length) {
        computed_lengths[j] =
            ProvideAbsoluteLength(specified_length, computed_font_size_,
                                  root_computed_font_size_, viewport_size_);
      }
    }

    scoped_refptr<RGBAColorValue> color = shadow_value->color();
    if (!color) {
      color = new RGBAColorValue(computed_color_->value());
    }

    builder->push_back(
        new ShadowValue(computed_lengths, color, shadow_value->has_inset()));
  }

  computed_shadow_ = new PropertyListValue(builder.Pass());
}

// Computed value: for length of translation transforms.
//   https://www.w3.org/TR/css3-transforms/#propdef-transform
class ComputedTransformFunctionProvider : public TransformFunctionVisitor {
 public:
  ComputedTransformFunctionProvider(const LengthValue* computed_font_size,
                                    const LengthValue* root_computed_font_size,
                                    const math::Size& viewport_size);

  void VisitMatrix(const MatrixFunction* matrix_function) override;
  void VisitRotate(const RotateFunction* rotate_function) override;
  void VisitScale(const ScaleFunction* scale_function) override;
  void VisitTranslate(const TranslateFunction* translate_function) override;

  scoped_ptr<TransformFunction> PassComputedTransformFunction() {
    return computed_transform_function_.Pass();
  }

 private:
  scoped_ptr<TransformFunction> computed_transform_function_;
  const LengthValue* computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;
};

ComputedTransformFunctionProvider::ComputedTransformFunctionProvider(
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size, const math::Size& viewport_size)
    : computed_font_size_(computed_font_size),
      root_computed_font_size_(root_computed_font_size),
      viewport_size_(viewport_size) {}

void ComputedTransformFunctionProvider::VisitMatrix(
    const MatrixFunction* matrix_function) {
  computed_transform_function_.reset(new MatrixFunction(*matrix_function));
}

void ComputedTransformFunctionProvider::VisitRotate(
    const RotateFunction* rotate_function) {
  computed_transform_function_.reset(new RotateFunction(*rotate_function));
}

void ComputedTransformFunctionProvider::VisitScale(
    const ScaleFunction* scale_function) {
  computed_transform_function_.reset(new ScaleFunction(*scale_function));
}

void ComputedTransformFunctionProvider::VisitTranslate(
    const TranslateFunction* translate_function) {
  switch (translate_function->offset_type()) {
    case TranslateFunction::kLength: {
      computed_transform_function_.reset(new TranslateFunction(
          translate_function->axis(),
          ProvideAbsoluteLength(translate_function->offset_as_length(),
                                computed_font_size_, root_computed_font_size_,
                                viewport_size_)));
    } break;
    case TranslateFunction::kPercentage: {
      computed_transform_function_.reset(
          new TranslateFunction(*translate_function));
    } break;
    case TranslateFunction::kCalc: {
      scoped_refptr<CalcValue> calc_value =
          translate_function->offset_as_calc();
      computed_transform_function_.reset(new TranslateFunction(
          translate_function->axis(),
          new CalcValue(ProvideAbsoluteLength(
                            calc_value->length_value(), computed_font_size_,
                            root_computed_font_size_, viewport_size_),
                        calc_value->percentage_value())));
    } break;
  }
}

// Absolutizes the value of "text-indent" property.
class ComputedTextIndentProvider : public NotReachedPropertyValueVisitor {
 public:
  ComputedTextIndentProvider(const LengthValue* computed_font_size,
                             const LengthValue* root_computed_font_size,
                             const math::Size& viewport_size);

  void VisitLength(LengthValue* length) override;

  const scoped_refptr<LengthValue>& computed_text_indent() const {
    return computed_text_indent_;
  }

 private:
  const LengthValue* computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;

  scoped_refptr<LengthValue> computed_text_indent_;

  DISALLOW_COPY_AND_ASSIGN(ComputedTextIndentProvider);
};

ComputedTextIndentProvider::ComputedTextIndentProvider(
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size, const math::Size& viewport_size)
    : computed_font_size_(computed_font_size),
      root_computed_font_size_(root_computed_font_size),
      viewport_size_(viewport_size) {}

void ComputedTextIndentProvider::VisitLength(LengthValue* specified_length) {
  computed_text_indent_ =
      ProvideAbsoluteLength(specified_length, computed_font_size_,
                            root_computed_font_size_, viewport_size_);
}

// ComputedTransformOriginProvider provides a property list which has three
// PropertyValues. The first two PropertyValues are CalcValue to represent
// the horizontal position (or offset) and the vertical position (or offset).
// The third value always represents the Z position (or offset) and must be a
// LengthValue.
//  https://www.w3.org/TR/css3-transforms/#propdef-transform-origin
class ComputedTransformOriginProvider : public NotReachedPropertyValueVisitor {
 public:
  ComputedTransformOriginProvider(const LengthValue* computed_font_size,
                                  const LengthValue* root_computed_font_size,
                                  const math::Size& viewport_size);

  void VisitPropertyList(PropertyListValue* property_list_value) override;

  const scoped_refptr<PropertyValue>& computed_transform_origin() const {
    return computed_transform_origin_;
  }

 private:
  const LengthValue* computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;

  scoped_refptr<PropertyValue> computed_transform_origin_;

  DISALLOW_COPY_AND_ASSIGN(ComputedTransformOriginProvider);
};

ComputedTransformOriginProvider::ComputedTransformOriginProvider(
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size, const math::Size& viewport_size)
    : computed_font_size_(computed_font_size),
      root_computed_font_size_(root_computed_font_size),
      viewport_size_(viewport_size) {}

void ComputedTransformOriginProvider::VisitPropertyList(
    PropertyListValue* property_list_value) {
  size_t size = property_list_value->value().size();
  DCHECK_GE(size, 1u);
  DCHECK_LE(size, 3u);

  ComputedPositionHelper position_helper(
      computed_font_size_, root_computed_font_size_, viewport_size_);
  scoped_ptr<PropertyListValue::Builder> transform_origin_builder(
      new PropertyListValue::Builder(3, scoped_refptr<PropertyValue>()));

  // If one or two values are specified, the third value is assumed to be 0px.
  switch (size) {
    case 1:
    case 2:
      position_helper.ComputePosition(property_list_value->value(),
                                      transform_origin_builder.get());
      (*transform_origin_builder)[2] = new LengthValue(0.0f, kPixelsUnit);
      break;
    case 3:
      position_helper.ComputeTwoValuesPosition(property_list_value->value(),
                                               transform_origin_builder.get());
      // The third value must be LengthValue type.
      (*transform_origin_builder)[2] = ProvideAbsoluteLength(
          base::polymorphic_downcast<LengthValue*>(
              property_list_value->value()[2].get()),
          computed_font_size_, root_computed_font_size_, viewport_size_);
      break;
  }

  computed_transform_origin_ =
      new PropertyListValue(transform_origin_builder.Pass());
}

namespace {

// Functionality to check if a transform contains any relative units, such as
// "em" or "rem".

class TransformFunctionContainsRelativeUnitVisitor
    : public TransformFunctionVisitor {
 public:
  TransformFunctionContainsRelativeUnitVisitor()
      : contains_relative_unit_(false) {}

  void VisitMatrix(const MatrixFunction* matrix_function) override {
    UNREFERENCED_PARAMETER(matrix_function);
  }
  void VisitRotate(const RotateFunction* rotate_function) override {
    UNREFERENCED_PARAMETER(rotate_function);
  }
  void VisitScale(const ScaleFunction* scale_function) override {
    UNREFERENCED_PARAMETER(scale_function);
  }
  void VisitTranslate(const TranslateFunction* translate_function) override {
    contains_relative_unit_ =
        translate_function->offset_type() == TranslateFunction::kLength &&
        translate_function->offset_as_length()->IsUnitRelative();
  }

  bool contains_relative_unit() const { return contains_relative_unit_; }

 private:
  bool contains_relative_unit_;
};

bool TransformListContainsRelativeUnits(
    TransformFunctionListValue* transform_function_list) {
  for (TransformFunctionListValue::Builder::const_iterator iter =
           transform_function_list->value().begin();
       iter != transform_function_list->value().end(); ++iter) {
    TransformFunction* transform_function = *iter;

    TransformFunctionContainsRelativeUnitVisitor contains_ems_visitor;
    transform_function->Accept(&contains_ems_visitor);
    if (contains_ems_visitor.contains_relative_unit()) {
      return true;
    }
  }
  return false;
}

}  // namespace

class ComputedTransformProvider : public NotReachedPropertyValueVisitor {
 public:
  ComputedTransformProvider(const LengthValue* computed_font_size,
                            const LengthValue* root_computed_font_size,
                            const math::Size& viewport_size);

  void VisitKeyword(KeywordValue* keyword) override;
  void VisitTransformFunctionList(
      TransformFunctionListValue* transform_function_list) override;

  const scoped_refptr<PropertyValue>& computed_transform_list() const {
    return computed_transform_list_;
  }

 private:
  const LengthValue* computed_font_size_;
  const LengthValue* root_computed_font_size_;
  const math::Size& viewport_size_;

  scoped_refptr<PropertyValue> computed_transform_list_;

  DISALLOW_COPY_AND_ASSIGN(ComputedTransformProvider);
};

ComputedTransformProvider::ComputedTransformProvider(
    const LengthValue* computed_font_size,
    const LengthValue* root_computed_font_size, const math::Size& viewport_size)
    : computed_font_size_(computed_font_size),
      root_computed_font_size_(root_computed_font_size),
      viewport_size_(viewport_size) {}

void ComputedTransformProvider::VisitTransformFunctionList(
    TransformFunctionListValue* transform_function_list) {
  if (!TransformListContainsRelativeUnits(transform_function_list)) {
    // If the transform list contains no transforms that use relative units,
    // then we do not need to do anything and we can pass through the existing
    // transform.
    computed_transform_list_ = transform_function_list;
  } else {
    // The transform list contains at least one transform with relative units.
    // In this case, rebuild the transform list with computed length values.
    TransformFunctionListValue::Builder computed_list_builder;

    for (TransformFunctionListValue::Builder::const_iterator iter =
             transform_function_list->value().begin();
         iter != transform_function_list->value().end(); ++iter) {
      TransformFunction* transform_function = *iter;

      ComputedTransformFunctionProvider computed_transform_function_provider(
          computed_font_size_, root_computed_font_size_, viewport_size_);
      transform_function->Accept(&computed_transform_function_provider);

      computed_list_builder.push_back(
          computed_transform_function_provider.PassComputedTransformFunction()
              .release());
    }

    computed_transform_list_ =
        new TransformFunctionListValue(computed_list_builder.Pass());
  }
}

void ComputedTransformProvider::VisitKeyword(KeywordValue* keyword) {
  switch (keyword->value()) {
    case KeywordValue::kNone:
      computed_transform_list_ = keyword;
      break;
    case KeywordValue::kAbsolute:
    case KeywordValue::kAlternate:
    case KeywordValue::kAlternateReverse:
    case KeywordValue::kAuto:
    case KeywordValue::kBackwards:
    case KeywordValue::kBaseline:
    case KeywordValue::kBlock:
    case KeywordValue::kBoth:
    case KeywordValue::kBottom:
    case KeywordValue::kBreakWord:
    case KeywordValue::kCenter:
    case KeywordValue::kClip:
    case KeywordValue::kContain:
    case KeywordValue::kCover:
    case KeywordValue::kCurrentColor:
    case KeywordValue::kCursive:
    case KeywordValue::kEllipsis:
    case KeywordValue::kEnd:
    case KeywordValue::kEquirectangular:
    case KeywordValue::kFantasy:
    case KeywordValue::kFixed:
    case KeywordValue::kForwards:
    case KeywordValue::kHidden:
    case KeywordValue::kInfinite:
    case KeywordValue::kInherit:
    case KeywordValue::kInitial:
    case KeywordValue::kInline:
    case KeywordValue::kInlineBlock:
    case KeywordValue::kLeft:
    case KeywordValue::kLineThrough:
    case KeywordValue::kMiddle:
    case KeywordValue::kMonoscopic:
    case KeywordValue::kMonospace:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNormal:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kPreLine:
    case KeywordValue::kPreWrap:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStart:
    case KeywordValue::kStatic:
    case KeywordValue::kStereoscopicLeftRight:
    case KeywordValue::kStereoscopicTopBottom:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
      NOTREACHED();
  }
}

// This helper class creates a context within which cascaded style properties
// can be efficiently promoted to computed properties.
// In particular, some computed style calculations depend on other computed
// styles, and this class manages the caching of those dependent values so that
// if they are depended upon more than once, they are quickly recalled, and if
// they are never depended upon, no extra time is spend resolving them.  For
// example, many properties depend on font size, and so they can simply call
// CalculateComputedStyleContext::GetFontSize() to obtain that value, and all
// computations will be handled internally.
class CalculateComputedStyleContext {
 public:
  CalculateComputedStyleContext(
      CSSComputedStyleData* cascaded_style,
      const scoped_refptr<CSSComputedStyleDeclaration>&
          parent_computed_style_declaration,
      const scoped_refptr<const CSSComputedStyleData>& root_computed_style,
      const math::Size& viewport_size,
      GURLMap* const property_key_to_base_url_map)
      : cascaded_style_(cascaded_style),
        parent_computed_style_(*parent_computed_style_declaration->data()),
        root_computed_style_(*root_computed_style),
        viewport_size_(viewport_size),
        property_key_to_base_url_map_(property_key_to_base_url_map) {
    cascaded_style_->SetParentComputedStyleDeclaration(
        parent_computed_style_declaration);
  }

  // Updates the property specified by the iterator to its computed value.
  void SetComputedStyleForProperty(PropertyKey key,
                                   scoped_refptr<PropertyValue>* value);

 private:
  // Immediately promote the specified property key to computed value (if
  // necessary).
  void ComputeValue(PropertyKey key);

  // Check if the property value is set to inherit or initial, and assign it
  // an appropriate computed value in this case.
  bool HandleInheritOrInitial(PropertyKey key,
                              scoped_refptr<PropertyValue>* value);

  // Check what property property we are dealing with, and promote it to
  // a computed value accordingly (e.g. by invoking one of the many different
  // computed style computations defined above.)
  void HandleSpecifiedValue(PropertyKey key,
                            scoped_refptr<PropertyValue>* value);

  // If the modified value was a (potentially) dependent property value, cache
  // its computed value so that we know it has been computed.
  void OnComputedStyleCalculated(PropertyKey key,
                                 const scoped_refptr<PropertyValue>& value);

  // Helper function to determine if the computed style implies absolute
  // positioning.
  bool IsAbsolutelyPositioned();

  // Helper function to return the computed font size.
  LengthValue* GetFontSize();
  // Helper function to return the computed font size of root element.
  LengthValue* GetRootFontSize();
  // Helper function to return one percent of the viewport size .
  const math::Size& GetViewportSizeOnePercent();

  // Helper function to return the computed border style for an edge based on
  // border width properties.
  PropertyValue* GetBorderOrOutlineStyleBasedOnWidth(PropertyKey key);
  PropertyValue* GetBorderBottomStyle();
  PropertyValue* GetBorderLeftStyle();
  PropertyValue* GetBorderRightStyle();
  PropertyValue* GetBorderTopStyle();
  PropertyValue* GetOutlineStyle();

  // Helper function to return the computed color.
  RGBAColorValue* GetColor();

  // The style that, during the scope of CalculateComputedStyleContext, is
  // promoted from being a cascaded style to a computed style.
  CSSComputedStyleData* cascaded_style_;

  // The parent computed style.
  const CSSComputedStyleData& parent_computed_style_;
  // The root computed style.
  const CSSComputedStyleData& root_computed_style_;

  // One percent of width and height of viewport size.
  const math::Size& viewport_size_;

  // Provides a base URL for each property key.  This is used by properties
  // that deal with URLs, such as background-image, to resolve relative URLs
  // based on which style sheet they were specified from.
  GURLMap* const property_key_to_base_url_map_;

  // Cached computed values for a small specific set of properties that other
  // properties computed style calculations depend upon.  These are lazily
  // computed.
  scoped_refptr<PropertyValue> computed_border_bottom_style_;
  scoped_refptr<PropertyValue> computed_border_left_style_;
  scoped_refptr<PropertyValue> computed_border_right_style_;
  scoped_refptr<PropertyValue> computed_border_top_style_;
  scoped_refptr<PropertyValue> computed_color_;
  scoped_refptr<PropertyValue> computed_font_size_;
  scoped_refptr<PropertyValue> computed_outline_style_;
  scoped_refptr<PropertyValue> computed_position_;
};

void CalculateComputedStyleContext::SetComputedStyleForProperty(
    PropertyKey key, scoped_refptr<PropertyValue>* value) {
  // If a property has keyword value 'inherit' or 'initial', it must be
  // set to the corresponding inherited or initial value.  In this case,
  // the parent's value is already computed so we can skip the computation
  // step.
  if (!HandleInheritOrInitial(key, value)) {
    HandleSpecifiedValue(key, value);
  }
  OnComputedStyleCalculated(key, *value);
}

bool CalculateComputedStyleContext::IsAbsolutelyPositioned() {
  // An absolutely positioned element (or its box) implies that the element's
  // 'position' property has the value 'absolute' or 'fixed'.
  //   https://www.w3.org/TR/CSS21/visuren.html#absolutely-positioned
  if (!computed_position_) {
    ComputeValue(kPositionProperty);
  }

  DCHECK(computed_position_);
  return computed_position_ == KeywordValue::GetAbsolute() ||
         computed_position_ == KeywordValue::GetFixed();
}

LengthValue* CalculateComputedStyleContext::GetFontSize() {
  if (!computed_font_size_) {
    ComputeValue(kFontSizeProperty);
  }

  DCHECK(computed_font_size_);
  return base::polymorphic_downcast<LengthValue*>(computed_font_size_.get());
}

LengthValue* CalculateComputedStyleContext::GetRootFontSize() {
  return base::polymorphic_downcast<LengthValue*>(
      root_computed_style_.font_size().get());
}

const math::Size& CalculateComputedStyleContext::GetViewportSizeOnePercent() {
  return viewport_size_;
}

PropertyValue*
CalculateComputedStyleContext::GetBorderOrOutlineStyleBasedOnWidth(
    PropertyKey key) {
  if (key == kBorderBottomWidthProperty) {
    return GetBorderBottomStyle();
  } else if (key == kBorderLeftWidthProperty) {
    return GetBorderLeftStyle();
  } else if (key == kBorderRightWidthProperty) {
    return GetBorderRightStyle();
  } else if (key == kBorderTopWidthProperty) {
    return GetBorderTopStyle();
  } else {
    DCHECK_EQ(key, kOutlineWidthProperty);
    return GetOutlineStyle();
  }
}

PropertyValue* CalculateComputedStyleContext::GetBorderBottomStyle() {
  if (!computed_border_bottom_style_) {
    ComputeValue(kBorderBottomStyleProperty);
  }

  DCHECK(computed_border_bottom_style_);
  return computed_border_bottom_style_.get();
}

PropertyValue* CalculateComputedStyleContext::GetBorderLeftStyle() {
  if (!computed_border_left_style_) {
    ComputeValue(kBorderLeftStyleProperty);
  }

  DCHECK(computed_border_left_style_);
  return computed_border_left_style_.get();
}

PropertyValue* CalculateComputedStyleContext::GetBorderRightStyle() {
  if (!computed_border_right_style_) {
    ComputeValue(kBorderRightStyleProperty);
  }

  DCHECK(computed_border_right_style_);
  return computed_border_right_style_.get();
}

PropertyValue* CalculateComputedStyleContext::GetBorderTopStyle() {
  if (!computed_border_top_style_) {
    ComputeValue(kBorderTopStyleProperty);
  }

  DCHECK(computed_border_top_style_);
  return computed_border_top_style_.get();
}

RGBAColorValue* CalculateComputedStyleContext::GetColor() {
  if (!computed_color_) {
    ComputeValue(kColorProperty);
  }

  DCHECK(computed_color_);
  return base::polymorphic_downcast<RGBAColorValue*>(computed_color_.get());
}

PropertyValue* CalculateComputedStyleContext::GetOutlineStyle() {
  if (!computed_outline_style_) {
    ComputeValue(kOutlineStyleProperty);
  }

  DCHECK(computed_outline_style_);
  return computed_outline_style_.get();
}

void CalculateComputedStyleContext::ComputeValue(PropertyKey key) {
  if (cascaded_style_->IsDeclared(key)) {
    scoped_refptr<PropertyValue>& cascaded_value =
        cascaded_style_->GetDeclaredPropertyValueReference(key);
    SetComputedStyleForProperty(key, &cascaded_value);
  } else {
    const scoped_refptr<PropertyValue>& computed_value =
        cascaded_style_->GetPropertyValueReference(key);
    OnComputedStyleCalculated(key, computed_value);
  }
}

bool CalculateComputedStyleContext::HandleInheritOrInitial(
    PropertyKey key, scoped_refptr<PropertyValue>* value) {
  if (*value == KeywordValue::GetInherit()) {
    // Add this property to the list of those that inherited their declared
    // value from the parent. This allows the computed style to later determine
    // if a value that was explicitly inherited from the parent is no longer
    // valid.
    cascaded_style_->AddDeclaredPropertyInheritedFromParent(key);
    *value = parent_computed_style_.GetPropertyValue(key);
    return true;
  } else if (*value == KeywordValue::GetInitial()) {
    *value = GetPropertyInitialValue(key);
    // If the initial value is current color, it still requires to do further
    // processing.
    return *value == KeywordValue::GetCurrentColor() ? false : true;
  } else {
    return false;
  }
}

void CalculateComputedStyleContext::HandleSpecifiedValue(
    PropertyKey key, scoped_refptr<PropertyValue>* value) {
  switch (key) {
    case kBackgroundPositionProperty: {
      ComputedBackgroundPositionProvider background_position_provider(
          GetFontSize(), GetRootFontSize(), GetViewportSizeOnePercent());
      (*value)->Accept(&background_position_provider);
      const scoped_refptr<PropertyValue>& computed_background_position =
          background_position_provider.computed_background_position();
      if (computed_background_position) {
        *value = computed_background_position;
      }
    } break;
    case kBorderBottomColorProperty:
    case kBorderLeftColorProperty:
    case kBorderRightColorProperty:
    case kBorderTopColorProperty:
    case kOutlineColorProperty:
    case kTextDecorationColorProperty: {
      if (*value == KeywordValue::GetCurrentColor()) {
        // The computed value of the 'currentColor' keyword is the computed
        // value of the 'color' property.
        *value = GetColor();
      }
    } break;
    case kBorderBottomWidthProperty:
    case kBorderLeftWidthProperty:
    case kBorderRightWidthProperty:
    case kBorderTopWidthProperty:
    case kOutlineWidthProperty: {
      ComputedBorderWidthProvider border_width_provider(
          GetFontSize(), GetRootFontSize(), GetViewportSizeOnePercent(),
          GetBorderOrOutlineStyleBasedOnWidth(key));
      (*value)->Accept(&border_width_provider);
      *value = border_width_provider.computed_border_width();
    } break;
    case kBoxShadowProperty:
    case kTextShadowProperty: {
      ComputedShadowProvider shadow_provider(GetFontSize(), GetRootFontSize(),
                                             GetViewportSizeOnePercent(),
                                             GetColor());
      (*value)->Accept(&shadow_provider);
      *value = shadow_provider.computed_shadow();
    } break;
    case kDisplayProperty: {
      // According to https://www.w3.org/TR/CSS21/visuren.html#dis-pos-flo,
      // "inline" and "inline-block" values of "display" become "block" if
      // "position" is "absolute" or "fixed".
      // TODO: Modify this logic so that the original display value is
      // not lost. Being unable to determine the original value breaks static
      // positioning of "inline" and "inline-block" values with absolute
      // positioning, because they are treated as block boxes but are supposed
      // to placed at the position they would be in the normal flow.
      // https://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-width
      if ((*value == KeywordValue::GetInline() ||
           *value == KeywordValue::GetInlineBlock()) &&
          IsAbsolutelyPositioned()) {
        *value = KeywordValue::GetBlock();
      }
    } break;
    case kFontSizeProperty: {
      // Only compute this if computed_font_size_ isn't set, otherwise that
      // is an indication that it was previously computed as a dependency for
      // another property value computation.
      if (!computed_font_size_) {
        ComputedFontSizeProvider font_size_provider(
            base::polymorphic_downcast<LengthValue*>(
                parent_computed_style_.font_size().get()),
            GetRootFontSize(), GetViewportSizeOnePercent());
        (*value)->Accept(&font_size_provider);
        if (font_size_provider.computed_font_size()) {
          *value = font_size_provider.computed_font_size();
        }
      }
    } break;
    case kFontWeightProperty: {
      ComputedFontWeightProvider font_weight_provider;
      (*value)->Accept(&font_weight_provider);
      *value = font_weight_provider.computed_font_weight();
    } break;
    case kHeightProperty: {
      ComputedHeightProvider height_provider(
          parent_computed_style_.height().get(),
          parent_computed_style_.top().get(),
          parent_computed_style_.bottom().get(), GetFontSize(),
          GetRootFontSize(), GetViewportSizeOnePercent(),
          IsAbsolutelyPositioned());
      (*value)->Accept(&height_provider);
      *value = height_provider.computed_height();
    } break;
    case kLineHeightProperty: {
      ComputedLineHeightProvider line_height_provider(
          GetFontSize(), GetRootFontSize(), GetViewportSizeOnePercent());
      (*value)->Accept(&line_height_provider);
      *value = line_height_provider.computed_line_height();
    } break;
    case kMarginBottomProperty:
    case kMarginLeftProperty:
    case kMarginRightProperty:
    case kMarginTopProperty: {
      ComputedMarginOrPaddingEdgeProvider margin_provider(
          GetFontSize(), GetRootFontSize(), GetViewportSizeOnePercent());
      (*value)->Accept(&margin_provider);
      *value = margin_provider.computed_margin_or_padding_edge();
    } break;
    case kPaddingBottomProperty:
    case kPaddingLeftProperty:
    case kPaddingRightProperty:
    case kPaddingTopProperty: {
      ComputedMarginOrPaddingEdgeProvider padding_provider(
          GetFontSize(), GetRootFontSize(), GetViewportSizeOnePercent());
      (*value)->Accept(&padding_provider);
      *value = padding_provider.computed_margin_or_padding_edge();
    } break;
    case kMaxHeightProperty: {
      ComputedMaxHeightProvider max_height_provider(
          parent_computed_style_.height().get(), GetFontSize(),
          GetRootFontSize(), GetViewportSizeOnePercent(),
          IsAbsolutelyPositioned());
      (*value)->Accept(&max_height_provider);
      *value = max_height_provider.computed_max_height();
    } break;
    case kMinHeightProperty: {
      ComputedMinHeightProvider min_height_provider(
          parent_computed_style_.height().get(), GetFontSize(),
          GetRootFontSize(), GetViewportSizeOnePercent(),
          IsAbsolutelyPositioned());
      (*value)->Accept(&min_height_provider);
      *value = min_height_provider.computed_min_height();
    } break;
    case kMaxWidthProperty:
    case kMinWidthProperty: {
      ComputedMinMaxWidthProvider min_max_width_provider(
          parent_computed_style_.width().get(), GetFontSize(),
          GetRootFontSize(), GetViewportSizeOnePercent());
      (*value)->Accept(&min_max_width_provider);
      *value = min_max_width_provider.computed_min_max_width();
    } break;
    case kWidthProperty: {
      ComputedWidthProvider width_provider(GetFontSize(), GetRootFontSize(),
                                           GetViewportSizeOnePercent());
      (*value)->Accept(&width_provider);
      *value = width_provider.computed_width();
    } break;
    case kBackgroundImageProperty: {
      if (property_key_to_base_url_map_) {
        ComputedBackgroundImageProvider background_image_provider(
            (*property_key_to_base_url_map_)[kBackgroundImageProperty],
            GetFontSize(), GetRootFontSize(), GetViewportSizeOnePercent());
        (*value)->Accept(&background_image_provider);
        *value = background_image_provider.computed_background_image();
      }
    } break;
    case kBackgroundSizeProperty: {
      ComputedBackgroundSizeProvider background_size_provider(
          GetFontSize(), GetRootFontSize(), GetViewportSizeOnePercent());
      (*value)->Accept(&background_size_provider);
      *value = background_size_provider.computed_background_size();
    } break;
    case kBorderBottomLeftRadiusProperty:
    case kBorderBottomRightRadiusProperty:
    case kBorderTopLeftRadiusProperty:
    case kBorderTopRightRadiusProperty: {
      ComputedBorderRadiusProvider border_radius_provider(
          GetFontSize(), GetRootFontSize(), GetViewportSizeOnePercent());
      (*value)->Accept(&border_radius_provider);
      *value = border_radius_provider.computed_border_radius();
    } break;
    case kTextIndentProperty: {
      ComputedTextIndentProvider text_indent_provider(
          GetFontSize(), GetRootFontSize(), GetViewportSizeOnePercent());
      (*value)->Accept(&text_indent_provider);
      *value = text_indent_provider.computed_text_indent();
    } break;
    case kTransformOriginProperty: {
      ComputedTransformOriginProvider transform_origin_provider(
          GetFontSize(), GetRootFontSize(), GetViewportSizeOnePercent());
      (*value)->Accept(&transform_origin_provider);
      *value = transform_origin_provider.computed_transform_origin();
    } break;
    case kTransformProperty: {
      ComputedTransformProvider transform_provider(
          GetFontSize(), GetRootFontSize(), GetViewportSizeOnePercent());
      (*value)->Accept(&transform_provider);
      *value = transform_provider.computed_transform_list();
    } break;
    case kBottomProperty:
    case kLeftProperty:
    case kRightProperty:
    case kTopProperty: {
      ComputedPositionOffsetProvider position_offset_provider(
          GetFontSize(), GetRootFontSize(), GetViewportSizeOnePercent());
      (*value)->Accept(&position_offset_provider);
      *value = position_offset_provider.computed_position_offset();
    } break;
    case kAnimationDelayProperty:
    case kAnimationDirectionProperty:
    case kAnimationDurationProperty:
    case kAnimationFillModeProperty:
    case kAnimationIterationCountProperty:
    case kAnimationNameProperty:
    case kAnimationTimingFunctionProperty:
    case kBackgroundColorProperty:
    case kBackgroundRepeatProperty:
    case kBorderBottomStyleProperty:
    case kBorderLeftStyleProperty:
    case kBorderRightStyleProperty:
    case kBorderTopStyleProperty:
    case kColorProperty:
    case kContentProperty:
    case kFilterProperty:
    case kFontFamilyProperty:
    case kFontStyleProperty:
    case kOpacityProperty:
    case kOutlineStyleProperty:
    case kOverflowProperty:
    case kOverflowWrapProperty:
    case kPointerEventsProperty:
    case kPositionProperty:
    case kTextAlignProperty:
    case kTextDecorationLineProperty:
    case kTextOverflowProperty:
    case kTextTransformProperty:
    case kTransitionDelayProperty:
    case kTransitionDurationProperty:
    case kTransitionPropertyProperty:
    case kTransitionTimingFunctionProperty:
    case kVerticalAlignProperty:
    case kVisibilityProperty:
    case kWhiteSpaceProperty:
    case kWordWrapProperty:
    case kZIndexProperty:
      // Nothing.
      break;
    case kNoneProperty:
    case kAllProperty:
    case kAnimationProperty:
    case kBackgroundProperty:
    case kBorderBottomProperty:
    case kBorderColorProperty:
    case kBorderLeftProperty:
    case kBorderProperty:
    case kBorderRadiusProperty:
    case kBorderRightProperty:
    case kBorderStyleProperty:
    case kBorderTopProperty:
    case kBorderWidthProperty:
    case kFontProperty:
    case kMarginProperty:
    case kOutlineProperty:
    case kPaddingProperty:
    case kSrcProperty:
    case kTextDecorationProperty:
    case kTransitionProperty:
    case kUnicodeRangeProperty:
      NOTREACHED();
      break;
  }
}

void CalculateComputedStyleContext::OnComputedStyleCalculated(
    PropertyKey key, const scoped_refptr<PropertyValue>& value) {
  switch (key) {
    case kFontSizeProperty:
      computed_font_size_ = value;
      break;
    case kPositionProperty:
      computed_position_ = value;
      break;
    case kBorderBottomStyleProperty:
      computed_border_bottom_style_ = value;
      break;
    case kBorderLeftStyleProperty:
      computed_border_left_style_ = value;
      break;
    case kBorderRightStyleProperty:
      computed_border_right_style_ = value;
      break;
    case kBorderTopStyleProperty:
      computed_border_top_style_ = value;
      break;
    case kColorProperty:
      computed_color_ = value;
      break;
    case kOutlineStyleProperty:
      computed_outline_style_ = value;
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
    case kBorderBottomProperty:
    case kBorderBottomColorProperty:
    case kBorderBottomLeftRadiusProperty:
    case kBorderBottomRightRadiusProperty:
    case kBorderBottomWidthProperty:
    case kBorderColorProperty:
    case kBorderLeftProperty:
    case kBorderLeftColorProperty:
    case kBorderLeftWidthProperty:
    case kBorderRadiusProperty:
    case kBorderRightProperty:
    case kBorderRightColorProperty:
    case kBorderRightWidthProperty:
    case kBorderStyleProperty:
    case kBorderTopProperty:
    case kBorderTopColorProperty:
    case kBorderTopLeftRadiusProperty:
    case kBorderTopRightRadiusProperty:
    case kBorderTopWidthProperty:
    case kBorderProperty:
    case kBorderWidthProperty:
    case kBottomProperty:
    case kBoxShadowProperty:
    case kContentProperty:
    case kDisplayProperty:
    case kFilterProperty:
    case kFontFamilyProperty:
    case kFontProperty:
    case kFontStyleProperty:
    case kFontWeightProperty:
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
    case kOutlineProperty:
    case kOutlineColorProperty:
    case kOutlineWidthProperty:
    case kOverflowProperty:
    case kOverflowWrapProperty:
    case kPaddingBottomProperty:
    case kPaddingLeftProperty:
    case kPaddingProperty:
    case kPaddingRightProperty:
    case kPaddingTopProperty:
    case kPointerEventsProperty:
    case kRightProperty:
    case kSrcProperty:
    case kTextAlignProperty:
    case kTextDecorationColorProperty:
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
}

}  // namespace

void PromoteToComputedStyle(
    const scoped_refptr<CSSComputedStyleData>& cascaded_style,
    const scoped_refptr<CSSComputedStyleDeclaration>&
        parent_computed_style_declaration,
    const scoped_refptr<const CSSComputedStyleData>& root_computed_style,
    const math::Size& viewport_size,
    GURLMap* const property_key_to_base_url_map) {
  DCHECK(cascaded_style);
  DCHECK(parent_computed_style_declaration);
  DCHECK(root_computed_style);

  // Create a context for calculating the computed style.  This object is useful
  // because it can cache computed style values that are depended upon by other
  // properties' computed style calculations.
  CalculateComputedStyleContext calculate_computed_style_context(
      cascaded_style.get(), parent_computed_style_declaration,
      root_computed_style, viewport_size, property_key_to_base_url_map);

  // For each inherited, animatable property, set the property value to
  // inherited if it is not already declared. This causes the value to be
  // explicitly set within the CSSComputedStyleData and ensures that the
  // original value will be available for transitions (which need to know the
  // before and after state of the property) even when the property is inherited
  // from a parent that has changed.
  const PropertyKeyVector& inherited_animatable_properties =
      GetInheritedAnimatableProperties();
  for (PropertyKeyVector::const_iterator iter =
           inherited_animatable_properties.begin();
       iter != inherited_animatable_properties.end(); ++iter) {
    if (!cascaded_style->IsDeclared(*iter)) {
      cascaded_style->SetPropertyValue(*iter, KeywordValue::GetInherit());
    }
  }

  // Go through all declared values and calculate their computed values.
  CSSComputedStyleData::PropertyValues* declared_property_values =
      cascaded_style->declared_property_values();
  for (CSSComputedStyleData::PropertyValues::iterator property_value_iterator =
           declared_property_values->begin();
       property_value_iterator != declared_property_values->end();
       ++property_value_iterator) {
    calculate_computed_style_context.SetComputedStyleForProperty(
        property_value_iterator->first, &property_value_iterator->second);
  }
}

scoped_refptr<CSSComputedStyleData> GetComputedStyleOfAnonymousBox(
    const scoped_refptr<CSSComputedStyleDeclaration>&
        parent_computed_style_declaration) {
  scoped_refptr<CSSComputedStyleData> computed_style =
      new CSSComputedStyleData();
  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style_declaration->data(),
                         math::Size(), NULL);
  return computed_style;
}

}  // namespace cssom
}  // namespace cobalt
