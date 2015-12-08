/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/cssom/computed_style.h"

#include "base/memory/scoped_ptr.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/cssom/absolute_url_value.h"
#include "cobalt/cssom/calc_value.h"
#include "cobalt/cssom/css_style_declaration_data.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/linear_gradient_value.h"
#include "cobalt/cssom/matrix_function.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/property_value_visitor.h"
#include "cobalt/cssom/rotate_function.h"
#include "cobalt/cssom/scale_function.h"
#include "cobalt/cssom/specified_style.h"
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
    const LengthValue* computed_font_size) {
  switch (specified_length->unit()) {
    // "px" is an absolute unit.
    //   http://www.w3.org/TR/css3-values/#absolute-lengths
    case kPixelsUnit:
      return scoped_refptr<LengthValue>(specified_length);

    // "em" equals to the computed value of the "font-size" property of
    // the element on which it is used.
    //   http://www.w3.org/TR/css3-values/#font-relative-lengths
    case kFontSizesAkaEmUnit: {
      DCHECK_EQ(kPixelsUnit, computed_font_size->unit());

      return new LengthValue(
          computed_font_size->value() * specified_length->value(), kPixelsUnit);
    }

    default:
      NOTREACHED();
      return NULL;
  }
}

// Computed value: absolute length;
//                 '0' if the border style is 'none' or 'hidden'.
//   http://www.w3.org/TR/css3-background/#border-width
class ComputedBorderWidthProvider : public NotReachedPropertyValueVisitor {
 public:
  explicit ComputedBorderWidthProvider(const LengthValue* computed_font_size,
                                       const PropertyValue* border_style);

  void VisitLength(LengthValue* length) OVERRIDE;

  const scoped_refptr<PropertyValue>& computed_border_width() const {
    return computed_border_width_;
  }

 private:
  const LengthValue* computed_font_size_;
  const PropertyValue* border_style_;

  scoped_refptr<PropertyValue> computed_border_width_;

  DISALLOW_COPY_AND_ASSIGN(ComputedBorderWidthProvider);
};

ComputedBorderWidthProvider::ComputedBorderWidthProvider(
    const LengthValue* computed_font_size, const PropertyValue* border_style)
    : computed_font_size_(computed_font_size), border_style_(border_style) {}

void ComputedBorderWidthProvider::VisitLength(LengthValue* specified_length) {
  if (border_style_ == cssom::KeywordValue::GetNone().get() ||
      border_style_ == cssom::KeywordValue::GetHidden().get()) {
    computed_border_width_ = new LengthValue(0, cssom::kPixelsUnit);
  } else {
    DCHECK_EQ(border_style_, cssom::KeywordValue::GetSolid().get());
    computed_border_width_ =
        ProvideAbsoluteLength(specified_length, computed_font_size_);
  }
}

// Computed value: numeric weight value.
//   http://www.w3.org/TR/css3-fonts/#font-weight-prop
class ComputedFontWeightProvider : public NotReachedPropertyValueVisitor {
 public:
  ComputedFontWeightProvider() {}

  void VisitFontWeight(FontWeightValue* weight) OVERRIDE;

  const scoped_refptr<FontWeightValue>& computed_font_weight() const {
    return computed_font_weight_;
  }

 private:
  scoped_refptr<FontWeightValue> computed_font_weight_;

  DISALLOW_COPY_AND_ASSIGN(ComputedFontWeightProvider);
};

// TODO(***REMOVED***): Support bolder and lighter. Match the weight with font face.
// Quite often there are only a few weights available for a particular font
// family. When a weight is specified for which no face exists, a face with a
// nearby weight is used.
//   http://www.w3.org/TR/css3-fonts/#font-matching-algorithm
void ComputedFontWeightProvider::VisitFontWeight(
    FontWeightValue* specified_weight) {
  computed_font_weight_ = specified_weight;
}

// Computed value: absolute length.
//   http://www.w3.org/TR/css3-fonts/#font-size-prop
class ComputedFontSizeProvider : public NotReachedPropertyValueVisitor {
 public:
  explicit ComputedFontSizeProvider(
      const LengthValue* parent_computed_font_size);

  void VisitLength(LengthValue* length) OVERRIDE;
  void VisitPercentage(PercentageValue* percentage) OVERRIDE;

  const scoped_refptr<LengthValue>& computed_font_size() const {
    return computed_font_size_;
  }

 private:
  const LengthValue* parent_computed_font_size_;

  scoped_refptr<LengthValue> computed_font_size_;

  DISALLOW_COPY_AND_ASSIGN(ComputedFontSizeProvider);
};

ComputedFontSizeProvider::ComputedFontSizeProvider(
    const LengthValue* parent_computed_font_size)
    : parent_computed_font_size_(parent_computed_font_size) {}

void ComputedFontSizeProvider::VisitLength(LengthValue* specified_length) {
  // "em" on "font-size" is calculated relatively to the inherited value
  // of "font-size".
  //   http://www.w3.org/TR/css3-values/#font-relative-lengths
  computed_font_size_ =
      ProvideAbsoluteLength(specified_length, parent_computed_font_size_);
}

void ComputedFontSizeProvider::VisitPercentage(
    PercentageValue* specified_percentage) {
  // A percentage value specifies an absolute font size relative to the parent
  // element's fonts size.
  //   http://www.w3.org/TR/css3-fonts/#percentage-size-value
  computed_font_size_ = new LengthValue(
      parent_computed_font_size_->value() * specified_percentage->value(),
      kPixelsUnit);
}

// Computed value: for length and percentage the absolute value;
//                 otherwise as specified.
//   http://www.w3.org/TR/CSS21/visudet.html#line-height
class ComputedLineHeightProvider : public NotReachedPropertyValueVisitor {
 public:
  explicit ComputedLineHeightProvider(const LengthValue* computed_font_size);

  void VisitKeyword(KeywordValue* keyword) OVERRIDE;
  void VisitLength(LengthValue* length) OVERRIDE;

  const scoped_refptr<PropertyValue>& computed_line_height() const {
    return computed_line_height_;
  }

 private:
  const LengthValue* computed_font_size_;

  scoped_refptr<PropertyValue> computed_line_height_;

