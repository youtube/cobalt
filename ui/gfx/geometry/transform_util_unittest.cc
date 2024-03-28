// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/transform_util.h"

#include <stddef.h>

#include "base/numerics/math_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace gfx {
namespace {

#define EXPECT_APPROX_EQ(val1, val2) EXPECT_NEAR(val1, val2, 1e-6);

TEST(TransformUtilTest, GetScaleTransform) {
  const Point kAnchor(20, 40);
  const float kScale = 0.5f;

  Transform scale = GetScaleTransform(kAnchor, kScale);

  const int kOffset = 10;
  for (int sign_x = -1; sign_x <= 1; ++sign_x) {
    for (int sign_y = -1; sign_y <= 1; ++sign_y) {
      Point test(kAnchor.x() + sign_x * kOffset,
                 kAnchor.y() + sign_y * kOffset);
      scale.TransformPoint(&test);

      EXPECT_EQ(Point(kAnchor.x() + sign_x * kOffset * kScale,
                      kAnchor.y() + sign_y * kOffset * kScale),
                test);
    }
  }
}

TEST(TransformUtilTest, SnapRotation) {
  Transform result(Transform::kSkipInitialization);
  Transform transform;
  transform.RotateAboutZAxis(89.99);

  Rect viewport(1920, 1200);
  bool snapped = SnapTransform(&result, transform, viewport);

  EXPECT_TRUE(snapped) << "Viewport should snap for this rotation.";
}

TEST(TransformUtilTest, SnapRotationDistantViewport) {
  const int kOffset = 5000;
  Transform result(Transform::kSkipInitialization);
  Transform transform;

  transform.RotateAboutZAxis(89.99);

  Rect viewport(kOffset, kOffset, 1920, 1200);
  bool snapped = SnapTransform(&result, transform, viewport);

  EXPECT_FALSE(snapped) << "Distant viewport shouldn't snap by more than 1px.";
}

TEST(TransformUtilTest, NoSnapRotation) {
  Transform result(Transform::kSkipInitialization);
  Transform transform;
  const int kOffset = 5000;

  transform.RotateAboutZAxis(89.9);

  Rect viewport(kOffset, kOffset, 1920, 1200);
  bool snapped = SnapTransform(&result, transform, viewport);

  EXPECT_FALSE(snapped) << "Viewport should not snap for this rotation.";
}

// Translations should always be snappable, the most we would move is 0.5
// pixels towards either direction to the nearest value in each component.
TEST(TransformUtilTest, SnapTranslation) {
  Transform result(Transform::kSkipInitialization);
  Transform transform;

  transform.Translate3d(SkDoubleToScalar(1.01), SkDoubleToScalar(1.99),
                        SkDoubleToScalar(3.0));

  Rect viewport(1920, 1200);
  bool snapped = SnapTransform(&result, transform, viewport);

  EXPECT_TRUE(snapped) << "Viewport should snap for this translation.";
}

TEST(TransformUtilTest, SnapTranslationDistantViewport) {
  Transform result(Transform::kSkipInitialization);
  Transform transform;
  const int kOffset = 5000;

  transform.Translate3d(SkDoubleToScalar(1.01), SkDoubleToScalar(1.99),
                        SkDoubleToScalar(3.0));

  Rect viewport(kOffset, kOffset, 1920, 1200);
  bool snapped = SnapTransform(&result, transform, viewport);

  EXPECT_TRUE(snapped)
      << "Distant viewport should still snap by less than 1px.";
}

TEST(TransformUtilTest, SnapScale) {
  Transform result(Transform::kSkipInitialization);
  Transform transform;

  transform.Scale3d(SkDoubleToScalar(5.0), SkDoubleToScalar(2.00001),
                    SkDoubleToScalar(1.0));
  Rect viewport(1920, 1200);
  bool snapped = SnapTransform(&result, transform, viewport);

  EXPECT_TRUE(snapped) << "Viewport should snap for this scaling.";
}

TEST(TransformUtilTest, NoSnapScale) {
  Transform result(Transform::kSkipInitialization);
  Transform transform;

  transform.Scale3d(SkDoubleToScalar(5.0), SkDoubleToScalar(2.1),
                    SkDoubleToScalar(1.0));
  Rect viewport(1920, 1200);
  bool snapped = SnapTransform(&result, transform, viewport);

  EXPECT_FALSE(snapped) << "Viewport shouldn't snap for this scaling.";
}

TEST(TransformUtilTest, SnapCompositeTransform) {
  Transform result(Transform::kSkipInitialization);
  Transform transform;

  transform.Translate3d(SkDoubleToScalar(30.5), SkDoubleToScalar(20.0),
                        SkDoubleToScalar(10.1));
  transform.RotateAboutZAxis(89.99);
  transform.Scale3d(SkDoubleToScalar(1.0), SkDoubleToScalar(3.00001),
                    SkDoubleToScalar(2.0));

  Rect viewport(1920, 1200);
  bool snapped = SnapTransform(&result, transform, viewport);
  ASSERT_TRUE(snapped) << "Viewport should snap all components.";

  Point3F point;

  point = Point3F(PointF(viewport.origin()));
  result.TransformPoint(&point);
  EXPECT_EQ(Point3F(31.f, 20.f, 10.f), point) << "Transformed origin";

  point = Point3F(PointF(viewport.top_right()));
  result.TransformPoint(&point);
  EXPECT_EQ(Point3F(31.f, 1940.f, 10.f), point) << "Transformed top-right";

  point = Point3F(PointF(viewport.bottom_left()));
  result.TransformPoint(&point);
  EXPECT_EQ(Point3F(-3569.f, 20.f, 10.f), point) << "Transformed bottom-left";

  point = Point3F(PointF(viewport.bottom_right()));
  result.TransformPoint(&point);
  EXPECT_EQ(Point3F(-3569.f, 1940.f, 10.f), point)
      << "Transformed bottom-right";
}

TEST(TransformUtilTest, NoSnapSkewedCompositeTransform) {
  Transform result(Transform::kSkipInitialization);
  Transform transform;

  transform.RotateAboutZAxis(89.99);
  transform.Scale3d(SkDoubleToScalar(1.0), SkDoubleToScalar(3.00001),
                    SkDoubleToScalar(2.0));
  transform.Translate3d(SkDoubleToScalar(30.5), SkDoubleToScalar(20.0),
                        SkDoubleToScalar(10.1));
  transform.Skew(20.0, 0.0);
  Rect viewport(1920, 1200);
  bool snapped = SnapTransform(&result, transform, viewport);
  EXPECT_FALSE(snapped) << "Skewed viewport should not snap.";
}

TEST(TransformUtilTest, TransformAboutPivot) {
  Transform transform;
  transform.Scale(3, 4);
  transform = TransformAboutPivot(Point(7, 8), transform);

  Point point;

  point = Point(0, 0);
  transform.TransformPoint(&point);
  EXPECT_EQ(Point(-14, -24).ToString(), point.ToString());

  point = Point(1, 1);
  transform.TransformPoint(&point);
  EXPECT_EQ(Point(-11, -20).ToString(), point.ToString());
}

TEST(TransformUtilTest, BlendOppositeQuaternions) {
  DecomposedTransform first;
  DecomposedTransform second;
  second.quaternion.set_w(-second.quaternion.w());

  DecomposedTransform result = BlendDecomposedTransforms(first, second, 0.25);

  EXPECT_TRUE(std::isfinite(result.quaternion.x()));
  EXPECT_TRUE(std::isfinite(result.quaternion.y()));
  EXPECT_TRUE(std::isfinite(result.quaternion.z()));
  EXPECT_TRUE(std::isfinite(result.quaternion.w()));

  EXPECT_FALSE(std::isnan(result.quaternion.x()));
  EXPECT_FALSE(std::isnan(result.quaternion.y()));
  EXPECT_FALSE(std::isnan(result.quaternion.z()));
  EXPECT_FALSE(std::isnan(result.quaternion.w()));
}

double ComputeDecompRecompError(const Transform& transform) {
  DecomposedTransform decomp;
  DecomposeTransform(&decomp, transform);
  Transform composed = ComposeTransform(decomp);

  float expected[16];
  float actual[16];
  transform.matrix().asRowMajorf(expected);
  composed.matrix().asRowMajorf(actual);
  double sse = 0;
  for (int i = 0; i < 16; i++) {
    double diff = expected[i] - actual[i];
    sse += diff * diff;
  }
  return sse;
}

TEST(TransformUtilTest, RoundTripTest) {
  // rotateZ(90deg)
  EXPECT_APPROX_EQ(0, ComputeDecompRecompError(Transform(0, 1, -1, 0, 0, 0)));

  // rotateZ(180deg)
  // Edge case where w = 0.
  EXPECT_APPROX_EQ(0, ComputeDecompRecompError(Transform(-1, 0, 0, -1, 0, 0)));

  // rotateX(90deg) rotateY(90deg) rotateZ(90deg)
  // [1  0   0][ 0 0 1][0 -1 0]   [0 0 1][0 -1 0]   [0  0 1]
  // [0  0  -1][ 0 1 0][1  0 0] = [1 0 0][1  0 0] = [0 -1 0]
  // [0  1   0][-1 0 0][0  0 1]   [0 1 0][0  0 1]   [1  0 0]
  // This test case leads to Gimbal lock when using Euler angles.
  EXPECT_APPROX_EQ(0, ComputeDecompRecompError(Transform(
                          0, 0, 1, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1)));

  // Quaternion matrices with 0 off-diagonal elements, and negative trace.
  // Stress tests handling of degenerate cases in computing quaternions.
  // Validates fix for https://crbug.com/647554.
  EXPECT_APPROX_EQ(0, ComputeDecompRecompError(Transform(1, 1, 1, 0, 0, 0)));
  EXPECT_APPROX_EQ(0, ComputeDecompRecompError(Transform(
                          -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)));
  EXPECT_APPROX_EQ(0, ComputeDecompRecompError(Transform(
                          1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)));
  EXPECT_APPROX_EQ(0, ComputeDecompRecompError(Transform(
                          1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1)));
}

TEST(TransformUtilTest, Transform2D) {
  // The spec covering interpolation of 2D matrix transforms calls for inverting
  // one of the axis in the case of a negative determinant.  This differs from
  // the general 3D spec, which calls for flipping all of the scales when the
  // determinant is negative. Flipping all scales not only introduces rotation
  // in the case of a trivial scale inversion, but causes transformed objects
  // to needlessly shrink and grow as they transform through scale = 0 along
  // multiple axes.  2D transformation matrices should follow the 2D spec
  // regarding matrix decomposition.
  DecomposedTransform decompFlipX;
  DecomposeTransform(&decompFlipX, Transform(-1, 0, 0, 1, 0, 0));
  EXPECT_APPROX_EQ(-1, decompFlipX.scale[0]);
  EXPECT_APPROX_EQ(1, decompFlipX.scale[1]);
  EXPECT_APPROX_EQ(1, decompFlipX.scale[2]);
  EXPECT_APPROX_EQ(0, decompFlipX.quaternion.z());
  EXPECT_APPROX_EQ(1, decompFlipX.quaternion.w());

  DecomposedTransform decompFlipY;
  DecomposeTransform(&decompFlipY, Transform(1, 0, 0, -1, 0, 0));
  EXPECT_APPROX_EQ(1, decompFlipY.scale[0]);
  EXPECT_APPROX_EQ(-1, decompFlipY.scale[1]);
  EXPECT_APPROX_EQ(1, decompFlipY.scale[2]);
  EXPECT_APPROX_EQ(0, decompFlipY.quaternion.z());
  EXPECT_APPROX_EQ(1, decompFlipY.quaternion.w());

  DecomposedTransform decompR180;
  DecomposeTransform(&decompR180, Transform(-1, 0, 0, -1, 0, 0));
  EXPECT_APPROX_EQ(1, decompR180.scale[0]);
  EXPECT_APPROX_EQ(1, decompR180.scale[1]);
  EXPECT_APPROX_EQ(1, decompR180.scale[2]);
  EXPECT_APPROX_EQ(1, decompR180.quaternion.z());
  EXPECT_APPROX_EQ(0, decompR180.quaternion.w());

  DecomposedTransform decompR90;
  DecomposeTransform(&decompR180, Transform(0, -1, 1, 0, 0, 0));
  EXPECT_APPROX_EQ(1, decompR180.scale[0]);
  EXPECT_APPROX_EQ(1, decompR180.scale[1]);
  EXPECT_APPROX_EQ(1, decompR180.scale[2]);
  EXPECT_APPROX_EQ(1 / sqrt(2), decompR180.quaternion.z());
  EXPECT_APPROX_EQ(1 / sqrt(2), decompR180.quaternion.w());

  DecomposedTransform decompR90Translate;
  DecomposeTransform(&decompR90Translate, Transform(0, -1, 1, 0, -1, 1));
  EXPECT_APPROX_EQ(1, decompR90Translate.scale[0]);
  EXPECT_APPROX_EQ(1, decompR90Translate.scale[1]);
  EXPECT_APPROX_EQ(1, decompR90Translate.scale[2]);
  EXPECT_APPROX_EQ(-1, decompR90Translate.translate[0]);
  EXPECT_APPROX_EQ(1, decompR90Translate.translate[1]);
  EXPECT_APPROX_EQ(0, decompR90Translate.translate[2]);
  EXPECT_APPROX_EQ(1 / sqrt(2), decompR90Translate.quaternion.z());
  EXPECT_APPROX_EQ(1 / sqrt(2), decompR90Translate.quaternion.w());

  DecomposedTransform decompSkewRotate;
  DecomposeTransform(&decompR90Translate, Transform(1, 1, 1, 0, 0, 0));
  EXPECT_APPROX_EQ(sqrt(2), decompR90Translate.scale[0]);
  EXPECT_APPROX_EQ(-1 / sqrt(2), decompR90Translate.scale[1]);
  EXPECT_APPROX_EQ(1, decompR90Translate.scale[2]);
  EXPECT_APPROX_EQ(-1, decompR90Translate.skew[0]);
  EXPECT_APPROX_EQ(sin(base::kPiDouble / 8), decompR90Translate.quaternion.z());
  EXPECT_APPROX_EQ(cos(base::kPiDouble / 8), decompR90Translate.quaternion.w());
}

TEST(TransformUtilTest, TransformBetweenRects) {
  auto verify = [](const RectF& src_rect, const RectF& dst_rect) {
    const Transform transform = TransformBetweenRects(src_rect, dst_rect);

    // Applies |transform| to calculate the target rectangle from |src_rect|.
    // Notes that |transform| is in |src_rect|'s local coordinates.
    RectF dst_in_src_coordinates = RectF(src_rect.size());
    transform.TransformRect(&dst_in_src_coordinates);
    RectF dst_in_parent_coordinates = dst_in_src_coordinates;
    dst_in_parent_coordinates.Offset(src_rect.OffsetFromOrigin());

    // Verifies that the target rectangle is expected.
    EXPECT_EQ(dst_rect, dst_in_parent_coordinates);
  };

  std::vector<const std::pair<const RectF, const RectF>> test_cases{
      {RectF(0.f, 0.f, 2.f, 3.f), RectF(3.f, 5.f, 4.f, 9.f)},
      {RectF(10.f, 7.f, 2.f, 6.f), RectF(4.f, 2.f, 1.f, 12.f)},
      {RectF(0.f, 0.f, 3.f, 5.f), RectF(0.f, 0.f, 6.f, 2.5f)}};

  for (const auto& test_case : test_cases) {
    verify(test_case.first, test_case.second);
    verify(test_case.second, test_case.first);
  }

  // Tests the case where the destination is an empty rectangle.
  verify(RectF(0.f, 0.f, 3.f, 5.f), RectF());
}

}  // namespace
}  // namespace gfx
