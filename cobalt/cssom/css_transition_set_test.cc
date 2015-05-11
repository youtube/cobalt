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

#include "base/time.h"
#include "cobalt/cssom/const_string_list_value.h"
#include "cobalt/cssom/css_style_declaration_data.h"
#include "cobalt/cssom/css_transition.h"
#include "cobalt/cssom/css_transition_set.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/property_names.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/cssom/time_list_value.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

namespace {
scoped_refptr<cssom::TimeListValue> MakeTimeListWithSingleTime(
    float time_in_seconds) {
  scoped_ptr<cssom::TimeListValue::Builder> time_list(
      new cssom::TimeListValue::Builder());
  time_list->push_back(base::TimeDelta::FromMicroseconds(static_cast<int64>(
      time_in_seconds * base::Time::kMicrosecondsPerSecond)));
  return make_scoped_refptr(new cssom::TimeListValue(time_list.Pass()));
}

scoped_refptr<cssom::ConstStringListValue>
MakePropertyNameListWithSingleProperty(const char* property) {
  scoped_ptr<cssom::ConstStringListValue::Builder> property_name_list(
      new cssom::ConstStringListValue::Builder());
  property_name_list->push_back(property);
  return make_scoped_refptr(
      new cssom::ConstStringListValue(property_name_list.Pass()));
}

scoped_refptr<CSSStyleDeclarationData> CreateTestComputedData() {
  scoped_refptr<CSSStyleDeclarationData> initial_data =
      new CSSStyleDeclarationData();

  initial_data->set_background_color(new cssom::RGBAColorValue(0xffffffff));
  initial_data->set_color(new cssom::RGBAColorValue(0x00000000));
  initial_data->set_display(cssom::KeywordValue::GetBlock());
  initial_data->set_font_family(new cssom::StringValue("Droid Sans"));
  initial_data->set_font_size(new cssom::LengthValue(16, cssom::kPixelsUnit));
  initial_data->set_height(new cssom::LengthValue(400, cssom::kPixelsUnit));
  initial_data->set_width(new cssom::LengthValue(400, cssom::kPixelsUnit));
  initial_data->set_transform(cssom::KeywordValue::GetNone());
  initial_data->set_transition_duration(MakeTimeListWithSingleTime(0.0f));
  initial_data->set_transition_property(
      MakePropertyNameListWithSingleProperty(cssom::kAllPropertyName));

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
  scoped_refptr<CSSStyleDeclarationData> start_;
  scoped_refptr<CSSStyleDeclarationData> end_;
};

TEST_F(TransitionSetTest, TransitionSetStartsEmpty) {
  TransitionSet transition_set;
  EXPECT_TRUE(transition_set.empty());
}

TEST_F(TransitionSetTest, TransitionPropertyOfAllGeneratesATransition) {
  end_->set_background_color(new cssom::RGBAColorValue(0x00000000));
  end_->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  end_->set_transition_property(
      MakePropertyNameListWithSingleProperty(cssom::kAllPropertyName));

  TransitionSet transition_set;
  transition_set.UpdateTransitions(base::Time::UnixEpoch(), *start_, *end_);

  EXPECT_FALSE(transition_set.empty());
  EXPECT_NE(
      static_cast<Transition*>(NULL),
      transition_set.GetTransitionForProperty(kBackgroundColorPropertyName));
}

TEST_F(TransitionSetTest,
       TransitionPropertyOfSpecificValueGeneratesATransition) {
  end_->set_background_color(new cssom::RGBAColorValue(0x00000000));
  end_->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  end_->set_transition_property(MakePropertyNameListWithSingleProperty(
      cssom::kBackgroundColorPropertyName));

  TransitionSet transition_set;
  transition_set.UpdateTransitions(base::Time::UnixEpoch(), *start_, *end_);

  EXPECT_FALSE(transition_set.empty());
  EXPECT_NE(
      static_cast<Transition*>(NULL),
      transition_set.GetTransitionForProperty(kBackgroundColorPropertyName));
}

TEST_F(TransitionSetTest, TransitionPropertyOfNoneGeneratesNoTransition) {
  end_->set_background_color(new cssom::RGBAColorValue(0x00000000));
  end_->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  end_->set_transition_property(KeywordValue::GetNone());

  TransitionSet transition_set;
  transition_set.UpdateTransitions(base::Time::UnixEpoch(), *start_, *end_);

  EXPECT_TRUE(transition_set.empty());
}

TEST_F(TransitionSetTest, TransitionDurationOfZeroGeneratesNoTransition) {
  end_->set_background_color(new cssom::RGBAColorValue(0x00000000));
  end_->set_transition_duration(MakeTimeListWithSingleTime(0.0f));
  end_->set_transition_property(MakePropertyNameListWithSingleProperty(
      cssom::kBackgroundColorPropertyName));

  TransitionSet transition_set;
  transition_set.UpdateTransitions(base::Time::UnixEpoch(), *start_, *end_);

  EXPECT_TRUE(transition_set.empty());
  EXPECT_EQ(
      static_cast<Transition*>(NULL),
      transition_set.GetTransitionForProperty(kBackgroundColorPropertyName));
}

TEST_F(TransitionSetTest, NoStyleChangesGeneratesNoTransitions) {
  end_->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  end_->set_transition_property(MakePropertyNameListWithSingleProperty(
      cssom::kBackgroundColorPropertyName));

  TransitionSet transition_set;
  transition_set.UpdateTransitions(base::Time::UnixEpoch(), *start_, *end_);

  EXPECT_TRUE(transition_set.empty());
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

  EXPECT_EQ(a.duration(), b.duration());
  EXPECT_EQ(a.delay(), b.delay());
  EXPECT_EQ(a.EndTime(), b.EndTime());
  EXPECT_EQ(a.start_time(), b.start_time());
  EXPECT_EQ(a.target_property(), b.target_property());
  EXPECT_FLOAT_EQ(a.reversing_shortening_factor(),
                  b.reversing_shortening_factor());
}
}  // namespace