  DISALLOW_COPY_AND_ASSIGN(ComputedLineHeightProvider);
};

ComputedLineHeightProvider::ComputedLineHeightProvider(
    const LengthValue* computed_font_size)
    : computed_font_size_(computed_font_size) {}

void ComputedLineHeightProvider::VisitLength(LengthValue* specified_length) {
  computed_line_height_ =
      ProvideAbsoluteLength(specified_length, computed_font_size_);
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
    case KeywordValue::kMiddle:
    case KeywordValue::kMonospace:
    case KeywordValue::kNone:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStatic:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
    default:
      NOTREACHED();
  }
}

// Computed value: the percentage as specified or the absolute length.
//   http://www.w3.org/TR/CSS21/box.html#margin-properties
//   http://www.w3.org/TR/CSS21/box.html#padding-properties
class ComputedMarginOrPaddingEdgeProvider
    : public NotReachedPropertyValueVisitor {
 public:
  explicit ComputedMarginOrPaddingEdgeProvider(
      const LengthValue* computed_font_size);

  void VisitKeyword(KeywordValue* keyword) OVERRIDE;
  void VisitLength(LengthValue* length) OVERRIDE;
  void VisitPercentage(PercentageValue* percentage) OVERRIDE;

  const scoped_refptr<PropertyValue>& computed_margin_or_padding_edge() const {
    return computed_margin_or_padding_edge_;
  }

 private:
  const LengthValue* computed_font_size_;

  scoped_refptr<PropertyValue> computed_margin_or_padding_edge_;

  DISALLOW_COPY_AND_ASSIGN(ComputedMarginOrPaddingEdgeProvider);
};

ComputedMarginOrPaddingEdgeProvider::ComputedMarginOrPaddingEdgeProvider(
    const LengthValue* computed_font_size)
    : computed_font_size_(computed_font_size) {}

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
    case KeywordValue::kMiddle:
    case KeywordValue::kMonospace:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNone:
    case KeywordValue::kNormal:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStatic:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
    default:
      NOTREACHED();
  }
}

void ComputedMarginOrPaddingEdgeProvider::VisitLength(
    LengthValue* specified_length) {
  computed_margin_or_padding_edge_ =
      ProvideAbsoluteLength(specified_length, computed_font_size_);
}

void ComputedMarginOrPaddingEdgeProvider::VisitPercentage(
    PercentageValue* percentage) {
  computed_margin_or_padding_edge_ = percentage;
}

class ComputedPositionOffsetProvider : public NotReachedPropertyValueVisitor {
 public:
  explicit ComputedPositionOffsetProvider(
      const LengthValue* computed_font_size);

  void VisitKeyword(KeywordValue* keyword) OVERRIDE;
  void VisitLength(LengthValue* length) OVERRIDE;
  void VisitPercentage(PercentageValue* percentage) OVERRIDE;

  const scoped_refptr<PropertyValue>& computed_position_offset() const {
    return computed_position_offset_;
  }

 private:
  const LengthValue* computed_font_size_;

  scoped_refptr<PropertyValue> computed_position_offset_;

  DISALLOW_COPY_AND_ASSIGN(ComputedPositionOffsetProvider);
};

void ComputedPositionOffsetProvider::VisitLength(
    LengthValue* specified_length) {
  computed_position_offset_ =
      ProvideAbsoluteLength(specified_length, computed_font_size_);
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
    case KeywordValue::kMiddle:
    case KeywordValue::kMonospace:
    case KeywordValue::kNone:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNormal:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStatic:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
    default:
      NOTREACHED();
  }
}

void ComputedPositionOffsetProvider::VisitPercentage(
    PercentageValue* percentage) {
  computed_position_offset_ = percentage;
}

ComputedPositionOffsetProvider::ComputedPositionOffsetProvider(
    const LengthValue* computed_font_size)
    : computed_font_size_(computed_font_size) {}

// Computed value: the percentage or "auto" or the absolute length.
//   http://www.w3.org/TR/CSS21/visudet.html#the-height-property
class ComputedHeightProvider : public NotReachedPropertyValueVisitor {
 public:
  ComputedHeightProvider(const PropertyValue* parent_computed_height,
                         const LengthValue* computed_font_size,
                         bool out_of_flow);

  void VisitKeyword(KeywordValue* keyword) OVERRIDE;
  void VisitLength(LengthValue* length) OVERRIDE;
  void VisitPercentage(PercentageValue* percentage) OVERRIDE;

  const scoped_refptr<PropertyValue>& computed_height() const {
    return computed_height_;
  }

 private:
  const PropertyValue* parent_computed_height_;
  const LengthValue* computed_font_size_;
  bool out_of_flow_;

  scoped_refptr<PropertyValue> computed_height_;

  DISALLOW_COPY_AND_ASSIGN(ComputedHeightProvider);
};

ComputedHeightProvider::ComputedHeightProvider(
    const PropertyValue* parent_computed_height,
    const LengthValue* computed_font_size, bool out_of_flow)
    : parent_computed_height_(parent_computed_height),
      computed_font_size_(computed_font_size),
      out_of_flow_(out_of_flow) {}

void ComputedHeightProvider::VisitLength(LengthValue* specified_length) {
  computed_height_ =
      ProvideAbsoluteLength(specified_length, computed_font_size_);
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
    case KeywordValue::kMiddle:
    case KeywordValue::kMonospace:
    case KeywordValue::kNone:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNormal:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStatic:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
    default:
      NOTREACHED();
  }
}

void ComputedHeightProvider::VisitPercentage(PercentageValue* percentage) {
  const scoped_refptr<PropertyValue>& auto_value = KeywordValue::GetAuto();

  // If the height of the containing block is not specified explicitly
  // (i.e., it depends on content height), and this element is not absolutely
  // positioned, the value computes to "auto".
  //   http://www.w3.org/TR/CSS21/visudet.html#the-height-property
  computed_height_ = (parent_computed_height_ == auto_value && !out_of_flow_)
                         ? auto_value
                         : percentage;
}

