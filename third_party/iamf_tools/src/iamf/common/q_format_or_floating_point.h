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

#ifndef COMMON_Q_FORMAT_OR_FLOATING_POINT_H_
#define COMMON_Q_FORMAT_OR_FLOATING_POINT_H_

#include <cstdint>

#include "absl/status/statusor.h"

namespace iamf_tools {

/*!\brief Wraps a value that may be interpreted as a Q-format or floating point.
 *
 * This class is constructed via factory functions and is useful when a value
 * may be used as either Q-format or floating point in different contexts. For
 * example, the IAMF bitstream represents various gain and loudness values as
 * Q7.8 fixed point. However, many mathematical operations are simpler to
 * implement as the floating-point equivalent.
 */
class QFormatOrFloatingPoint {
  // TODO(b/391851526): Add support for Q0.8 format.
 public:
  /*!\brief Creates an instance from a Q7.8 value.
   *
   * \param q7_8 Q7.8 value to initialize from.
   * \return `QFormatOrFloatingPoint` created from `q7_8`.
   */
  static QFormatOrFloatingPoint MakeFromQ7_8(int16_t q7_8);

  /*!\brief Creates an instance from a floating point value.
   *
   * \param value Floating point value to initialize from.
   * \return `QFormatOrFloatingPoint` on success. `absl::InvalidArgumentError`
   *         if `value` is outside the representable range of Q7.8 format.
   */
  static absl::StatusOr<QFormatOrFloatingPoint> CreateFromFloatingPoint(
      float value);

  /*!\brief Absl print function for `QFormatOrFloatingPoint`.*/
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const QFormatOrFloatingPoint& p) {
    absl::Format(&sink, "QFormatOrFloatingPoint(q7.8=%d, float=%f)",
                 p.GetQ7_8(), p.GetFloatingPoint());
  }

  friend bool operator==(const QFormatOrFloatingPoint& lhs,
                         const QFormatOrFloatingPoint& rhs) = default;

  /*!\brief Gets the value as Q7.8.
   *
   * \return Value represented as Q7.8.
   */
  int16_t GetQ7_8() const { return q7_8_; }

  /*!\brief Gets the value as floating-point.
   *
   * \return Value represented as floating-point.
   */
  float GetFloatingPoint() const { return floating_point_; }

 private:
  /*!\brief Constructor.
   *
   * \param q78 Q7.8 value to initialize from.
   */
  explicit QFormatOrFloatingPoint(int16_t q7_8);

  int16_t q7_8_;
  float floating_point_;
};

}  // namespace iamf_tools

#endif  // COMMON_Q_FORMAT_OR_FLOATING_POINT_H_
