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

#include "cobalt/cssom/css_transition.h"

#include <limits>

#include "base/memory/scoped_ptr.h"
#include "cobalt/base/enable_if.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/cssom/const_string_list_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/linear_gradient_value.h"
#include "cobalt/cssom/local_src_value.h"
#include "cobalt/cssom/matrix_function.h"
#include "cobalt/cssom/media_feature_keyword_value.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/property_value_visitor.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/cssom/rotate_function.h"
#include "cobalt/cssom/scale_function.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/cssom/time_list_value.h"
#include "cobalt/cssom/timing_function_list_value.h"
#include "cobalt/cssom/transform_function.h"
#include "cobalt/cssom/transform_function_list_value.h"
#include "cobalt/cssom/transform_function_visitor.h"
#include "cobalt/cssom/translate_function.h"
#include "cobalt/cssom/unicode_range_value.h"
#include "cobalt/cssom/url_value.h"
#include "cobalt/cssom/url_src_value.h"
#include "cobalt/math/matrix_interpolation.h"
#include "cobalt/math/transform_2d.h"

namespace cobalt {
namespace cssom {

Transition::Transition(
    const char* target_property,
    const scoped_refptr<PropertyValue>& start_value,
    const scoped_refptr<PropertyValue>& end_value,
    const base::TimeDelta& start_time, const base::TimeDelta& duration,
    const base::TimeDelta& delay,
    const scoped_refptr<TimingFunction>& timing_function,
    scoped_refptr<PropertyValue> reversing_adjusted_start_value,
    float reversing_shortening_factor)
    : target_property_(target_property),
      start_value_(start_value),
      end_value_(end_value),
      start_time_(start_time),
      duration_(duration < base::TimeDelta() ? base::TimeDelta() : duration),
      delay_(delay),
      timing_function_(timing_function),
      reversing_adjusted_start_value_(reversing_adjusted_start_value),
      reversing_shortening_factor_(reversing_shortening_factor) {}

// This AnimatorVisitor allows us to define how to apply an animation to
// a CSS style value, for each different CSS style value.  Input to the visitor
// conceptually is a start CSS style value and an end CSS style value that are
// of the same type.  Technically though, we can only visit on one of them, so
// the end value is passed into the constructor and the start value is visited.
//   http://www.w3.org/TR/css3-transitions/#animatable-types
class AnimatorVisitor : public PropertyValueVisitor {
 public:
  AnimatorVisitor(const scoped_refptr<PropertyValue>& end_value, float progress)
      : end_value_(end_value), progress_(progress) {}

  const scoped_refptr<PropertyValue>& animated_value() const {
    return animated_value_;
  }

  void VisitAbsoluteURL(AbsoluteURLValue* start_absolute_url_value) OVERRIDE;
  void VisitConstStringList(
      ConstStringListValue* start_const_string_list_value) OVERRIDE;
  void VisitFontStyle(FontStyleValue* start_font_style_value) OVERRIDE;
  void VisitFontWeight(FontWeightValue* start_font_weight_value) OVERRIDE;
  void VisitInteger(IntegerValue* integer_value) OVERRIDE;
  void VisitKeyword(KeywordValue* start_keyword_value) OVERRIDE;
  void VisitLength(LengthValue* start_length_value) OVERRIDE;
  void VisitLinearGradient(
      LinearGradientValue* start_linear_gradient_value) OVERRIDE;
  void VisitLocalSrc(LocalSrcValue* local_src_value) OVERRIDE;
  void VisitMediaFeatureKeywordValue(
      MediaFeatureKeywordValue* media_feature_keyword_value) OVERRIDE;
  void VisitNumber(NumberValue* start_number_value) OVERRIDE;
  void VisitPercentage(PercentageValue* start_percentage_value) OVERRIDE;
  void VisitPropertyList(PropertyListValue* property_list_value) OVERRIDE;
  void VisitRatio(RatioValue* start_ratio_value) OVERRIDE;
  void VisitResolution(ResolutionValue* start_resolution_value) OVERRIDE;
  void VisitRGBAColor(RGBAColorValue* start_color_value) OVERRIDE;
  void VisitString(StringValue* start_string_value) OVERRIDE;
  void VisitTransformFunctionList(
      TransformFunctionListValue* start_transform_list_value) OVERRIDE;
  void VisitTimeList(TimeListValue* start_time_list_value) OVERRIDE;
  void VisitTimingFunctionList(
      TimingFunctionListValue* start_timing_function_list_value) OVERRIDE;
  void VisitUnicodeRange(UnicodeRangeValue* unicode_range_value) OVERRIDE;
  void VisitURL(URLValue* url_value) OVERRIDE;
  void VisitUrlSrc(UrlSrcValue* url_src_value) OVERRIDE;

