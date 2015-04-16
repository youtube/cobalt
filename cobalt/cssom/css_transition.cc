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

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/property_name_list_value.h"
#include "cobalt/cssom/property_value_visitor.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/cssom/time_list_value.h"
#include "cobalt/cssom/transform_list_value.h"

namespace cobalt {
namespace cssom {

Transition::Transition(
    const char* target_property,
    const scoped_refptr<PropertyValue>& start_value,
    const scoped_refptr<PropertyValue>& end_value, const base::Time& start_time,
    const base::TimeDelta& duration, const base::TimeDelta& delay,
    const TimingFunction& timing_function,
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
      reversing_shortening_factor_(reversing_shortening_factor) {
  DCHECK(start_value->GetTypeId() == end_value->GetTypeId());
  DCHECK(start_value->GetTypeId() ==
         reversing_adjusted_start_value->GetTypeId());
}

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

  void VisitFontWeight(FontWeightValue* start_font_weight_value) OVERRIDE;
  void VisitKeyword(KeywordValue* start_keyword_value) OVERRIDE;
  void VisitLength(LengthValue* start_length_value) OVERRIDE;
  void VisitNumber(NumberValue* start_number_value) OVERRIDE;
  void VisitPropertyNameList(
      PropertyNameListValue* start_property_name_list_value) OVERRIDE;
  void VisitRGBAColor(RGBAColorValue* start_color_value) OVERRIDE;
  void VisitString(StringValue* start_string_value) OVERRIDE;
  void VisitTimeList(TimeListValue* start_time_list_value) OVERRIDE;
  void VisitTransformList(
      TransformListValue* start_transform_list_value) OVERRIDE;

 private:
  scoped_refptr<PropertyValue> end_value_;
  float progress_;

  scoped_refptr<PropertyValue> animated_value_;
};

namespace {
template <typename T>
T Lerp(const T& a, const T& b, float progress) {
  return static_cast<T>(a * (1 - progress) + b * progress);
}
}  // namespace

void AnimatorVisitor::VisitFontWeight(
    FontWeightValue* /*start_font_weight_value*/) {
  NOTIMPLEMENTED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitKeyword(KeywordValue* /*start_keyword_value*/) {
  NOTIMPLEMENTED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitLength(LengthValue* /*start_length_value*/) {
  NOTIMPLEMENTED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitNumber(NumberValue* start_number_value) {
  const NumberValue& end_number_value =
      *base::polymorphic_downcast<NumberValue*>(end_value_.get());

  animated_value_ = scoped_refptr<PropertyValue>(new NumberValue(
      Lerp(start_number_value->value(), end_number_value.value(), progress_)));
}

void AnimatorVisitor::VisitPropertyNameList(
    PropertyNameListValue* /*start_property_name_list_value*/) {
  NOTIMPLEMENTED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitRGBAColor(RGBAColorValue* start_color_value) {
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

void AnimatorVisitor::VisitTimeList(TimeListValue* /*start_time_list_value*/) {
  NOTIMPLEMENTED();
  animated_value_ = end_value_;
}

void AnimatorVisitor::VisitTransformList(
    TransformListValue* /*start_transform_list_value*/) {
  NOTIMPLEMENTED();
  animated_value_ = end_value_;
}

float Transition::Progress(const base::Time& time) const {
  base::TimeDelta since_start = time - start_time_;
  return timing_function_.Run(static_cast<float>(
      (since_start - delay_).InSecondsF() / duration_.InSecondsF()));
}

const base::Time Transition::EndTime() const {
  return start_time_ +
         (duration_ < base::TimeDelta() ? base::TimeDelta() : duration_) +
         delay_;
}

scoped_refptr<PropertyValue> Transition::Evaluate(
    const base::Time& time) const {
  // Return the animated property value given all animation parameters.
  AnimatorVisitor animator(end_value_, Progress(time));
  start_value_->Accept(&animator);

  return animator.animated_value();
}

}  // namespace cssom
}  // namespace cobalt