// Computed value: the percentage or "auto" or the absolute length.
//   http://www.w3.org/TR/CSS2/visudet.html#propdef-max-height
class ComputedMaxHeightProvider : public NotReachedPropertyValueVisitor {
 public:
  ComputedMaxHeightProvider(const PropertyValue* parent_computed_max_height,
                            const LengthValue* computed_font_size,
                            bool out_of_flow);

  void VisitKeyword(KeywordValue* keyword) OVERRIDE;
  void VisitLength(LengthValue* length) OVERRIDE;
  void VisitPercentage(PercentageValue* percentage) OVERRIDE;

  const scoped_refptr<PropertyValue>& computed_max_height() const {
    return computed_max_height_;
  }

 private:
  const PropertyValue* parent_computed_max_height_;
  const LengthValue* computed_font_size_;
  bool out_of_flow_;

  scoped_refptr<PropertyValue> computed_max_height_;

  DISALLOW_COPY_AND_ASSIGN(ComputedMaxHeightProvider);
};

ComputedMaxHeightProvider::ComputedMaxHeightProvider(
    const PropertyValue* parent_computed_max_height,
    const LengthValue* computed_font_size, bool out_of_flow)
    : parent_computed_max_height_(parent_computed_max_height),
      computed_font_size_(computed_font_size),
      out_of_flow_(out_of_flow) {}

void ComputedMaxHeightProvider::VisitLength(LengthValue* specified_length) {
  computed_max_height_ =
      ProvideAbsoluteLength(specified_length, computed_font_size_);
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
    case KeywordValue::kMiddle:
    case KeywordValue::kMonospace:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNormal:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStatic:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
    default:
      NOTREACHED();
  }
}

void ComputedMaxHeightProvider::VisitPercentage(PercentageValue* percentage) {
  const scoped_refptr<PropertyValue>& auto_value = KeywordValue::GetAuto();
  const scoped_refptr<PropertyValue>& none_value = KeywordValue::GetNone();

  // If the max_height of the containing block is not specified explicitly
  // (i.e., it depends on content max_height), and this element is not
  // absolutely positioned, the percentage value is treated as 'none'.
  //   http://www.w3.org/TR/CSS2/visudet.html#propdef-max-height
  computed_max_height_ =
      (parent_computed_max_height_ == auto_value && !out_of_flow_) ? none_value
                                                                   : percentage;
}

// Computed value: the percentage or "auto" or the absolute length.
//   http://www.w3.org/TR/CSS2/visudet.html#propdef-min-height
class ComputedMinHeightProvider : public NotReachedPropertyValueVisitor {
 public:
  ComputedMinHeightProvider(const PropertyValue* parent_computed_min_max_height,
                            const LengthValue* computed_font_size,
                            bool out_of_flow);

  void VisitKeyword(KeywordValue* keyword) OVERRIDE;
  void VisitLength(LengthValue* length) OVERRIDE;
  void VisitPercentage(PercentageValue* percentage) OVERRIDE;

  const scoped_refptr<PropertyValue>& computed_min_height() const {
    return computed_min_height_;
  }

 private:
  const PropertyValue* parent_computed_min_max_height_;
  const LengthValue* computed_font_size_;
  bool out_of_flow_;

  scoped_refptr<PropertyValue> computed_min_height_;

  DISALLOW_COPY_AND_ASSIGN(ComputedMinHeightProvider);
};

ComputedMinHeightProvider::ComputedMinHeightProvider(
    const PropertyValue* parent_computed_min_max_height,
    const LengthValue* computed_font_size, bool out_of_flow)
    : parent_computed_min_max_height_(parent_computed_min_max_height),
      computed_font_size_(computed_font_size),
      out_of_flow_(out_of_flow) {}

void ComputedMinHeightProvider::VisitLength(LengthValue* specified_length) {
  computed_min_height_ =
      ProvideAbsoluteLength(specified_length, computed_font_size_);
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
    case KeywordValue::kMiddle:
    case KeywordValue::kMonospace:
    case KeywordValue::kNone:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNormal:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStatic:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
    default:
      NOTREACHED();
  }
}

void ComputedMinHeightProvider::VisitPercentage(PercentageValue* percentage) {
  const scoped_refptr<PropertyValue>& auto_value = KeywordValue::GetAuto();

  // If the min_height of the containing block is not specified explicitly
  // (i.e., it depends on content min_height), and this element is not
  // absolutely positioned, the percentage value is treated as '0'.
  //   http://www.w3.org/TR/CSS2/visudet.html#propdef-min-height
  if (parent_computed_min_max_height_ == auto_value && !out_of_flow_) {
    computed_min_height_ = new LengthValue(0, kPixelsUnit);
  } else {
    computed_min_height_ = percentage;
  }
}

// Computed value: the percentage or "auto" as specified or the absolute length.
//   http://www.w3.org/TR/CSS21/visudet.html#the-width-property
class ComputedWidthProvider : public NotReachedPropertyValueVisitor {
 public:
  explicit ComputedWidthProvider(const LengthValue* computed_font_size);

  void VisitKeyword(KeywordValue* keyword) OVERRIDE;
  void VisitLength(LengthValue* length) OVERRIDE;
  void VisitPercentage(PercentageValue* percentage) OVERRIDE;

  const scoped_refptr<PropertyValue>& computed_width() const {
    return computed_width_;
  }

 private:
  const LengthValue* computed_font_size_;

  scoped_refptr<PropertyValue> computed_width_;

  DISALLOW_COPY_AND_ASSIGN(ComputedWidthProvider);
};

ComputedWidthProvider::ComputedWidthProvider(
    const LengthValue* computed_font_size)
    : computed_font_size_(computed_font_size) {}

void ComputedWidthProvider::VisitLength(LengthValue* specified_length) {
  computed_width_ =
      ProvideAbsoluteLength(specified_length, computed_font_size_);
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
    case KeywordValue::kMiddle:
    case KeywordValue::kMonospace:
    case KeywordValue::kNone:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNormal:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStatic:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
    default:
      NOTREACHED();
  }
}

void ComputedWidthProvider::VisitPercentage(PercentageValue* percentage) {
  computed_width_ = percentage;
}

