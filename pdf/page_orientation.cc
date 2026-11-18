// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/page_orientation.h"

#include <type_traits>

#include "base/check_op.h"
#include "base/notreached.h"

namespace chrome_pdf {

namespace {

using PageOrientationIntType = std::underlying_type<PageOrientation>::type;

constexpr auto kOrientationCount =
    static_cast<PageOrientationIntType>(PageOrientation::kLast) + 1;

// Adds two PageOrientation values together. This works because the underlying
// integer values have been chosen to allow modular arithmetic.
PageOrientation AddOrientations(PageOrientation first, PageOrientation second) {
  auto first_int = static_cast<PageOrientationIntType>(first);
  auto second_int = static_cast<PageOrientationIntType>(second);
  return static_cast<PageOrientation>((first_int + second_int) %
                                      kOrientationCount);
}

}  // namespace

PageOrientation PageOrientationFromClockwiseRotationSteps(int steps) {
  CHECK_GE(steps, 0);
  switch (steps % kOrientationCount) {
    case 0:
      return PageOrientation::kOriginal;
    case 1:
      return PageOrientation::kClockwise90;
    case 2:
      return PageOrientation::kClockwise180;
    case 3:
      return PageOrientation::kClockwise270;
  }
  NOTREACHED();
}

bool IsTransposedPageOrientation(PageOrientation orientation) {
  switch (orientation) {
    case PageOrientation::kOriginal:
    case PageOrientation::kClockwise180:
      return false;
    case PageOrientation::kClockwise90:
    case PageOrientation::kClockwise270:
      return true;
  }
  NOTREACHED();
}

PageOrientation RotateClockwise(PageOrientation orientation) {
  return AddOrientations(orientation, PageOrientation::kClockwise90);
}

PageOrientation RotateCounterclockwise(PageOrientation orientation) {
  // Adding `kLast` is equivalent to rotating one step counterclockwise.
  return AddOrientations(orientation, PageOrientation::kLast);
}

}  // namespace chrome_pdf
