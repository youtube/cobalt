// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MATH_CUBIC_BEZIER_H_
#define COBALT_MATH_CUBIC_BEZIER_H_

namespace cobalt {
namespace math {

// CubicBezier implements functionality required to evaluate the cubic-bezier()
// class of timing functions as defined here:
//   https://developer.mozilla.org/en-US/docs/Web/CSS/timing-function
//
// A cubic Bezier curve is defined by four points P0, P1, P2, and P3.
// P0 and P3 are the start and the end of the curve and in CSS these points
// are fixed and defined as (0, 0) and (1, 1) respectively.
//
// Given P1=(x1, y1) and P2=(x2, y2), for each x in [0, 1] the following code
// will find y, such that P=(x, y) resides on the curve.
//
//   const CubicBezier curve(x1, y1, x2, y2);
//   const double y = curve.Solve(x);
//
class CubicBezier {
 public:
  CubicBezier(double x1, double y1, double x2, double y2);
  ~CubicBezier();

  // Returns an approximation of y at the given x.
  double Solve(double x) const;

  // Returns an approximation of dy/dx at the given x.
  double Slope(double x) const;

  // Sets |min| and |max| to the bezier's minimum and maximum y values in the
  // interval [0, 1].
  void Range(double* min, double* max) const;

  double x1() const { return x1_; }
  double y1() const { return y1_; }
  double x2() const { return x2_; }
  double y2() const { return y2_; }

  bool operator==(const CubicBezier& other) const {
    return x1_ == other.x1_ && y1_ == other.y1_ && x2_ == other.x2_ &&
           y2_ == other.y2_;
  }

 private:
  double x1_;
  double y1_;
  double x2_;
  double y2_;
};

}  // namespace math
}  // namespace cobalt

#endif  // COBALT_MATH_CUBIC_BEZIER_H_
