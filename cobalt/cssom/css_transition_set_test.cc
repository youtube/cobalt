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

#include <memory>

#include "base/time/time.h"
#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/cssom/css_transition.h"
#include "cobalt/cssom/css_transition_set.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/property_key_list_value.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/cssom/time_list_value.h"
#include "cobalt/cssom/timing_function.h"
#include "cobalt/cssom/timing_function_list_value.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

namespace {
scoped_refptr<TimeListValue> MakeTimeListWithSingleTime(float time_in_seconds) {
  std::unique_ptr<TimeListValue::Builder> time_list(
      new TimeListValue::Builder());
  time_list->push_back(base::TimeDelta::FromMicroseconds(static_cast<int64>(
      time_in_seconds * base::Time::kMicrosecondsPerSecond)));
  return base::WrapRefCounted(new TimeListValue(std::move(time_list)));
}

scoped_refptr<PropertyListValue> MakePropertyListWithSingleProperty(
    const scoped_refptr<PropertyValue>& property_value) {
  std::unique_ptr<PropertyListValue::Builder> property_list_builder(
      new PropertyListValue::Builder());
  property_list_builder->push_back(property_value.get());
  return base::WrapRefCounted(
      new PropertyListValue(std::move(property_list_builder)));
}

scoped_refptr<PropertyKeyListValue> MakePropertyNameListWithSingleProperty(
    PropertyKey property) {
  std::unique_ptr<PropertyKeyListValue::Builder> property_name_list(
      new PropertyKeyListValue::Builder());
  property_name_list->push_back(property);
  return base::WrapRefCounted(
      new PropertyKeyListValue(std::move(property_name_list)));
}

scoped_refptr<TimingFunctionListValue> MakeTimingFunctionWithSingleProperty(
    const scoped_refptr<TimingFunction>& timing_function) {
  std::unique_ptr<TimingFunctionListValue::Builder> timing_function_list(
      new TimingFunctionListValue::Builder());
  timing_function_list->push_back(timing_function);
  return base::WrapRefCounted(
      new TimingFunctionListValue(std::move(timing_function_list)));
}

scoped_refptr<MutableCSSComputedStyleData> CreateTestComputedData() {
  scoped_refptr<MutableCSSComputedStyleData> initial_data =
      new MutableCSSComputedStyleData();

  initial_data->set_background_color(new RGBAColorValue(0xffffffff));
  initial_data->set_color(new RGBAColorValue(0x00000000));
  initial_data->set_display(KeywordValue::GetBlock());
  initial_data->set_is_inline_before_blockification(false);
  initial_data->set_font_family(
      MakePropertyListWithSingleProperty(new StringValue("Roboto")));
  initial_data->set_font_size(new LengthValue(16, kPixelsUnit));
  initial_data->set_height(new LengthValue(400, kPixelsUnit));
  initial_data->set_opacity(new NumberValue(0.5f));
  initial_data->set_width(new LengthValue(400, kPixelsUnit));
  initial_data->set_transform(KeywordValue::GetNone());
  initial_data->set_transition_delay(MakeTimeListWithSingleTime(0.0f));
  initial_data->set_transition_duration(MakeTimeListWithSingleTime(0.0f));
  initial_data->set_transition_property(
      MakePropertyNameListWithSingleProperty(kAllProperty));
  initial_data->set_transition_timing_function(
      MakeTimingFunctionWithSingleProperty(TimingFunction::GetLinear()));

  return initial_data;
}
}  // namespace

class TransitionSetTest : public testing::Test {
 public:
  TransitionSetTest() {
    // Initialize start and end styles to the same data.
    start_ = CreateTestComputedData();
    end_ = CreateTestComputedData();
  }

 protected:
  scoped_refptr<MutableCSSComputedStyleData> start_;
  scoped_refptr<MutableCSSComputedStyleData> end_;
};

TEST_F(TransitionSetTest, TransitionSetStartsEmpty) {
  TransitionSet transition_set;
  EXPECT_TRUE(transition_set.empty());
}

TEST_F(TransitionSetTest, TransitionPropertyOfAllGeneratesATransition) {
  end_->set_background_color(new RGBAColorValue(0x00000000));
  end_->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  end_->set_transition_property(
      MakePropertyNameListWithSingleProperty(kAllProperty));

  TransitionSet transition_set;
  transition_set.UpdateTransitions(base::TimeDelta(), *start_, *end_);

  EXPECT_FALSE(transition_set.empty());
  EXPECT_TRUE(
      transition_set.GetTransitionForProperty(kBackgroundColorProperty));
}

TEST_F(TransitionSetTest,
       TransitionPropertyOfSpecificValueGeneratesATransition) {
  end_->set_background_color(new RGBAColorValue(0x00000000));
  end_->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  end_->set_transition_property(
      MakePropertyNameListWithSingleProperty(kBackgroundColorProperty));

  TransitionSet transition_set;
  transition_set.UpdateTransitions(base::TimeDelta(), *start_, *end_);

  EXPECT_FALSE(transition_set.empty());
  EXPECT_TRUE(
      transition_set.GetTransitionForProperty(kBackgroundColorProperty));
}