// Computed value: the percentage or "auto" as specified or the absolute length.
//   http://www.w3.org/TR/CSS2/visudet.html#min-max-widths
class ComputedMinMaxWidthProvider : public NotReachedPropertyValueVisitor {
 public:
  explicit ComputedMinMaxWidthProvider(
      PropertyValue* parent_computed_min_max_height,
      const LengthValue* computed_font_size);

  void VisitKeyword(KeywordValue* keyword) OVERRIDE;
  void VisitLength(LengthValue* length) OVERRIDE;
  void VisitPercentage(PercentageValue* percentage) OVERRIDE;

  const scoped_refptr<PropertyValue>& computed_min_max_width() const {
    return computed_min_max_width_;
  }

 private:
  PropertyValue* parent_computed_min_max_width_;
  const LengthValue* computed_font_size_;

  scoped_refptr<PropertyValue> computed_min_max_width_;

  DISALLOW_COPY_AND_ASSIGN(ComputedMinMaxWidthProvider);
};

ComputedMinMaxWidthProvider::ComputedMinMaxWidthProvider(
    PropertyValue* parent_computed_min_max_width,
    const LengthValue* computed_font_size)
    : parent_computed_min_max_width_(parent_computed_min_max_width),
      computed_font_size_(computed_font_size) {}

void ComputedMinMaxWidthProvider::VisitLength(LengthValue* specified_length) {
  computed_min_max_width_ =
      ProvideAbsoluteLength(specified_length, computed_font_size_);
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
    case KeywordValue::kMiddle:
    case KeywordValue::kMonospace:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNormal:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStatic:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
    default:
      NOTREACHED();
  }
}

class ComputedLengthIsNegativeProvider : public DefaultingPropertyValueVisitor {
 public:
  ComputedLengthIsNegativeProvider() : computed_length_is_negative_(false) {}

  void VisitLength(LengthValue* length_value) OVERRIDE {
    switch (length_value->unit()) {
      case kPixelsUnit:
      case kFontSizesAkaEmUnit:
        computed_length_is_negative_ = length_value->value() < 0;
        break;
      default:
        NOTREACHED();
    }
  }

  void VisitDefault(PropertyValue* property_value) OVERRIDE {
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
  //   http://www.w3.org/TR/CSS2/visudet.html#min-max-widths
  if (computed_length_is_negative_provider.computed_length_is_negative()) {
    computed_min_max_width_ = new LengthValue(0, kPixelsUnit);
  } else {
    computed_min_max_width_ = percentage;
  }
}

// Absolutizes the value of "background-image" property.
// Computed value: as specified, but with URIs made absolute.
//   http://www.w3.org/TR/css3-background/#the-background-image
class ComputedBackgroundImageSingleLayerProvider
    : public NotReachedPropertyValueVisitor {
 public:
  explicit ComputedBackgroundImageSingleLayerProvider(const GURL& base_url)
      : base_url_(base_url) {}

  void VisitAbsoluteURL(AbsoluteURLValue* absolute_url_value) OVERRIDE;
  void VisitKeyword(KeywordValue* keyword) OVERRIDE;
  void VisitLinearGradient(LinearGradientValue* linear_gradient_value) OVERRIDE;
  void VisitURL(URLValue* url_value) OVERRIDE;

  const scoped_refptr<PropertyValue>& computed_background_image() const {
    return computed_background_image_;
  }

 private:
  const GURL base_url_;

  scoped_refptr<PropertyValue> computed_background_image_;
};

void ComputedBackgroundImageSingleLayerProvider::VisitAbsoluteURL(
    AbsoluteURLValue* absolute_url_value) {
  computed_background_image_ = absolute_url_value;
}

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
    case KeywordValue::kMiddle:
    case KeywordValue::kMonospace:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNormal:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStatic:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
    default:
      NOTREACHED();
      break;
  }
}

void ComputedBackgroundImageSingleLayerProvider::VisitLinearGradient(
    LinearGradientValue* /*linear_gradient_value*/) {
  // TODO(***REMOVED***): Deal with linear gradient value.
  NOTIMPLEMENTED();
}

void ComputedBackgroundImageSingleLayerProvider::VisitURL(URLValue* url_value) {
  GURL absolute_url;

  if (url_value->is_absolute()) {
    absolute_url = GURL(url_value->value());
  } else {
    absolute_url = url_value->Resolve(base_url_);
  }

  computed_background_image_ = new AbsoluteURLValue(absolute_url);
}

class ComputedBackgroundImageProvider : public NotReachedPropertyValueVisitor {
 public:
  explicit ComputedBackgroundImageProvider(const GURL& base_url)
      : base_url_(base_url) {}

  void VisitPropertyList(PropertyListValue* property_list_value) OVERRIDE;

  const scoped_refptr<PropertyValue>& computed_background_image() const {
    return computed_background_image_;
  }

 private:
  const GURL base_url_;

  scoped_refptr<PropertyValue> computed_background_image_;
};

void ComputedBackgroundImageProvider::VisitPropertyList(
    PropertyListValue* property_list_value) {
  scoped_ptr<PropertyListValue::Builder> builder(
      new PropertyListValue::Builder());
  builder->reserve(property_list_value->value().size());

  ComputedBackgroundImageSingleLayerProvider single_layer_provider(base_url_);
  for (size_t i = 0; i < property_list_value->value().size(); ++i) {
    property_list_value->value()[i]->Accept(&single_layer_provider);
    scoped_refptr<PropertyValue> computed_background_image =
        single_layer_provider.computed_background_image();
    if (computed_background_image) {
      builder->push_back(computed_background_image);
    }
  }

  computed_background_image_ = new PropertyListValue(builder.Pass());
}

class ComputedBackgroundSizeSingleValueProvider
    : public NotReachedPropertyValueVisitor {
 public:
  explicit ComputedBackgroundSizeSingleValueProvider(
      const LengthValue* computed_font_size);

  void VisitLength(LengthValue* length) OVERRIDE;
  void VisitPercentage(PercentageValue* percentage) OVERRIDE;
  void VisitKeyword(KeywordValue* keyword) OVERRIDE;

  const scoped_refptr<PropertyValue>& computed_background_size() const {
    return computed_background_size_;
  }

 private:
  const LengthValue* computed_font_size_;

  scoped_refptr<PropertyValue> computed_background_size_;

  DISALLOW_COPY_AND_ASSIGN(ComputedBackgroundSizeSingleValueProvider);
};

