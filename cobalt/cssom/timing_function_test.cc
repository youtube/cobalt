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

#include "cobalt/cssom/timing_function.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

namespace {
const float kErrorEpsilon = 0.00015f;
}  // namespace

TEST(TimingFunctionTest, BasicCubicBezierTest) {
  scoped_refptr<TimingFunction> function(
      new CubicBezierTimingFunction(0.25f, 0.0f, 0.75f, 1.0f));

  EXPECT_NEAR(function->Evaluate(0.0f), 0.0f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.05f), 0.01136f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.1f), 0.03978f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.15f), 0.079780f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.2f), 0.12803f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.25f), 0.18235f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.3f), 0.24115f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.35f), 0.30323f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.4f), 0.36761f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.45f), 0.43345f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.5f), 0.5f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.6f), 0.63238f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.65f), 0.69676f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.7f), 0.75884f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.75f), 0.81764f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.8f), 0.87196f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.85f), 0.92021f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.9f), 0.96021f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.95f), 0.98863f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(1.0f), 1.0f, kErrorEpsilon);
}

TEST(TimingFunctionTest, LinearCubicBezierTest) {
  scoped_refptr<TimingFunction> function(TimingFunction::GetLinear());

  EXPECT_NEAR(function->Evaluate(0.0f), 0.0f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.1f), 0.1f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.2f), 0.2f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.3f), 0.3f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.4f), 0.4f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.5f), 0.5f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.6f), 0.6f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.7f), 0.7f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.8f), 0.8f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.9f), 0.9f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(1.0f), 1.0f, kErrorEpsilon);
}

// Tests that solving the bezier works with knots with y not in (0, 1).
TEST(TimingFunctionTest, UnclampedYValuesCubicBezierTest) {
  scoped_refptr<TimingFunction> function(
      new CubicBezierTimingFunction(0.5f, -1.0f, 0.5f, 2.0f));

  EXPECT_NEAR(function->Evaluate(0.0f), 0.0f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.05f), -0.08954f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.1f), -0.15613f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.15f), -0.19641f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.2f), -0.20651f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.25f), -0.18232f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.3f), -0.11992f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.35f), -0.01672f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.4f), 0.12660f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.45f), 0.30349f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.5f), 0.50000f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.55f), 0.69651f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.6f), 0.87340f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.65f), 1.01672f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.7f), 1.11992f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.75f), 1.18232f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.8f), 1.20651f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.85f), 1.19641f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.9f), 1.15613f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.95f), 1.08954f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(1.0f), 1.0f, kErrorEpsilon);
}

TEST(TimingFunctionTest, StepStartTest) {
  scoped_refptr<TimingFunction> function(TimingFunction::GetStepStart());

  EXPECT_NEAR(function->Evaluate(0.0f), 1.0f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.5f), 1.0f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(1.0f), 1.0f, kErrorEpsilon);
}

TEST(TimingFunctionTest, StepEndTest) {
  scoped_refptr<TimingFunction> function(TimingFunction::GetStepEnd());

  EXPECT_NEAR(function->Evaluate(0.0f), 0.0f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.5f), 0.0f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(1.0f), 1.0f, kErrorEpsilon);
}

TEST(TimingFunctionTest, MultiStepEndTest) {
  scoped_refptr<TimingFunction> function(
      new SteppingTimingFunction(4, SteppingTimingFunction::kEnd));

  EXPECT_NEAR(function->Evaluate(0.000f), 0.00f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.125f), 0.00f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.250f), 0.25f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.375f), 0.25f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.500f), 0.50f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.625f), 0.50f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.750f), 0.75f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.875f), 0.75f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(1.000f), 1.00f, kErrorEpsilon);
}

TEST(TimingFunctionTest, MultiStepStartTest) {
  scoped_refptr<TimingFunction> function(
      new SteppingTimingFunction(4, SteppingTimingFunction::kStart));

  EXPECT_NEAR(function->Evaluate(0.000f), 0.25f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.125f), 0.25f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.250f), 0.50f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.375f), 0.50f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.500f), 0.75f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.625f), 0.75f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.750f), 1.00f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(0.875f), 1.00f, kErrorEpsilon);
  EXPECT_NEAR(function->Evaluate(1.000f), 1.00f, kErrorEpsilon);
}

}  // namespace cssom
}  // namespace cobalt