TEST_F(TransitionSetTest, TransitionSetProducesValidFirstTransition) {
  end_->set_background_color(new cssom::RGBAColorValue(0x00000000));
  end_->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  end_->set_transition_property(
      MakePropertyNameListWithSingleProperty(cssom::kAllPropertyName));

  TransitionSet transition_set;
  transition_set.UpdateTransitions(base::Time::UnixEpoch(), *start_, *end_);

  EXPECT_FALSE(transition_set.empty());
  const Transition* transition =
      transition_set.GetTransitionForProperty(kBackgroundColorPropertyName);
  ASSERT_NE(static_cast<Transition*>(NULL), transition);

  Transition expected_transition(
      kBackgroundColorPropertyName, new cssom::RGBAColorValue(0xffffffff),
      new cssom::RGBAColorValue(0x00000000), base::Time::UnixEpoch(),
      base::TimeDelta::FromSeconds(1), base::TimeDelta(), TimingFunction(),
      new cssom::RGBAColorValue(0xffffffff), 1.0f);

  CheckTransitionsEqual(expected_transition, *transition);

  // The transition should remain untouched if the style remains unchanged and
  // the transition has not completed.
  transition_set.UpdateTransitions(base::Time::FromDoubleT(0.5), *end_, *end_);

  EXPECT_FALSE(transition_set.empty());
  transition =
      transition_set.GetTransitionForProperty(kBackgroundColorPropertyName);
  ASSERT_NE(static_cast<Transition*>(NULL), transition);

  CheckTransitionsEqual(expected_transition, *transition);
}

TEST_F(TransitionSetTest, TransitionSetRemovesTransitionAfterCompletion) {
  end_->set_background_color(new cssom::RGBAColorValue(0x00000000));
  end_->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  end_->set_transition_property(
      MakePropertyNameListWithSingleProperty(cssom::kAllPropertyName));

  TransitionSet transition_set;
  transition_set.UpdateTransitions(base::Time::UnixEpoch(), *start_, *end_);

  EXPECT_FALSE(transition_set.empty());
  EXPECT_NE(
      static_cast<Transition*>(NULL),
      transition_set.GetTransitionForProperty(kBackgroundColorPropertyName));

  transition_set.UpdateTransitions(base::Time::FromDoubleT(2.0), *end_, *end_);

  EXPECT_TRUE(transition_set.empty());
  EXPECT_EQ(
      static_cast<Transition*>(NULL),
      transition_set.GetTransitionForProperty(kBackgroundColorPropertyName));
}

