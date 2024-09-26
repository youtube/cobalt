// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/timing_input.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_keyframe_animation_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_keyframe_effect_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_range_offset.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_keyframeanimationoptions_unrestricteddouble.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_keyframeeffectoptions_unrestricteddouble.h"
#include "third_party/blink/renderer/core/animation/animation_test_helpers.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_values.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "v8/include/v8.h"

namespace blink {

using animation_test_helpers::SetV8ObjectPropertyAsNumber;
using animation_test_helpers::SetV8ObjectPropertyAsString;

class AnimationTimingInputTest : public testing::Test {
 public:
  Timing ApplyTimingInputNumber(v8::Isolate*,
                                String timing_property,
                                double timing_property_value,
                                bool& timing_conversion_success,
                                bool is_keyframeeffectoptions = true);
  Timing ApplyTimingInputString(v8::Isolate*,
                                String timing_property,
                                String timing_property_value,
                                bool& timing_conversion_success,
                                bool is_keyframeeffectoptions = true);

 private:
  void SetUp() override { page_holder_ = std::make_unique<DummyPageHolder>(); }

  Document* GetDocument() const { return &page_holder_->GetDocument(); }

  std::unique_ptr<DummyPageHolder> page_holder_;
};

Timing AnimationTimingInputTest::ApplyTimingInputNumber(
    v8::Isolate* isolate,
    String timing_property,
    double timing_property_value,
    bool& timing_conversion_success,
    bool is_keyframeeffectoptions) {
  v8::Local<v8::Object> timing_input = v8::Object::New(isolate);
  SetV8ObjectPropertyAsNumber(isolate, timing_input, timing_property,
                              timing_property_value);

  Timing result;
  timing_conversion_success = false;
  DummyExceptionStateForTesting exception_state;
  if (is_keyframeeffectoptions) {
    KeyframeEffectOptions* timing_input_dictionary =
        NativeValueTraits<KeyframeEffectOptions>::NativeValue(
            isolate, timing_input, exception_state);
    if (exception_state.HadException()) {
      return Timing();
    }

    auto* options =
        MakeGarbageCollected<V8UnionKeyframeEffectOptionsOrUnrestrictedDouble>(
            timing_input_dictionary);
    result = TimingInput::Convert(options, GetDocument(), exception_state);
    if (exception_state.HadException())
      return Timing();
  } else {
    KeyframeAnimationOptions* timing_input_dictionary =
        NativeValueTraits<KeyframeAnimationOptions>::NativeValue(
            isolate, timing_input, exception_state);
    if (exception_state.HadException())
      return Timing();

    auto* options = MakeGarbageCollected<
        V8UnionKeyframeAnimationOptionsOrUnrestrictedDouble>(
        timing_input_dictionary);
    result = TimingInput::Convert(options, GetDocument(), exception_state);
    if (exception_state.HadException())
      return Timing();
  }
  timing_conversion_success = true;
  return result;
}

Timing AnimationTimingInputTest::ApplyTimingInputString(
    v8::Isolate* isolate,
    String timing_property,
    String timing_property_value,
    bool& timing_conversion_success,
    bool is_keyframeeffectoptions) {
  v8::Local<v8::Object> timing_input = v8::Object::New(isolate);
  SetV8ObjectPropertyAsString(isolate, timing_input, timing_property,
                              timing_property_value);

  Timing result;
  timing_conversion_success = false;
  DummyExceptionStateForTesting exception_state;
  if (is_keyframeeffectoptions) {
    KeyframeEffectOptions* timing_input_dictionary =
        NativeValueTraits<KeyframeEffectOptions>::NativeValue(
            isolate, timing_input, exception_state);
    if (exception_state.HadException())
      return Timing();

    auto* options =
        MakeGarbageCollected<V8UnionKeyframeEffectOptionsOrUnrestrictedDouble>(
            timing_input_dictionary);
    result = TimingInput::Convert(options, GetDocument(), exception_state);
    if (exception_state.HadException())
      return Timing();
  } else {
    KeyframeAnimationOptions* timing_input_dictionary =
        NativeValueTraits<KeyframeAnimationOptions>::NativeValue(
            isolate, timing_input, exception_state);
    if (exception_state.HadException())
      return Timing();

    auto* options = MakeGarbageCollected<
        V8UnionKeyframeAnimationOptionsOrUnrestrictedDouble>(
        timing_input_dictionary);
    result = TimingInput::Convert(options, GetDocument(), exception_state);
    if (exception_state.HadException())
      return Timing();
  }
  timing_conversion_success = true;
  return result;
}

TEST_F(AnimationTimingInputTest, TimingInputStartDelay) {
  V8TestingScope scope;
  bool did_success;
  EXPECT_EQ(1.1, ApplyTimingInputNumber(scope.GetIsolate(), "delay", 1100,
                                        did_success)
                     .start_delay.AsTimeValue()
                     .InSecondsF());
  EXPECT_TRUE(did_success);
  EXPECT_EQ(-1, ApplyTimingInputNumber(scope.GetIsolate(), "delay", -1000,
                                       did_success)
                    .start_delay.AsTimeValue()
                    .InSecondsF());
  EXPECT_TRUE(did_success);
  EXPECT_EQ(1, ApplyTimingInputString(scope.GetIsolate(), "delay", "1000",
                                      did_success)
                   .start_delay.AsTimeValue()
                   .InSecondsF());
  EXPECT_TRUE(did_success);
  EXPECT_EQ(
      0, ApplyTimingInputString(scope.GetIsolate(), "delay", "1s", did_success)
             .start_delay.AsTimeValue()
             .InSecondsF());
  EXPECT_FALSE(did_success);
  EXPECT_EQ(0, ApplyTimingInputString(scope.GetIsolate(), "delay", "Infinity",
                                      did_success)
                   .start_delay.AsTimeValue()
                   .InSecondsF());
  EXPECT_FALSE(did_success);
  EXPECT_EQ(0, ApplyTimingInputString(scope.GetIsolate(), "delay", "-Infinity",
                                      did_success)
                   .start_delay.AsTimeValue()
                   .InSecondsF());
  EXPECT_FALSE(did_success);
  EXPECT_EQ(
      0, ApplyTimingInputString(scope.GetIsolate(), "delay", "NaN", did_success)
             .start_delay.AsTimeValue()
             .InSecondsF());
  EXPECT_FALSE(did_success);
  EXPECT_EQ(0, ApplyTimingInputString(scope.GetIsolate(), "delay", "rubbish",
                                      did_success)
                   .start_delay.AsTimeValue()
                   .InSecondsF());
  EXPECT_FALSE(did_success);
}

TEST_F(AnimationTimingInputTest,
       TimingInputStartDelayKeyframeAnimationOptions) {
  V8TestingScope scope;
  bool ignored_success;
  EXPECT_EQ(1.1, ApplyTimingInputNumber(scope.GetIsolate(), "delay", 1100,
                                        ignored_success, false)
                     .start_delay.AsTimeValue()
                     .InSecondsF());
  EXPECT_EQ(-1, ApplyTimingInputNumber(scope.GetIsolate(), "delay", -1000,
                                       ignored_success, false)
                    .start_delay.AsTimeValue()
                    .InSecondsF());
  EXPECT_EQ(1, ApplyTimingInputString(scope.GetIsolate(), "delay", "1000",
                                      ignored_success, false)
                   .start_delay.AsTimeValue()
                   .InSecondsF());
  EXPECT_EQ(0, ApplyTimingInputString(scope.GetIsolate(), "delay", "1s",
                                      ignored_success, false)
                   .start_delay.AsTimeValue()
                   .InSecondsF());
  EXPECT_EQ(0, ApplyTimingInputString(scope.GetIsolate(), "delay", "Infinity",
                                      ignored_success, false)
                   .start_delay.AsTimeValue()
                   .InSecondsF());
  EXPECT_EQ(0, ApplyTimingInputString(scope.GetIsolate(), "delay", "-Infinity",
                                      ignored_success, false)
                   .start_delay.AsTimeValue()
                   .InSecondsF());
  EXPECT_EQ(0, ApplyTimingInputString(scope.GetIsolate(), "delay", "NaN",
                                      ignored_success, false)
                   .start_delay.AsTimeValue()
                   .InSecondsF());
  EXPECT_EQ(0, ApplyTimingInputString(scope.GetIsolate(), "delay", "rubbish",
                                      ignored_success, false)
                   .start_delay.AsTimeValue()
                   .InSecondsF());
}

TEST_F(AnimationTimingInputTest, TimingInputEndDelay) {
  V8TestingScope scope;
  bool ignored_success;
  EXPECT_EQ(10, ApplyTimingInputNumber(scope.GetIsolate(), "endDelay", 10000,
                                       ignored_success)
                    .end_delay.AsTimeValue()
                    .InSecondsF());
  EXPECT_EQ(-2.5, ApplyTimingInputNumber(scope.GetIsolate(), "endDelay", -2500,
                                         ignored_success)
                      .end_delay.AsTimeValue()
                      .InSecondsF());
}

TEST_F(AnimationTimingInputTest, TimingInputFillMode) {
  V8TestingScope scope;
  Timing::FillMode default_fill_mode = Timing::FillMode::AUTO;
  bool ignored_success;

  EXPECT_EQ(Timing::FillMode::AUTO,
            ApplyTimingInputString(scope.GetIsolate(), "fill", "auto",
                                   ignored_success)
                .fill_mode);
  EXPECT_EQ(Timing::FillMode::FORWARDS,
            ApplyTimingInputString(scope.GetIsolate(), "fill", "forwards",
                                   ignored_success)
                .fill_mode);
  EXPECT_EQ(Timing::FillMode::NONE,
            ApplyTimingInputString(scope.GetIsolate(), "fill", "none",
                                   ignored_success)
                .fill_mode);
  EXPECT_EQ(Timing::FillMode::BACKWARDS,
            ApplyTimingInputString(scope.GetIsolate(), "fill", "backwards",
                                   ignored_success)
                .fill_mode);
  EXPECT_EQ(Timing::FillMode::BOTH,
            ApplyTimingInputString(scope.GetIsolate(), "fill", "both",
                                   ignored_success)
                .fill_mode);
  EXPECT_EQ(default_fill_mode,
            ApplyTimingInputString(scope.GetIsolate(), "fill", "everything!",
                                   ignored_success)
                .fill_mode);
  EXPECT_EQ(default_fill_mode,
            ApplyTimingInputString(scope.GetIsolate(), "fill",
                                   "backwardsandforwards", ignored_success)
                .fill_mode);
  EXPECT_EQ(
      default_fill_mode,
      ApplyTimingInputNumber(scope.GetIsolate(), "fill", 2, ignored_success)
          .fill_mode);
}

TEST_F(AnimationTimingInputTest, TimingInputIterationStart) {
  V8TestingScope scope;
  bool success;
  EXPECT_EQ(1.1, ApplyTimingInputNumber(scope.GetIsolate(), "iterationStart",
                                        1.1, success)
                     .iteration_start);
  EXPECT_TRUE(success);

  ApplyTimingInputNumber(scope.GetIsolate(), "iterationStart", -1, success);
  EXPECT_FALSE(success);

  ApplyTimingInputString(scope.GetIsolate(), "iterationStart", "Infinity",
                         success);
  EXPECT_FALSE(success);

  ApplyTimingInputString(scope.GetIsolate(), "iterationStart", "-Infinity",
                         success);
  EXPECT_FALSE(success);

  ApplyTimingInputString(scope.GetIsolate(), "iterationStart", "NaN", success);
  EXPECT_FALSE(success);

  ApplyTimingInputString(scope.GetIsolate(), "iterationStart", "rubbish",
                         success);
  EXPECT_FALSE(success);
}

TEST_F(AnimationTimingInputTest, TimingInputIterationCount) {
  V8TestingScope scope;
  bool success;
  EXPECT_EQ(2.1, ApplyTimingInputNumber(scope.GetIsolate(), "iterations", 2.1,
                                        success)
                     .iteration_count);
  EXPECT_TRUE(success);

  Timing timing = ApplyTimingInputString(scope.GetIsolate(), "iterations",
                                         "Infinity", success);
  EXPECT_TRUE(success);
  EXPECT_TRUE(std::isinf(timing.iteration_count));
  EXPECT_GT(timing.iteration_count, 0);

  ApplyTimingInputNumber(scope.GetIsolate(), "iterations", -1, success);
  EXPECT_FALSE(success);

  ApplyTimingInputString(scope.GetIsolate(), "iterations", "-Infinity",
                         success);
  EXPECT_FALSE(success);

  ApplyTimingInputString(scope.GetIsolate(), "iterations", "NaN", success);
  EXPECT_FALSE(success);

  ApplyTimingInputString(scope.GetIsolate(), "iterations", "rubbish", success);
  EXPECT_FALSE(success);
}

TEST_F(AnimationTimingInputTest, TimingInputIterationDuration) {
  V8TestingScope scope;
  bool success;
  EXPECT_EQ(
      ANIMATION_TIME_DELTA_FROM_SECONDS(1.1),
      ApplyTimingInputNumber(scope.GetIsolate(), "duration", 1100, success)
          .iteration_duration);
  EXPECT_TRUE(success);

  Timing timing =
      ApplyTimingInputNumber(scope.GetIsolate(), "duration",
                             std::numeric_limits<double>::infinity(), success);
  EXPECT_TRUE(success);
  EXPECT_TRUE(timing.iteration_duration->is_max());

  EXPECT_FALSE(
      ApplyTimingInputString(scope.GetIsolate(), "duration", "auto", success)
          .iteration_duration);
  EXPECT_TRUE(success);

  ApplyTimingInputString(scope.GetIsolate(), "duration", "1000", success);
  EXPECT_FALSE(success);

  ApplyTimingInputNumber(scope.GetIsolate(), "duration", -1000, success);
  EXPECT_FALSE(success);

  ApplyTimingInputString(scope.GetIsolate(), "duration", "-Infinity", success);
  EXPECT_FALSE(success);

  ApplyTimingInputString(scope.GetIsolate(), "duration", "NaN", success);
  EXPECT_FALSE(success);

  ApplyTimingInputString(scope.GetIsolate(), "duration", "rubbish", success);
  EXPECT_FALSE(success);
}

TEST_F(AnimationTimingInputTest, TimingInputDirection) {
  V8TestingScope scope;
  Timing::PlaybackDirection default_playback_direction =
      Timing::PlaybackDirection::NORMAL;
  bool ignored_success;

  EXPECT_EQ(Timing::PlaybackDirection::NORMAL,
            ApplyTimingInputString(scope.GetIsolate(), "direction", "normal",
                                   ignored_success)
                .direction);
  EXPECT_EQ(Timing::PlaybackDirection::REVERSE,
            ApplyTimingInputString(scope.GetIsolate(), "direction", "reverse",
                                   ignored_success)
                .direction);
  EXPECT_EQ(Timing::PlaybackDirection::ALTERNATE_NORMAL,
            ApplyTimingInputString(scope.GetIsolate(), "direction", "alternate",
                                   ignored_success)
                .direction);
  EXPECT_EQ(Timing::PlaybackDirection::ALTERNATE_REVERSE,
            ApplyTimingInputString(scope.GetIsolate(), "direction",
                                   "alternate-reverse", ignored_success)
                .direction);
  EXPECT_EQ(default_playback_direction,
            ApplyTimingInputString(scope.GetIsolate(), "direction", "rubbish",
                                   ignored_success)
                .direction);
  EXPECT_EQ(default_playback_direction,
            ApplyTimingInputNumber(scope.GetIsolate(), "direction", 2,
                                   ignored_success)
                .direction);
}

TEST_F(AnimationTimingInputTest, TimingInputTimingFunction) {
  V8TestingScope scope;
  const scoped_refptr<TimingFunction> default_timing_function =
      LinearTimingFunction::Shared();
  bool success;

  EXPECT_EQ(
      *CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE),
      *ApplyTimingInputString(scope.GetIsolate(), "easing", "ease", success)
           .timing_function);
  EXPECT_TRUE(success);
  EXPECT_EQ(
      *CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_IN),
      *ApplyTimingInputString(scope.GetIsolate(), "easing", "ease-in", success)
           .timing_function);
  EXPECT_TRUE(success);
  EXPECT_EQ(
      *CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_OUT),
      *ApplyTimingInputString(scope.GetIsolate(), "easing", "ease-out", success)
           .timing_function);
  EXPECT_TRUE(success);
  EXPECT_EQ(*CubicBezierTimingFunction::Preset(
                CubicBezierTimingFunction::EaseType::EASE_IN_OUT),
            *ApplyTimingInputString(scope.GetIsolate(), "easing", "ease-in-out",
                                    success)
                 .timing_function);
  EXPECT_TRUE(success);
  EXPECT_EQ(
      *LinearTimingFunction::Shared(),
      *ApplyTimingInputString(scope.GetIsolate(), "easing", "linear", success)
           .timing_function);
  EXPECT_TRUE(success);
  EXPECT_EQ(
      *StepsTimingFunction::Preset(StepsTimingFunction::StepPosition::START),
      *ApplyTimingInputString(scope.GetIsolate(), "easing", "step-start",
                              success)
           .timing_function);
  EXPECT_TRUE(success);
  EXPECT_EQ(
      *StepsTimingFunction::Preset(StepsTimingFunction::StepPosition::END),
      *ApplyTimingInputString(scope.GetIsolate(), "easing", "step-end", success)
           .timing_function);
  EXPECT_TRUE(success);
  EXPECT_EQ(*CubicBezierTimingFunction::Create(1, 1, 0.3, 0.3),
            *ApplyTimingInputString(scope.GetIsolate(), "easing",
                                    "cubic-bezier(1, 1, 0.3, 0.3)", success)
                 .timing_function);
  EXPECT_TRUE(success);
  EXPECT_EQ(
      *StepsTimingFunction::Create(3, StepsTimingFunction::StepPosition::START),
      *ApplyTimingInputString(scope.GetIsolate(), "easing", "steps(3, start)",
                              success)
           .timing_function);
  EXPECT_TRUE(success);
  EXPECT_EQ(
      *StepsTimingFunction::Create(5, StepsTimingFunction::StepPosition::END),
      *ApplyTimingInputString(scope.GetIsolate(), "easing", "steps(5, end)",
                              success)
           .timing_function);
  EXPECT_TRUE(success);

  ApplyTimingInputString(scope.GetIsolate(), "easing", "", success);
  EXPECT_FALSE(success);
  ApplyTimingInputString(scope.GetIsolate(), "easing", "steps(5.6, end)",
                         success);
  EXPECT_FALSE(success);
  ApplyTimingInputString(scope.GetIsolate(), "easing",
                         "cubic-bezier(2, 2, 0.3, 0.3)", success);
  EXPECT_FALSE(success);
  ApplyTimingInputString(scope.GetIsolate(), "easing", "rubbish", success);
  EXPECT_FALSE(success);
  ApplyTimingInputNumber(scope.GetIsolate(), "easing", 2, success);
  EXPECT_FALSE(success);
  ApplyTimingInputString(scope.GetIsolate(), "easing", "initial", success);
  EXPECT_FALSE(success);
}