 private:
  scoped_refptr<PropertyValue> end_value_;
  float progress_;

  scoped_refptr<PropertyValue> animated_value_;
};

namespace {

// Round to nearest integer for integer types.
template <typename T>
typename base::enable_if<std::numeric_limits<T>::is_integer, T>::type Round(
    float value) {
  return static_cast<T>(value + 0.5);
}

// Pass through the value in the case of non-integer types.
template <typename T>
typename base::enable_if<!std::numeric_limits<T>::is_integer, T>::type Round(
    float value) {
  return static_cast<T>(value);
}

// Linearly interpolate from value a to value b, and then apply a round on the
// results before returning if we are interpolating integer types (as specified
// by http://www.w3.org/TR/css3-transitions/#animatable-types).
template <typename T>
T Lerp(const T& a, const T& b, float progress) {
  return Round<T>(a * (1 - progress) + b * progress);
}

}  // namespace

namespace {
// Defines how each different type of transform function should be animated.
class AnimateTransformFunction : public TransformFunctionVisitor {
 public:
  // Returns an animated version of the transform function given the start, end
  // and progress.  Note that end may be NULL if the destination transform is
  // 'none'.  In this case, we should use an appropriate identity transform
  // to animate towards.
  static scoped_ptr<TransformFunction> Animate(const TransformFunction* start,
                                               const TransformFunction* end,
                                               float progress) {
    AnimateTransformFunction visitor(end, progress);
    const_cast<TransformFunction*>(start)->Accept(&visitor);
    return visitor.animated_.Pass();
  }

 private:
  void VisitMatrix(const MatrixFunction* matrix_function) OVERRIDE;
  void VisitRotate(const RotateFunction* rotate_function) OVERRIDE;
  void VisitScale(const ScaleFunction* scale_function) OVERRIDE;
  void VisitTranslate(const TranslateFunction* translate_function) OVERRIDE;

  AnimateTransformFunction(const TransformFunction* end, float progress)
      : end_(end), progress_(progress) {}