ComputedBackgroundSizeSingleValueProvider::
    ComputedBackgroundSizeSingleValueProvider(
        const LengthValue* computed_font_size)
    : computed_font_size_(computed_font_size) {}

void ComputedBackgroundSizeSingleValueProvider::VisitLength(
    LengthValue* length) {
  computed_background_size_ =
      ProvideAbsoluteLength(length, computed_font_size_);
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
    case KeywordValue::kMiddle:
    case KeywordValue::kMonospace:
    case KeywordValue::kNone:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNormal:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStatic:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
    default:
      NOTREACHED();
  }
}

//   http://www.w3.org/TR/css3-background/#the-background-position
class ComputedBackgroundPositionProvider
    : public NotReachedPropertyValueVisitor {
 public:
  explicit ComputedBackgroundPositionProvider(
      const LengthValue* computed_font_size);

  void VisitPropertyList(PropertyListValue* property_list_value) OVERRIDE;

  const scoped_refptr<PropertyValue>& computed_background_position() const {
    return computed_background_position_;
  }

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

  void BackgroundPositionBuilderProviderForSize2(
      PropertyListValue* property_list_value,
      PropertyListValue::Builder* position_builder);

  void BackgroundPositionBuilderProviderForMoreThanSize2(
      PropertyListValue* property_list_value,
      PropertyListValue::Builder* position_builder);

  const LengthValue* computed_font_size_;

  scoped_refptr<PropertyValue> computed_background_position_;

  DISALLOW_COPY_AND_ASSIGN(ComputedBackgroundPositionProvider);
};

ComputedBackgroundPositionProvider::ComputedBackgroundPositionProvider(
    const LengthValue* computed_font_size)
    : computed_font_size_(computed_font_size) {}

void ComputedBackgroundPositionProvider::VisitPropertyList(
    PropertyListValue* property_list_value) {
  size_t size = property_list_value->value().size();
  // We have already filled the default value 'center' if only one value is
  // specified when we parse the grammar.
  DCHECK_GE(size, static_cast<size_t>(2));
  DCHECK_LE(size, static_cast<size_t>(4));

  scoped_ptr<PropertyListValue::Builder> background_position_builder(
      new cssom::PropertyListValue::Builder(2, scoped_refptr<PropertyValue>()));

  if (size == 2) {
    BackgroundPositionBuilderProviderForSize2(
        property_list_value, background_position_builder.get());
  } else {
    BackgroundPositionBuilderProviderForMoreThanSize2(
        property_list_value, background_position_builder.get());
  }

  computed_background_position_ =
      new cssom::PropertyListValue(background_position_builder.Pass());
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
//   http://www.w3.org/TR/css3-background/#the-background-position
const ComputedBackgroundPositionProvider::OriginInfo
ComputedBackgroundPositionProvider::ConvertToOriginInfo(
    const scoped_refptr<PropertyValue>& keyword) const {
  DCHECK(keyword->GetTypeId() == base::GetTypeId<KeywordValue>());

  if (keyword == cssom::KeywordValue::GetLeft()) {
    return OriginInfo(0.0f, 1, kHorizontal);
  } else if (keyword == cssom::KeywordValue::GetRight()) {
    return OriginInfo(1.0f, -1, kHorizontal);
  } else if (keyword == cssom::KeywordValue::GetTop()) {
    return OriginInfo(0.0f, 1, kVertical);
  } else if (keyword == cssom::KeywordValue::GetBottom()) {
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
ComputedBackgroundPositionProvider::ProvideCalcValueFromOriginAndOffset(
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
        computed_font_size_);
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

void ComputedBackgroundPositionProvider::FillPositionBuilderFromOriginAndOffset(
    const scoped_refptr<PropertyValue>& origin,
    const scoped_refptr<PropertyValue>& offset,
    PropertyListValue::Builder* position_builder) {
  DCHECK(origin->GetTypeId() == base::GetTypeId<KeywordValue>());

  OriginInfo origin_info = ConvertToOriginInfo(origin);
  switch (origin_info.direction) {
    case kHorizontal: {
      (*position_builder)[0] =
          ProvideCalcValueFromOriginAndOffset(&origin_info, offset);
      break;
    }
    case kVertical: {
      (*position_builder)[1] =
          ProvideCalcValueFromOriginAndOffset(&origin_info, offset);
      break;
    }
    case kCenter: {
      if (!(*position_builder)[0]) {
        (*position_builder)[0] =
            ProvideCalcValueFromOriginAndOffset(&origin_info, offset);
      }
      if (!(*position_builder)[1]) {
        (*position_builder)[1] =
            ProvideCalcValueFromOriginAndOffset(&origin_info, offset);
      }
      break;
    }
    case kNone:  // fall-through
    default:
      NOTREACHED();
      break;
  }
}

// If two background position values are given, a length or percentage as the
// first value represents the horizontal position (or offset) and a length or
// percentage as the second value represents the vertical position (or offset).
// <percentage> and <length> values here represent an offset of the top left
// corner of the background image from the top left corner of the background
// positioning area.
void ComputedBackgroundPositionProvider::
    BackgroundPositionBuilderProviderForSize2(
        PropertyListValue* property_list_value,
        cssom::PropertyListValue::Builder* position_builder) {
  DCHECK_EQ(2, property_list_value->value().size());

  for (size_t i = 0; i < 2; ++i) {
    scoped_refptr<PropertyValue> current_value =
        property_list_value->value()[i];

    if (current_value->GetTypeId() == base::GetTypeId<CalcValue>()) {
      // Inherited value could be a CalcValue type and it is already resolved.
      (*position_builder)[i] = current_value;
    } else if (current_value->GetTypeId() == base::GetTypeId<KeywordValue>()) {
      FillPositionBuilderFromOriginAndOffset(current_value, NULL,
                                             position_builder);
    } else {
      OriginInfo default_origin = OriginInfo(0.0f, 1, kNone);
      (*position_builder)[i] =
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
void ComputedBackgroundPositionProvider::
    BackgroundPositionBuilderProviderForMoreThanSize2(
        PropertyListValue* property_list_value,
        PropertyListValue::Builder* position_builder) {
  for (size_t i = 0; i < property_list_value->value().size(); ++i) {
    scoped_refptr<PropertyValue> previous_value =
        (i == 0) ? NULL : property_list_value->value()[i - 1];

    scoped_refptr<PropertyValue> current_value =
        property_list_value->value()[i];

    if (current_value->GetTypeId() == base::GetTypeId<KeywordValue>()) {
      FillPositionBuilderFromOriginAndOffset(current_value, NULL,
                                             position_builder);
    } else {
      DCHECK(previous_value);
      DCHECK(previous_value->GetTypeId() == base::GetTypeId<KeywordValue>());
      FillPositionBuilderFromOriginAndOffset(previous_value, current_value,
                                             position_builder);
    }
  }
}

class ComputedBackgroundSizeProvider : public NotReachedPropertyValueVisitor {
 public:
  explicit ComputedBackgroundSizeProvider(
      const LengthValue* computed_font_size);

  void VisitKeyword(KeywordValue* keyword) OVERRIDE;
  void VisitPropertyList(PropertyListValue* property_list_value) OVERRIDE;

  const scoped_refptr<PropertyValue>& computed_background_size() const {
    return computed_background_size_;
  }

 private:
  const LengthValue* computed_font_size_;

  scoped_refptr<PropertyValue> computed_background_size_;

  DISALLOW_COPY_AND_ASSIGN(ComputedBackgroundSizeProvider);
};

ComputedBackgroundSizeProvider::ComputedBackgroundSizeProvider(
    const LengthValue* computed_font_size)
    : computed_font_size_(computed_font_size) {}

void ComputedBackgroundSizeProvider::VisitKeyword(KeywordValue* keyword) {
  computed_background_size_ = keyword;
}

void ComputedBackgroundSizeProvider::VisitPropertyList(
    PropertyListValue* property_list_value) {
  ComputedBackgroundSizeSingleValueProvider left_value_provider(
      computed_font_size_);
  property_list_value->value()[0]->Accept(&left_value_provider);

  ComputedBackgroundSizeSingleValueProvider right_value_provider(
      computed_font_size_);
  property_list_value->value()[1]->Accept(&right_value_provider);

  scoped_ptr<PropertyListValue::Builder> builder(
      new PropertyListValue::Builder());
  builder->reserve(2);
  builder->push_back(left_value_provider.computed_background_size());
  builder->push_back(right_value_provider.computed_background_size());
  computed_background_size_ = new PropertyListValue(builder.Pass());
}

// Computed value: for length of translation transforms.
//   http://www.w3.org/TR/css3-transforms/#propdef-transform
class ComputedTransformFunctionProvider : public TransformFunctionVisitor {
 public:
  explicit ComputedTransformFunctionProvider(
      const LengthValue* computed_font_size);

  void VisitMatrix(const MatrixFunction* matrix_function) OVERRIDE;
  void VisitRotate(const RotateFunction* rotate_function) OVERRIDE;
  void VisitScale(const ScaleFunction* scale_function) OVERRIDE;
  void VisitTranslate(const TranslateFunction* translate_function) OVERRIDE;

  scoped_ptr<TransformFunction> PassComputedTransformFunction() {
    return computed_transform_function_.Pass();
  }

 private:
  scoped_ptr<TransformFunction> computed_transform_function_;
  const LengthValue* computed_font_size_;
};

ComputedTransformFunctionProvider::ComputedTransformFunctionProvider(
    const LengthValue* computed_font_size)
    : computed_font_size_(computed_font_size) {}

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
                                computed_font_size_)));
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
          new CalcValue(ProvideAbsoluteLength(calc_value->length_value(),
                                              computed_font_size_),
                        calc_value->percentage_value())));
    } break;
    default: { NOTREACHED(); }
  }
}