TEST_F(TransitionSetTest, TransitionPropertyOfNoneGeneratesNoTransition) {
  end_->set_background_color(new RGBAColorValue(0x00000000));
  end_->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  end_->set_transition_property(KeywordValue::GetNone());

  TransitionSet transition_set;
  transition_set.UpdateTransitions(base::TimeDelta(), *start_, *end_);

  EXPECT_TRUE(transition_set.empty());
}

TEST_F(TransitionSetTest, TransitionDurationOfZeroGeneratesNoTransition) {
  end_->set_background_color(new RGBAColorValue(0x00000000));
  end_->set_transition_duration(MakeTimeListWithSingleTime(0.0f));
  end_->set_transition_property(
      MakePropertyNameListWithSingleProperty(kBackgroundColorProperty));

  TransitionSet transition_set;
  transition_set.UpdateTransitions(base::TimeDelta(), *start_, *end_);

  EXPECT_TRUE(transition_set.empty());
  EXPECT_EQ(static_cast<Transition*>(NULL),
            transition_set.GetTransitionForProperty(kBackgroundColorProperty));
}

TEST_F(TransitionSetTest, NoStyleChangesGeneratesNoTransitions) {
  end_->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  end_->set_transition_property(
      MakePropertyNameListWithSingleProperty(kBackgroundColorProperty));

  TransitionSet transition_set;
  transition_set.UpdateTransitions(base::TimeDelta(), *start_, *end_);

  EXPECT_TRUE(transition_set.empty());
}

TEST_F(TransitionSetTest, TransitionDelayIsAccountedFor) {
  end_->set_background_color(new RGBAColorValue(0x00000000));
  end_->set_transition_delay(MakeTimeListWithSingleTime(1.0f));
  end_->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  end_->set_transition_property(
      MakePropertyNameListWithSingleProperty(kBackgroundColorProperty));

  TransitionSet transition_set;
  transition_set.UpdateTransitions(base::TimeDelta(), *start_, *end_);

  const Transition* transition =
      transition_set.GetTransitionForProperty(kBackgroundColorProperty);
  ASSERT_TRUE(transition);
  EXPECT_EQ(base::TimeDelta::FromSeconds(1), transition->delay());
}

// Here we start testing that the produced transitions are actually
// transitioning a given element correctly.  We use background color since it is
// the first animatable property we have available when these tests were
// written.
namespace {
void CheckTransitionsEqual(const Transition& a, const Transition& b) {
  EXPECT_TRUE(a.start_value()->Equals(*b.start_value()));
  EXPECT_TRUE(a.end_value()->Equals(*b.end_value()));
  EXPECT_TRUE(a.reversing_adjusted_start_value()->Equals(
      *b.reversing_adjusted_start_value()));

  const float kErrorEpsilon = 0.00015f;

  EXPECT_NEAR(a.duration().InSecondsF(), b.duration().InSecondsF(),
              kErrorEpsilon);
  EXPECT_NEAR(a.delay().InSecondsF(), b.delay().InSecondsF(), kErrorEpsilon);
  EXPECT_EQ(a.start_time(), b.start_time());
  EXPECT_EQ(a.target_property(), b.target_property());
  EXPECT_NEAR(a.reversing_shortening_factor(), b.reversing_shortening_factor(),
              kErrorEpsilon);
}
}  // namespace

TEST_F(TransitionSetTest, TransitionSetProducesValidFirstTransition) {
  end_->set_background_color(new RGBAColorValue(0x00000000));
  end_->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  end_->set_transition_property(
      MakePropertyNameListWithSingleProperty(kAllProperty));

  TransitionSet transition_set;
  transition_set.UpdateTransitions(base::TimeDelta(), *start_, *end_);

  EXPECT_FALSE(transition_set.empty());
  const Transition* transition =
      transition_set.GetTransitionForProperty(kBackgroundColorProperty);
  ASSERT_TRUE(transition);

  Transition expected_transition(
      kBackgroundColorProperty, new RGBAColorValue(0xffffffff),
      new RGBAColorValue(0x00000000), base::TimeDelta(),
      base::TimeDelta::FromSeconds(1), base::TimeDelta(),
      TimingFunction::GetLinear(), new RGBAColorValue(0xffffffff), 1.0f);

  CheckTransitionsEqual(expected_transition, *transition);

  // The transition should remain untouched if the style remains unchanged and
  // the transition has not completed.
  transition_set.UpdateTransitions(base::TimeDelta::FromSecondsD(0.5), *end_,
                                   *end_);

  EXPECT_FALSE(transition_set.empty());
  transition =
      transition_set.GetTransitionForProperty(kBackgroundColorProperty);
  ASSERT_TRUE(transition);

  CheckTransitionsEqual(expected_transition, *transition);
}