  const TransformFunction* end_;
  float progress_;
  scoped_ptr<TransformFunction> animated_;
};

void AnimateTransformFunction::VisitMatrix(
    const MatrixFunction* matrix_function) {
  const MatrixFunction* matrix_end =
      base::polymorphic_downcast<const MatrixFunction*>(end_);
  math::Matrix3F interpolated_matrix = math::InterpolateMatrices(
      matrix_function->value(),
      matrix_end ? matrix_end->value() : math::Matrix3F::Identity(), progress_);

  animated_.reset(new MatrixFunction(interpolated_matrix));
}

void AnimateTransformFunction::VisitRotate(
    const RotateFunction* rotate_function) {
  const RotateFunction* rotate_end =
      base::polymorphic_downcast<const RotateFunction*>(end_);

  // The rotate function's identity is the value 0.
  float end_angle = rotate_end ?
                    rotate_end->clockwise_angle_in_radians() : 0.0f;

  animated_.reset(new RotateFunction(
      Lerp(rotate_function->clockwise_angle_in_radians(),
           end_angle, progress_)));
}

void AnimateTransformFunction::VisitScale(const ScaleFunction* scale_function) {
  float end_x_factor, end_y_factor;
  if (end_ == NULL) {
    // Use the scale identity function, which is the value 1.
    end_x_factor = 1.0f;
    end_y_factor = 1.0f;
  } else {
    const ScaleFunction* end_scale =
        base::polymorphic_downcast<const ScaleFunction*>(end_);
    end_x_factor = end_scale->x_factor();
    end_y_factor = end_scale->y_factor();
  }

  animated_.reset(new ScaleFunction(
      Lerp(scale_function->x_factor(), end_x_factor, progress_),
      Lerp(scale_function->y_factor(), end_y_factor, progress_)));
}

void AnimateTransformFunction::VisitTranslate(
    const TranslateFunction* translate_function) {
  const TranslateFunction* translate_end =
      base::polymorphic_downcast<const TranslateFunction*>(end_);
  if (translate_end) {
    // TODO(***REMOVED***): Eventually we'll have to support calc() so that we can
    //               animate from one unit (e.g. length) to another
    //               (e.g. percentage) for not just translations but other
    //               length/percentage property values also.
    DCHECK_EQ(translate_function->offset_type(), translate_end->offset_type())
        << "calc() is needed for animating mixed units, but is not yet "
           "supported.";

    DCHECK_EQ(translate_function->axis(), translate_end->axis());
  }

  switch (translate_function->offset_type()) {
    case TranslateFunction::kLength: {
      DCHECK_EQ(kPixelsUnit, translate_function->offset_as_length()->unit());
      if (translate_end) {
        DCHECK_EQ(kPixelsUnit, translate_end->offset_as_length()->unit());
      }

      // The transform function's identity is the value 0.0f.
      float end_offset =
          translate_end ? translate_end->offset_as_length()->value() : 0.0f;

      animated_.reset(new TranslateFunction(
          translate_function->axis(),
          new LengthValue(Lerp(translate_function->offset_as_length()->value(),
                               end_offset, progress_),
                          kPixelsUnit)));
    } break;
    case TranslateFunction::kPercentage: {
      // The transform function's identity is the value 0.0f.
      float end_offset =
          translate_end ? translate_end->offset_as_percentage()->value() : 0.0f;

      animated_.reset(new TranslateFunction(
          translate_function->axis(),
          new PercentageValue(
              Lerp(translate_function->offset_as_percentage()->value(),
                   end_offset, progress_))));
    } break;
    default: { NOTREACHED(); }
  }
}

// Returns true if two given transform function lists have the same number of
// elements, and each element type matches the corresponding element type at
// the same index in the other list.
bool TransformListsHaveSameType(const TransformFunctionListValue::Builder& a,
                                const TransformFunctionListValue::Builder& b) {
  if (a.size() != b.size()) {
    return false;
  }

  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i]->GetTypeId() != b[i]->GetTypeId()) {
      return false;
    } else if (a[i]->GetTypeId() == base::GetTypeId<TranslateFunction>() &&
               base::polymorphic_downcast<const TranslateFunction*>(a[i])
                       ->axis() !=
                   base::polymorphic_downcast<const TranslateFunction*>(b[i])
                       ->axis()) {
      return false;
    }
  }
  return true;
}