// Absolutizes the value of "text-indent" property.
class ComputedTextIndentProvider : public NotReachedPropertyValueVisitor {
 public:
  explicit ComputedTextIndentProvider(const LengthValue* computed_font_size);

  void VisitLength(LengthValue* length) OVERRIDE;

  const scoped_refptr<LengthValue>& computed_text_indent() const {
    return computed_text_indent_;
  }

 private:
  const LengthValue* computed_font_size_;

  scoped_refptr<LengthValue> computed_text_indent_;

  DISALLOW_COPY_AND_ASSIGN(ComputedTextIndentProvider);
};

ComputedTextIndentProvider::ComputedTextIndentProvider(
    const LengthValue* computed_font_size)
    : computed_font_size_(computed_font_size) {}

void ComputedTextIndentProvider::VisitLength(LengthValue* specified_length) {
  computed_text_indent_ =
      ProvideAbsoluteLength(specified_length, computed_font_size_);
}

namespace {

// Functionality to check if a transform contains any em units.  If it does not,
// then we need not modify the transform list here at all.
class TransformFunctionContainsEmsVisitor : public TransformFunctionVisitor {
 public:
  TransformFunctionContainsEmsVisitor() : contains_em_units_(false) {}

  void VisitMatrix(const MatrixFunction* matrix_function) OVERRIDE {
    UNREFERENCED_PARAMETER(matrix_function);
  }
  void VisitRotate(const RotateFunction* rotate_function) OVERRIDE {
    UNREFERENCED_PARAMETER(rotate_function);
  }
  void VisitScale(const ScaleFunction* scale_function) OVERRIDE {
    UNREFERENCED_PARAMETER(scale_function);
  }
  void VisitTranslate(const TranslateFunction* translate_function) OVERRIDE {
    contains_em_units_ =
        (translate_function->offset_type() == TranslateFunction::kLength &&
         translate_function->offset_as_length()->unit() == kFontSizesAkaEmUnit);
  }

  bool contains_em_units() const { return contains_em_units_; }

 private:
  bool contains_em_units_;
};

bool TransformListContainsEmUnits(
    TransformFunctionListValue* transform_function_list) {
  for (TransformFunctionListValue::Builder::const_iterator iter =
           transform_function_list->value().begin();
       iter != transform_function_list->value().end(); ++iter) {
    TransformFunction* transform_function = *iter;

    TransformFunctionContainsEmsVisitor contains_ems_visitor;
    transform_function->Accept(&contains_ems_visitor);
    if (contains_ems_visitor.contains_em_units()) {
      return true;
    }
  }
  return false;
}

}  // namespace

