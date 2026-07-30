/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/common/q_format_or_floating_point.h"

#include <cstdint>

#include "absl/status/statusor.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"

namespace iamf_tools {

absl::StatusOr<QFormatOrFloatingPoint>
QFormatOrFloatingPoint::CreateFromFloatingPoint(float value) {
  int16_t q7_8;
  RETURN_IF_NOT_OK(FloatToQ7_8(value, q7_8));

  // Note that the constructor will recompute the float representation, which
  // may vary slightly from the original value because it will snap to a value
  // directly representable in Q7.8.
  return QFormatOrFloatingPoint(q7_8);
}

QFormatOrFloatingPoint QFormatOrFloatingPoint::MakeFromQ7_8(int16_t q7_8) {
  return QFormatOrFloatingPoint(q7_8);
}

QFormatOrFloatingPoint::QFormatOrFloatingPoint(int16_t q7_8)
    : q7_8_(q7_8), floating_point_(Q7_8ToFloat(q7_8)) {}

}  // namespace iamf_tools