scoped_refptr<PropertyValue> AnimateTransform(const PropertyValue* start_value,
                                              const PropertyValue* end_value,
                                              float progress) {
  // The process for animating a transform list are described here:
  //  http://www.w3.org/TR/2012/WD-css3-transforms-20120228/#animation

  // If both start and end values are "none", then the animated value is
  // "none" also and we are done.
  if (start_value->Equals(*KeywordValue::GetNone()) &&
      end_value->Equals(*KeywordValue::GetNone())) {
    return KeywordValue::GetNone();
  }

  // At this point, either start_value or end_value may be "none" (though not
  // both).  We shuffle things around here to ensure that start_value is always
  // not "none" so that subsequent code is simplified.
  if (start_value->Equals(*KeywordValue::GetNone())) {
    std::swap(start_value, end_value);
    progress = 1 - progress;
  }

  const TransformFunctionListValue* start_transform =
      base::polymorphic_downcast<const TransformFunctionListValue*>(
          start_value);
  const TransformFunctionListValue* end_transform =
      end_value->Equals(*KeywordValue::GetNone())
          ? NULL
          : base::polymorphic_downcast<const TransformFunctionListValue*>(
                end_value);

  const TransformFunctionListValue::Builder* start_functions =
      &start_transform->value();

  const TransformFunctionListValue::Builder* end_functions =
      end_transform ? &end_transform->value() : NULL;

  // We first check to see if there is a match between transform types in
  // the start transform list and transform types in the end transform list.
  // This is necessary to know how to proceed in animating this transform.
  // Note that a value of "none" implies that we will use identity transforms
  // for that transform list, so in that case, there is indeed a match.
  bool matching_list_types =
      end_functions == NULL ||
      TransformListsHaveSameType(*start_functions, *end_functions);
  if (matching_list_types) {
    TransformFunctionListValue::Builder animated_functions;
    // The lists have the same number of values and each corresponding transform
    // matches in type.  In this case, we do a transition on each
    // corresponding transform individually.
    for (size_t i = 0; i < start_functions->size(); ++i) {
      animated_functions.push_back(
          AnimateTransformFunction::Animate(
              (*start_functions)[i], end_functions ? (*end_functions)[i] : NULL,
              progress).release());
    }
    return new TransformFunctionListValue(animated_functions.Pass());
  } else {
    // The transform lists do not match up type for type. Collapse each list
    // into a matrix and animate the matrix using the algorithm described here:
    //   http://www.w3.org/TR/2012/WD-css3-transforms-20120228/#matrix-decomposition
    DCHECK(end_transform);
    TransformFunctionListValue::Builder animated_functions;
    animated_functions.push_back(new MatrixFunction(math::InterpolateMatrices(
        start_transform->ToMatrix(), end_transform->ToMatrix(), progress)));
    return new TransformFunctionListValue(animated_functions.Pass());
  }
}
}  // namespace