class ComputedTransformProvider : public NotReachedPropertyValueVisitor {
 public:
  explicit ComputedTransformProvider(const LengthValue* computed_font_size);

  void VisitKeyword(KeywordValue* keyword) OVERRIDE;
  void VisitTransformFunctionList(
      TransformFunctionListValue* transform_function_list) OVERRIDE;

  const scoped_refptr<PropertyValue>& computed_transform_list() const {
    return computed_transform_list_;
  }

 private:
  const LengthValue* computed_font_size_;

  scoped_refptr<PropertyValue> computed_transform_list_;

  DISALLOW_COPY_AND_ASSIGN(ComputedTransformProvider);
};

ComputedTransformProvider::ComputedTransformProvider(
    const LengthValue* computed_font_size)
    : computed_font_size_(computed_font_size) {}

void ComputedTransformProvider::VisitTransformFunctionList(
    TransformFunctionListValue* transform_function_list) {
  if (!TransformListContainsEmUnits(transform_function_list)) {
    // If the transform list contains no transforms that use em units, then
    // we do not need to do anything and we can pass through the existing
    // transform.
    computed_transform_list_ = transform_function_list;
  } else {
    // The transform list contains at least one transform with em units.
    // In this case, rebuild the transform list with computed length values.
    TransformFunctionListValue::Builder computed_list_builder;

    for (TransformFunctionListValue::Builder::const_iterator iter =
             transform_function_list->value().begin();
         iter != transform_function_list->value().end(); ++iter) {
      TransformFunction* transform_function = *iter;

      ComputedTransformFunctionProvider computed_transform_function_provider(
          computed_font_size_);
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
    case KeywordValue::kMiddle:
    case KeywordValue::kMonospace:
    case KeywordValue::kNoRepeat:
    case KeywordValue::kNormal:
    case KeywordValue::kNoWrap:
    case KeywordValue::kPre:
    case KeywordValue::kRelative:
    case KeywordValue::kRepeat:
    case KeywordValue::kReverse:
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kSolid:
    case KeywordValue::kStatic:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
    default:
      NOTREACHED();
  }
}

}  // namespace

