/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef OBU_MIX_GAIN_PARAMETER_DATA_H_
#define OBU_MIX_GAIN_PARAMETER_DATA_H_

#include <cstdint>
#include <variant>

#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
/*!\brief The metadata to describe animation of type `kAnimateStep`. */
struct AnimationStepInt16 {
  friend bool operator==(const AnimationStepInt16& lhs,
                         const AnimationStepInt16& rhs) = default;

  /*!\brief Prints the `AnimationStepInt16`.
   */
  void Print() const;

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const;

  /*!\brief Reads and validates the `AnimationStepInt16` from a buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` unless the buffer is exhausted during reading.
   */
  absl::Status ReadAndValidate(ReadBitBuffer& rb);

  int16_t start_point_value;
};

/*!\brief The metadata to describe animation of type `kAnimateLinear`. */
struct AnimationLinearInt16 {
  friend bool operator==(const AnimationLinearInt16& lhs,
                         const AnimationLinearInt16& rhs) = default;

  /*!\brief Prints the `AnimationLinearInt16`.
   */
  void Print() const;

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const;

  /*!\brief Reads and validates the `AnimationLinearInt16` from a buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` unless the buffer is exhausted during reading.
   */
  absl::Status ReadAndValidate(ReadBitBuffer& rb);

  int16_t start_point_value;
  int16_t end_point_value;
};

/*!\brief The metadata to describe animation of type `kAnimateBezier`. */
struct AnimationBezierInt16 {
  friend bool operator==(const AnimationBezierInt16& lhs,
                         const AnimationBezierInt16& rhs) = default;

  /*!\brief Prints the `AnimationBezierInt16`.
   */
  void Print() const;

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const;

  /*!\brief Reads and validates the `AnimationBezierInt16` from a buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` unless the buffer is exhausted during reading.
   */
  absl::Status ReadAndValidate(ReadBitBuffer& rb);

  int16_t start_point_value;
  int16_t end_point_value;
  int16_t control_point_value;
  uint8_t control_point_relative_time;  // Q0.8 format.
};

struct MixGainParameterData : public ParameterData {
  /*!\brief A `DecodedUleb128` enum for the type of animation to apply. */
  enum AnimationType : DecodedUleb128 {
    kAnimateStep = 0,
    kAnimateLinear = 1,
    kAnimateBezier = 2,
  };

  /*!\brief Constructor.
   *
   * \param input_param_data Input metadata describing the animation type.
   */
  explicit MixGainParameterData(
      const std::variant<AnimationStepInt16, AnimationLinearInt16,
                         AnimationBezierInt16>& input_param_data)
      : ParameterData(), param_data(input_param_data) {}
  MixGainParameterData() = default;

  /*!\brief Overridden destructor.*/
  ~MixGainParameterData() override = default;

  /*!\brief Reads and validates a `MixGainParameterData` from a buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()`. Or a specific error code on failure.
   */
  absl::Status ReadAndValidate(ReadBitBuffer& rb) override;

  /*!\brief Gets the animation type of the parameter data.
   *
   * \return Animation type.
   */
  AnimationType GetAnimationType() const;

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Write(WriteBitBuffer& wb) const override;

  /*!\brief Prints the mix gain parameter data.
   */
  void Print() const override;

  // The animation type is serialized base on the active field of the variant.
  std::variant<AnimationStepInt16, AnimationLinearInt16, AnimationBezierInt16>
      param_data;
};

}  // namespace iamf_tools

#endif  // OBU_MIX_GAIN_PARAMETER_DATA_H_