void AnimatorVisitor::VisitAbsoluteURL(
    AbsoluteURLValue* /*start_absolute_url_value*/) {
  NOTIMPLEMENTED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitFontStyle(
    FontStyleValue* /*start_font_style_value*/) {
  NOTIMPLEMENTED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitFontWeight(
    FontWeightValue* /*start_font_weight_value*/) {
  NOTIMPLEMENTED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitInteger(
    IntegerValue* integer_value) {
  UNREFERENCED_PARAMETER(integer_value);
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitKeyword(KeywordValue* start_keyword_value) {
  if (start_keyword_value->Equals(*end_value_)) {
    animated_value_ = start_keyword_value;
  }

  switch (start_keyword_value->value()) {
    case KeywordValue::kNone:
      if (end_value_->GetTypeId() ==
          base::GetTypeId<TransformFunctionListValue>()) {
        animated_value_ =
            AnimateTransform(start_keyword_value, end_value_, progress_);
      }
      break;

    case KeywordValue::kAbsolute:
    case KeywordValue::kAuto:
    case KeywordValue::kBaseline:
    case KeywordValue::kBlock:
    case KeywordValue::kBreakWord:
    case KeywordValue::kCenter:
    case KeywordValue::kClip:
    case KeywordValue::kContain:
    case KeywordValue::kCover:
    case KeywordValue::kCursive:
    case KeywordValue::kEllipsis:
    case KeywordValue::kFantasy:
    case KeywordValue::kFixed:
    case KeywordValue::kHidden:
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
    case KeywordValue::kRight:
    case KeywordValue::kSansSerif:
    case KeywordValue::kSerif:
    case KeywordValue::kStatic:
    case KeywordValue::kTop:
    case KeywordValue::kUppercase:
    case KeywordValue::kVisible:
    default:
      NOTREACHED();
      break;
  }
}

void AnimatorVisitor::VisitLength(LengthValue* /*start_length_value*/) {
  NOTIMPLEMENTED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitLinearGradient(
    LinearGradientValue* /*start_linear_gradient_value*/) {
  NOTIMPLEMENTED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitLocalSrc(LocalSrcValue* /*local_src_value*/) {
  NOTIMPLEMENTED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitMediaFeatureKeywordValue(
    MediaFeatureKeywordValue* /*media_feature_keyword_value*/) {
  NOTREACHED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitNumber(NumberValue* start_number_value) {
  DCHECK(start_number_value->GetTypeId() == end_value_->GetTypeId());
  const NumberValue& end_number_value =
      *base::polymorphic_downcast<NumberValue*>(end_value_.get());

  animated_value_ = scoped_refptr<PropertyValue>(new NumberValue(
      Lerp(start_number_value->value(), end_number_value.value(), progress_)));
}

void AnimatorVisitor::VisitPercentage(
    PercentageValue* /*start_percentage_value*/) {
  NOTIMPLEMENTED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitPropertyList(
    PropertyListValue* /*property_list_value*/) {
  NOTIMPLEMENTED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitRatio(RatioValue* /*start_ratio_value*/) {
  NOTREACHED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitResolution(
    ResolutionValue* /*start_resolution_value*/) {
  NOTREACHED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitRGBAColor(RGBAColorValue* start_color_value) {
  DCHECK(start_color_value->GetTypeId() == end_value_->GetTypeId());
  const RGBAColorValue& end_color_value =
      *base::polymorphic_downcast<RGBAColorValue*>(end_value_.get());

  animated_value_ = scoped_refptr<PropertyValue>(new RGBAColorValue(
      Lerp(start_color_value->r(), end_color_value.r(), progress_),
      Lerp(start_color_value->g(), end_color_value.g(), progress_),
      Lerp(start_color_value->b(), end_color_value.b(), progress_),
      Lerp(start_color_value->a(), end_color_value.a(), progress_)));
}

void AnimatorVisitor::VisitString(StringValue* /*start_string_value*/) {
  NOTIMPLEMENTED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitConstStringList(
    ConstStringListValue* /*start_const_string_list_value*/) {
  NOTIMPLEMENTED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitTimeList(TimeListValue* /*start_time_list_value*/) {
  NOTIMPLEMENTED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitTransformFunctionList(
    TransformFunctionListValue* start_transform_list_value) {
  animated_value_ =
      AnimateTransform(start_transform_list_value, end_value_, progress_);
}

void AnimatorVisitor::VisitTimingFunctionList(
    TimingFunctionListValue* /*start_timing_function_list_value*/) {
  NOTIMPLEMENTED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitUnicodeRange(
    UnicodeRangeValue* /*unicode_range_value*/) {
  NOTREACHED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitURL(URLValue* /*url_value*/) {
  NOTREACHED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitUrlSrc(UrlSrcValue* /*url_src_value*/) {
  NOTREACHED();
  animated_value_ = end_value_;
}

float Transition::Progress(const base::TimeDelta& time) const {
  base::TimeDelta since_start = time - start_time_;
  return timing_function_->Evaluate(static_cast<float>(
      (since_start - delay_).InSecondsF() / duration_.InSecondsF()));
}

const base::TimeDelta Transition::EndTime() const {
  return start_time_ +
         (duration_ < base::TimeDelta() ? base::TimeDelta() : duration_) +
         delay_;
}

scoped_refptr<PropertyValue> Transition::Evaluate(
    const base::TimeDelta& time) const {
  // Return the animated property value given all animation parameters.
  AnimatorVisitor animator(end_value_, Progress(time));
  start_value_->Accept(&animator);

  return animator.animated_value();
}

}  // namespace cssom
}  // namespace cobalt
