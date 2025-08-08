// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/paint_rendering_context_2d.h"

#include "cc/paint/paint_op.h"
#include "cc/test/paint_op_matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style_test_utils.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/recording_test_utils.h"

namespace blink {
namespace {

using ::blink_testing::RecordedOpsAre;
using ::cc::ConcatOp;
using ::cc::DrawColorOp;
using ::cc::PaintOpEq;
using ::cc::ScaleOp;
using ::cc::SetMatrixOp;

static const int kWidth = 50;
static const int kHeight = 75;

void TrySettingStrokeStyle(V8TestingScope& v8_testing_scope,
                           PaintRenderingContext2D* ctx,
                           const String& expected,
                           const String& value) {
  auto* script_state = v8_testing_scope.GetScriptState();
  SetStrokeStyleString(ctx, script_state, "red");
  SetStrokeStyleString(ctx, script_state, value);
  EXPECT_EQ(expected, GetStrokeStyleAsString(ctx, script_state));
}

TEST(PaintRenderingContext2DTest, testParseColorOrCurrentColor) {
  V8TestingScope v8_testing_scope;
  PaintRenderingContext2DSettings* context_settings =
      PaintRenderingContext2DSettings::Create();
  context_settings->setAlpha(false);
  PaintRenderingContext2D* ctx = MakeGarbageCollected<PaintRenderingContext2D>(
      gfx::Size(kWidth, kHeight), context_settings, /*zoom=*/1,
      scheduler::GetSingleThreadTaskRunnerForTesting());
  TrySettingStrokeStyle(v8_testing_scope, ctx, "#0000ff", "blue");
  TrySettingStrokeStyle(v8_testing_scope, ctx, "#000000", "currentColor");
}

TEST(PaintRenderingContext2DTest, testWidthAndHeight) {
  PaintRenderingContext2DSettings* context_settings =
      PaintRenderingContext2DSettings::Create();
  PaintRenderingContext2D* ctx = MakeGarbageCollected<PaintRenderingContext2D>(
      gfx::Size(kWidth, kHeight), context_settings, /*zoom=*/1,
      scheduler::GetSingleThreadTaskRunnerForTesting());
  EXPECT_EQ(kWidth, ctx->Width());
  EXPECT_EQ(kHeight, ctx->Height());
}

TEST(PaintRenderingContext2DTest, testBasicState) {
  PaintRenderingContext2DSettings* context_settings =
      PaintRenderingContext2DSettings::Create();
  PaintRenderingContext2D* ctx = MakeGarbageCollected<PaintRenderingContext2D>(
      gfx::Size(kWidth, kHeight), context_settings, /*zoom=*/1,
      scheduler::GetSingleThreadTaskRunnerForTesting());

  const double kShadowBlurBefore = 2;
  const double kShadowBlurAfter = 3;

  const String line_join_before = "bevel";
  const String line_join_after = "round";

  ctx->setShadowBlur(kShadowBlurBefore);
  ctx->setLineJoin(line_join_before);
  EXPECT_EQ(kShadowBlurBefore, ctx->shadowBlur());
  EXPECT_EQ(line_join_before, ctx->lineJoin());

  ctx->save();

  ctx->setShadowBlur(kShadowBlurAfter);
  ctx->setLineJoin(line_join_after);
  EXPECT_EQ(kShadowBlurAfter, ctx->shadowBlur());
  EXPECT_EQ(line_join_after, ctx->lineJoin());

  NonThrowableExceptionState exception_state;
  ctx->restore(exception_state);

  EXPECT_EQ(kShadowBlurBefore, ctx->shadowBlur());
  EXPECT_EQ(line_join_before, ctx->lineJoin());
}

TEST(PaintRenderingContext2DTest, setTransformWithDeviceScaleFactor) {
  PaintRenderingContext2DSettings* context_settings =
      PaintRenderingContext2DSettings::Create();
  float zoom = 1.23;
  PaintRenderingContext2D* ctx = MakeGarbageCollected<PaintRenderingContext2D>(
      gfx::Size(kWidth, kHeight), context_settings, zoom,
      scheduler::GetSingleThreadTaskRunnerForTesting());
  DOMMatrix* matrix = ctx->getTransform();
  EXPECT_TRUE(matrix->isIdentity());
  ctx->setTransform(2.1, 2.5, 1.4, 2.3, 20, 50);
  matrix = ctx->getTransform();
  EXPECT_FLOAT_EQ(matrix->a(), 2.1);
  EXPECT_FLOAT_EQ(matrix->b(), 2.5);
  EXPECT_FLOAT_EQ(matrix->c(), 1.4);
  EXPECT_FLOAT_EQ(matrix->d(), 2.3);
  EXPECT_FLOAT_EQ(matrix->e(), 20);
  EXPECT_FLOAT_EQ(matrix->f(), 50);

  EXPECT_THAT(ctx->GetRecord(),
              RecordedOpsAre(PaintOpEq<ScaleOp>(1.23, 1.23),
                             PaintOpEq<DrawColorOp>(SkColors::kTransparent,
                                                    SkBlendMode::kSrc),
                             PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                                          0, 1, 0, 0,  //
                                                          0, 0, 1, 0,  //
                                                          0, 0, 0, 1)),
                             PaintOpEq<ConcatOp>(SkM44(zoom, 0, 0, 0,  //
                                                       0, zoom, 0, 0,  //
                                                       0, 0, 1, 0,     //
                                                       0, 0, 0, 1)),
                             PaintOpEq<ConcatOp>(SkM44(2.1, 1.4, 0, 20,  //
                                                       2.5, 2.3, 0, 50,  //
                                                       0, 0, 1, 0,       //
                                                       0, 0, 0, 1))));
}

TEST(PaintRenderingContext2DTest, setTransformWithDefaultDeviceScaleFactor) {
  PaintRenderingContext2DSettings* context_settings =
      PaintRenderingContext2DSettings::Create();
  PaintRenderingContext2D* ctx = MakeGarbageCollected<PaintRenderingContext2D>(
      gfx::Size(kWidth, kHeight), context_settings, /*zoom=*/1,
      scheduler::GetSingleThreadTaskRunnerForTesting());
  DOMMatrix* matrix = ctx->getTransform();
  EXPECT_TRUE(matrix->isIdentity());
  ctx->setTransform(1.2, 2.3, 3.4, 4.5, 56, 67);
  matrix = ctx->getTransform();
  EXPECT_FLOAT_EQ(matrix->a(), 1.2);
  EXPECT_FLOAT_EQ(matrix->b(), 2.3);
  EXPECT_FLOAT_EQ(matrix->c(), 3.4);
  EXPECT_FLOAT_EQ(matrix->d(), 4.5);
  EXPECT_FLOAT_EQ(matrix->e(), 56);
  EXPECT_FLOAT_EQ(matrix->f(), 67);

  EXPECT_THAT(ctx->GetRecord(),
              RecordedOpsAre(PaintOpEq<DrawColorOp>(SkColors::kTransparent,
                                                    SkBlendMode::kSrc),
                             PaintOpEq<ConcatOp>(SkM44(1.2, 3.4, 0, 56,  //
                                                       2.3, 4.5, 0, 67,  //
                                                       0, 0, 1, 0,       //
                                                       0, 0, 0, 1))));
}

}  // namespace
}  // namespace blink