TEST_F(AnimationTimingInputTest, TimingInputEmpty) {
  DummyExceptionStateForTesting exception_state;
  Timing control_timing;
  auto* timing_input =
      MakeGarbageCollected<V8UnionKeyframeEffectOptionsOrUnrestrictedDouble>(
          KeyframeEffectOptions::Create());
  Timing updated_timing =
      TimingInput::Convert(timing_input, nullptr, exception_state);
  EXPECT_FALSE(exception_state.HadException());

  EXPECT_EQ(control_timing.start_delay.AsTimeValue(),
            updated_timing.start_delay.AsTimeValue());
  EXPECT_EQ(control_timing.fill_mode, updated_timing.fill_mode);
  EXPECT_EQ(control_timing.iteration_start, updated_timing.iteration_start);
  EXPECT_EQ(control_timing.iteration_count, updated_timing.iteration_count);
  EXPECT_FALSE(updated_timing.iteration_duration);
  EXPECT_EQ(control_timing.direction, updated_timing.direction);
  EXPECT_EQ(*control_timing.timing_function, *updated_timing.timing_function);
}

TEST_F(AnimationTimingInputTest, TimingInputEmptyKeyframeAnimationOptions) {
  DummyExceptionStateForTesting exception_state;
  Timing control_timing;
  auto* input_timing =
      MakeGarbageCollected<V8UnionKeyframeAnimationOptionsOrUnrestrictedDouble>(
          KeyframeAnimationOptions::Create());
  Timing updated_timing =
      TimingInput::Convert(input_timing, nullptr, exception_state);
  EXPECT_FALSE(exception_state.HadException());

  EXPECT_EQ(control_timing.start_delay.AsTimeValue(),
            updated_timing.start_delay.AsTimeValue());
  EXPECT_EQ(control_timing.fill_mode, updated_timing.fill_mode);
  EXPECT_EQ(control_timing.iteration_start, updated_timing.iteration_start);
  EXPECT_EQ(control_timing.iteration_count, updated_timing.iteration_count);
  EXPECT_FALSE(updated_timing.iteration_duration);
  EXPECT_EQ(control_timing.direction, updated_timing.direction);
  EXPECT_EQ(*control_timing.timing_function, *updated_timing.timing_function);
}

}  // namespace blink