namespace {
base::TimeDelta TimeDeltaFromSecondsF(float seconds) {
  return base::TimeDelta::FromMicroseconds(
      static_cast<int64>(seconds * base::Time::kMicrosecondsPerSecond));
}
}  // namespace


TEST_F(TransitionSetTest, TransitionsFromTransitionsWork) {
  end_->set_background_color(new cssom::RGBAColorValue(0x00000000));
  end_->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  end_->set_transition_property(
      MakePropertyNameListWithSingleProperty(cssom::kAllPropertyName));
  scoped_refptr<CSSStyleDeclarationData> end2 = CreateTestComputedData();
  end2->set_background_color(new cssom::RGBAColorValue(0x000000ff));
  end2->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  end2->set_transition_property(
      MakePropertyNameListWithSingleProperty(cssom::kAllPropertyName));

  TransitionSet transition_set;
  transition_set.UpdateTransitions(base::Time::UnixEpoch(), *start_, *end_);
  transition_set.UpdateTransitions(base::Time::FromDoubleT(0.5), *end_, *end2);

  EXPECT_FALSE(transition_set.empty());
  const Transition* transition =
      transition_set.GetTransitionForProperty(kBackgroundColorPropertyName);
  ASSERT_NE(static_cast<Transition*>(NULL), transition);

  CheckTransitionsEqual(
      Transition(
          kBackgroundColorPropertyName, new cssom::RGBAColorValue(0x7f7f7f7f),
          new cssom::RGBAColorValue(0x000000ff), base::Time::FromDoubleT(0.5),
          TimeDeltaFromSecondsF(1.0f), base::TimeDelta(), TimingFunction(),
          new cssom::RGBAColorValue(0x7f7f7f7f), 1.0f),
      *transition);
}

TEST_F(TransitionSetTest, ReverseTransitionsWork) {
  end_->set_background_color(new cssom::RGBAColorValue(0x00000000));
  end_->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  end_->set_transition_property(
      MakePropertyNameListWithSingleProperty(cssom::kAllPropertyName));
  start_->set_transition_duration(MakeTimeListWithSingleTime(1.0f));
  start_->set_transition_property(
      MakePropertyNameListWithSingleProperty(cssom::kAllPropertyName));

  TransitionSet transition_set;
  transition_set.UpdateTransitions(base::Time::UnixEpoch(), *start_, *end_);
  transition_set.UpdateTransitions(
      base::Time::FromDoubleT(0.5), *end_, *start_);

  EXPECT_FALSE(transition_set.empty());
  const Transition* transition =
      transition_set.GetTransitionForProperty(kBackgroundColorPropertyName);
  ASSERT_NE(static_cast<Transition*>(NULL), transition);

  CheckTransitionsEqual(
      Transition(
          kBackgroundColorPropertyName, new cssom::RGBAColorValue(0x7f7f7f7f),
          new cssom::RGBAColorValue(0xffffffff), base::Time::FromDoubleT(0.5),
          TimeDeltaFromSecondsF(0.5f), base::TimeDelta(), TimingFunction(),
          new cssom::RGBAColorValue(0x00000000), 0.5f),
      *transition);

  // Let's try reversing the transition again.
  transition_set.UpdateTransitions(
      base::Time::FromDoubleT(0.75), *start_, *end_);

  EXPECT_FALSE(transition_set.empty());
  transition =
      transition_set.GetTransitionForProperty(kBackgroundColorPropertyName);
  ASSERT_NE(static_cast<Transition*>(NULL), transition);

  CheckTransitionsEqual(
      Transition(
          kBackgroundColorPropertyName, new cssom::RGBAColorValue(0xbfbfbfbf),
          new cssom::RGBAColorValue(0x00000000), base::Time::FromDoubleT(0.75),
          TimeDeltaFromSecondsF(0.75f), base::TimeDelta(), TimingFunction(),
          new cssom::RGBAColorValue(0xffffffff), 0.75f),
      *transition);
}

}  // namespace cssom
}  // namespace cobalt