TEST_F(TransitionSetTest, TransitionSetRemovesTransitionAfterCompletion) {
  end_->set_background_color(new RGBAColorValue(0x00000000));
  end_->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  end_->set_transition_property(
      MakePropertyNameListWithSingleProperty(kAllProperty));

  TransitionSet transition_set;
  transition_set.UpdateTransitions(base::TimeDelta(), *start_, *end_);

  EXPECT_FALSE(transition_set.empty());
  EXPECT_TRUE(
      transition_set.GetTransitionForProperty(kBackgroundColorProperty));

  transition_set.UpdateTransitions(base::TimeDelta::FromSecondsD(2.0), *end_,
                                   *end_);

  EXPECT_TRUE(transition_set.empty());
  EXPECT_EQ(static_cast<Transition*>(NULL),
            transition_set.GetTransitionForProperty(kBackgroundColorProperty));
}

TEST_F(TransitionSetTest, TransitionsFromTransitionsWork) {
  end_->set_background_color(new RGBAColorValue(0x00000000));
  end_->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  end_->set_transition_property(
      MakePropertyNameListWithSingleProperty(kAllProperty));
  scoped_refptr<MutableCSSComputedStyleData> end2 = CreateTestComputedData();
  end2->set_background_color(new RGBAColorValue(0x000000ff));
  end2->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  end2->set_transition_property(
      MakePropertyNameListWithSingleProperty(kAllProperty));

  TransitionSet transition_set;
  transition_set.UpdateTransitions(base::TimeDelta(), *start_, *end_);
  transition_set.UpdateTransitions(base::TimeDelta::FromSecondsD(0.5), *end_,
                                   *end2);

  EXPECT_FALSE(transition_set.empty());
  const Transition* transition =
      transition_set.GetTransitionForProperty(kBackgroundColorProperty);
  ASSERT_TRUE(transition);

  CheckTransitionsEqual(
      Transition(
          kBackgroundColorProperty, new RGBAColorValue(0x80808080),
          new RGBAColorValue(0x000000ff), base::TimeDelta::FromSecondsD(0.5),
          base::TimeDelta::FromSecondsD(1.0f), base::TimeDelta(),
          TimingFunction::GetLinear(), new RGBAColorValue(0x80808080), 1.0f),
      *transition);
}

TEST_F(TransitionSetTest, ReverseTransitionsWork) {
  end_->set_background_color(new RGBAColorValue(0x00000000));
  end_->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  end_->set_transition_property(
      MakePropertyNameListWithSingleProperty(kAllProperty));
  start_->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  start_->set_transition_property(
      MakePropertyNameListWithSingleProperty(kAllProperty));

  TransitionSet transition_set;
  transition_set.UpdateTransitions(base::TimeDelta(), *start_, *end_);
  transition_set.UpdateTransitions(base::TimeDelta::FromSecondsD(0.5), *end_,
                                   *start_);

  EXPECT_FALSE(transition_set.empty());
  const Transition* transition =
      transition_set.GetTransitionForProperty(kBackgroundColorProperty);
  ASSERT_TRUE(transition);

  CheckTransitionsEqual(
      Transition(
          kBackgroundColorProperty, new RGBAColorValue(0x80808080),
          new RGBAColorValue(0xffffffff), base::TimeDelta::FromSecondsD(0.5),
          base::TimeDelta::FromSecondsD(0.5f), base::TimeDelta(),
          TimingFunction::GetLinear(), new RGBAColorValue(0x00000000), 0.5f),
      *transition);

  // Let's try reversing the transition again.
  transition_set.UpdateTransitions(base::TimeDelta::FromSecondsD(0.75), *start_,
                                   *end_);

  EXPECT_FALSE(transition_set.empty());
  transition =
      transition_set.GetTransitionForProperty(kBackgroundColorProperty);
  ASSERT_TRUE(transition);

  CheckTransitionsEqual(
      Transition(
          kBackgroundColorProperty, new RGBAColorValue(0xc0c0c0c0),
          new RGBAColorValue(0x00000000), base::TimeDelta::FromSecondsD(0.75),
          base::TimeDelta::FromSecondsD(0.75f), base::TimeDelta(),
          TimingFunction::GetLinear(), new RGBAColorValue(0xffffffff), 0.75f),
      *transition);
}

// Make sure that we successfully remove transitions when the 'transition'
// CSS property is removed (and so we return to the default setting of
// 'all' for transition an '0' for duration).
TEST_F(TransitionSetTest, ClearingTransitionsWorks) {
  end_->set_background_color(new RGBAColorValue(0x00000000));
  end_->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  end_->set_transition_property(
      MakePropertyNameListWithSingleProperty(kAllProperty));
  scoped_refptr<MutableCSSComputedStyleData> end2 = CreateTestComputedData();
  end2->set_background_color(new RGBAColorValue(0x000000ff));
  end2->set_transition_duration(MakeTimeListWithSingleTime(0.0f));
  end2->set_transition_property(
      MakePropertyNameListWithSingleProperty(kAllProperty));

  TransitionSet transition_set;
  transition_set.UpdateTransitions(base::TimeDelta(), *start_, *end_);
  transition_set.UpdateTransitions(base::TimeDelta::FromSecondsD(0.5), *end_,
                                   *end2);

  EXPECT_TRUE(transition_set.empty());
}

}  // namespace cssom
}  // namespace cobalt