void PromoteToComputedStyle(
    const scoped_refptr<CSSStyleDeclarationData>& specified_style,
    const scoped_refptr<const CSSStyleDeclarationData>& parent_computed_style,
    GURLMap* const property_key_to_base_url_map) {
  DCHECK(specified_style);
  DCHECK(parent_computed_style);

  // An absolutely positioned element (or its box) implies that the element's
  // 'position' property has the value 'absolute' or 'fixed'.
  //   http://www.w3.org/TR/CSS21/visuren.html#absolutely-positioned
  const bool element_is_absolutely_positioned =
      specified_style->position() == KeywordValue::GetAbsolute() ||
      specified_style->position() == KeywordValue::GetFixed();
  // According to http://www.w3.org/TR/CSS21/visuren.html#dis-pos-flo,
  // "inline" and "inline-block" values of "display" become "block" if
  // "position" is "absolute" or "fixed".
  if (element_is_absolutely_positioned &&
      (specified_style->display() == KeywordValue::GetInline() ||
       specified_style->display() == KeywordValue::GetInlineBlock())) {
    specified_style->set_display(KeywordValue::GetBlock());
  }

  // The font size is computed first because most other properties depend on
  // the computed font size value.
  ComputedFontSizeProvider font_size_provider(
      base::polymorphic_downcast<LengthValue*>(
          parent_computed_style->font_size().get()));
  specified_style->font_size()->Accept(&font_size_provider);
  const scoped_refptr<PropertyValue>& font_size =
      font_size_provider.computed_font_size();
  if (font_size) {
    specified_style->set_font_size(font_size);
  }

  // The border color is always computed, even when the value is not declared
  // because the inital value of 'currentColor' is not a computed property
  // value.
  if (specified_style->border_color() == KeywordValue::GetCurrentColor()) {
    // The computed value of the 'currentColor' keyword is the computed
    // value of the 'color' property.
    specified_style->set_border_color(specified_style->color());
  }

  for (CSSStyleDeclarationData::PropertyValueIterator property_value_iterator =
           specified_style->BeginPropertyValueIterator();
       !property_value_iterator.Done(); property_value_iterator.Next()) {
    switch (property_value_iterator.Key()) {
      case kBackgroundPositionProperty: {
        ComputedBackgroundPositionProvider background_position_provider(
            font_size_provider.computed_font_size().get());
        specified_style->background_position()->Accept(
            &background_position_provider);
        const scoped_refptr<PropertyValue>& computed_background_position =
            background_position_provider.computed_background_position();
        if (computed_background_position) {
          specified_style->set_background_position(
              computed_background_position);
        }

        break;
      }
      case kBorderWidthProperty: {
        ComputedBorderWidthProvider border_width_provider(
            font_size_provider.computed_font_size().get(),
            specified_style->border_style().get());
        property_value_iterator.Value()->Accept(&border_width_provider);
        property_value_iterator.SetValue(
            border_width_provider.computed_border_width());
        break;
      }
      case kFontWeightProperty: {
        ComputedFontWeightProvider font_weight_provider;
        property_value_iterator.Value()->Accept(&font_weight_provider);
        property_value_iterator.SetValue(
            font_weight_provider.computed_font_weight());
        break;
      }
      case kHeightProperty: {
        ComputedHeightProvider height_provider(
            parent_computed_style->height().get(),
            font_size_provider.computed_font_size().get(),
            element_is_absolutely_positioned);
        property_value_iterator.Value()->Accept(&height_provider);
        property_value_iterator.SetValue(height_provider.computed_height());
        break;
      }
      case kLineHeightProperty: {
        ComputedLineHeightProvider line_height_provider(
            font_size_provider.computed_font_size().get());
        property_value_iterator.Value()->Accept(&line_height_provider);
        property_value_iterator.SetValue(
            line_height_provider.computed_line_height());
        break;
      }
      case kMarginBottomProperty:
      case kMarginLeftProperty:
      case kMarginRightProperty:
      case kMarginTopProperty: {
        ComputedMarginOrPaddingEdgeProvider margin_provider(
            font_size_provider.computed_font_size().get());
        property_value_iterator.Value()->Accept(&margin_provider);
        property_value_iterator.SetValue(
            margin_provider.computed_margin_or_padding_edge());
        break;
      }
      case kPaddingBottomProperty:
      case kPaddingLeftProperty:
      case kPaddingRightProperty:
      case kPaddingTopProperty: {
        ComputedMarginOrPaddingEdgeProvider padding_provider(
            font_size_provider.computed_font_size().get());
        property_value_iterator.Value()->Accept(&padding_provider);
        property_value_iterator.SetValue(
            padding_provider.computed_margin_or_padding_edge());
        break;
      }
      case kMaxHeightProperty: {
        scoped_refptr<PropertyValue> property_value =
            property_value_iterator.Value();
        ComputedMaxHeightProvider max_height_provider(
            parent_computed_style->height().get(),
            font_size_provider.computed_font_size().get(),
            element_is_absolutely_positioned);
        property_value->Accept(&max_height_provider);
        property_value_iterator.SetValue(
            max_height_provider.computed_max_height());
        break;
      }
      case kMaxWidthProperty:
      case kMinWidthProperty: {
        ComputedMinMaxWidthProvider min_max_width_provider(
            parent_computed_style->width().get(),
            font_size_provider.computed_font_size().get());
        property_value_iterator.Value()->Accept(&min_max_width_provider);
        property_value_iterator.SetValue(
            min_max_width_provider.computed_min_max_width());
        break;
      }
      case kMinHeightProperty: {
        scoped_refptr<PropertyValue> property_value =
            property_value_iterator.Value();
        ComputedMinHeightProvider min_height_provider(
            parent_computed_style->height().get(),
            font_size_provider.computed_font_size().get(),
            element_is_absolutely_positioned);
        property_value->Accept(&min_height_provider);
        property_value_iterator.SetValue(
            min_height_provider.computed_min_height());
        break;
      }
      case kWidthProperty: {
        ComputedWidthProvider width_provider(
            font_size_provider.computed_font_size().get());
        property_value_iterator.Value()->Accept(&width_provider);
        property_value_iterator.SetValue(width_provider.computed_width());
        break;
      }
      case kBackgroundImageProperty: {
        if (property_key_to_base_url_map) {
          ComputedBackgroundImageProvider background_image_provider(
              (*property_key_to_base_url_map)[kBackgroundImageProperty]);
          property_value_iterator.Value()->Accept(&background_image_provider);
          property_value_iterator.SetValue(
              background_image_provider.computed_background_image());
        }
        break;
      }
      case kBackgroundSizeProperty: {
        ComputedBackgroundSizeProvider background_size_provider(
            font_size_provider.computed_font_size().get());
        property_value_iterator.Value()->Accept(&background_size_provider);
        property_value_iterator.SetValue(
            background_size_provider.computed_background_size());
        break;
      }
      case kTransformProperty: {
        ComputedTransformProvider transform_provider(
            font_size_provider.computed_font_size().get());
        property_value_iterator.Value()->Accept(&transform_provider);
        property_value_iterator.SetValue(
            transform_provider.computed_transform_list());
        break;
      }
      case kTextIndentProperty: {
        ComputedTextIndentProvider text_indent_provider(
            font_size_provider.computed_font_size().get());
        property_value_iterator.Value()->Accept(&text_indent_provider);
        property_value_iterator.SetValue(
            text_indent_provider.computed_text_indent());
        break;
      }
      case kBottomProperty:
      case kLeftProperty:
      case kRightProperty:
      case kTopProperty: {
        ComputedPositionOffsetProvider position_offset_provider(
            font_size_provider.computed_font_size().get());
        property_value_iterator.Value()->Accept(&position_offset_provider);
        property_value_iterator.SetValue(
            position_offset_provider.computed_position_offset());
        break;
      }
      case kAnimationDelayProperty:
      case kAnimationDirectionProperty:
      case kAnimationDurationProperty:
      case kAnimationFillModeProperty:
      case kAnimationIterationCountProperty:
      case kAnimationNameProperty:
      case kAnimationTimingFunctionProperty:
      case kBackgroundColorProperty:
      case kBackgroundRepeatProperty:
      case kBorderColorProperty:
      case kBorderRadiusProperty:
      case kBorderStyleProperty:
      case kColorProperty:
      case kContentProperty:
      case kDisplayProperty:
      case kFontFamilyProperty:
      case kFontSizeProperty:
      case kFontStyleProperty:
      case kOpacityProperty:
      case kOverflowProperty:
      case kOverflowWrapProperty:
      case kPositionProperty:
      case kTabSizeProperty:
      case kTextAlignProperty:
      case kTextOverflowProperty:
      case kTextTransformProperty:
      case kTransitionDelayProperty:
      case kTransitionDurationProperty:
      case kTransitionPropertyProperty:
      case kTransitionTimingFunctionProperty:
      case kVerticalAlignProperty:
      case kVisibilityProperty:
      case kWhiteSpaceProperty:
      case kZIndexProperty:
        // Nothing.
        break;
      case kNoneProperty:
      case kAllProperty:
      case kAnimationProperty:
      case kBackgroundProperty:
      case kBorderProperty:
      case kMarginProperty:
      case kPaddingProperty:
      case kSrcProperty:
      case kTransitionProperty:
      case kUnicodeRangeProperty:
      case kWordWrapProperty:
      default:
        NOTREACHED();
        break;
    }
  }
}

scoped_refptr<CSSStyleDeclarationData> GetComputedStyleOfAnonymousBox(
    const scoped_refptr<const CSSStyleDeclarationData>& parent_computed_style) {
  scoped_refptr<CSSStyleDeclarationData> computed_style =
      new CSSStyleDeclarationData();
  PromoteToSpecifiedStyle(computed_style, parent_computed_style);
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);
  return computed_style;
}

}  // namespace cssom
}  // namespace cobalt
