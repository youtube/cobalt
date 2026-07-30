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
#include "iamf/obu/mix_gain_parameter_data.h"

#include <variant>

#include "absl/functional/overload.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

void AnimationStepInt16::Print() const {
  ABSL_LOG(INFO) << "     // Step";
  ABSL_LOG(INFO) << "     start_point_value= " << start_point_value;
}

absl::Status AnimationStepInt16::ValidateAndWrite(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteSigned16(start_point_value));
  return absl::OkStatus();
}

absl::Status AnimationStepInt16::ReadAndValidate(ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(rb.ReadSigned16(start_point_value));
  return absl::OkStatus();
}

void AnimationLinearInt16::Print() const {
  ABSL_LOG(INFO) << "     // Linear";
  ABSL_LOG(INFO) << "     start_point_value= " << start_point_value;
  ABSL_LOG(INFO) << "     end_point_value= " << end_point_value;
}

absl::Status AnimationLinearInt16::ValidateAndWrite(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteSigned16(start_point_value));
  RETURN_IF_NOT_OK(wb.WriteSigned16(end_point_value));
  return absl::OkStatus();
}

absl::Status AnimationLinearInt16::ReadAndValidate(ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(rb.ReadSigned16(start_point_value));
  RETURN_IF_NOT_OK(rb.ReadSigned16(end_point_value));
  return absl::OkStatus();
}

void AnimationBezierInt16::Print() const {
  ABSL_LOG(INFO) << "     // Bezier";
  ABSL_LOG(INFO) << "     start_point_value= " << start_point_value;
  ABSL_LOG(INFO) << "     end_point_value= " << end_point_value;
  ABSL_LOG(INFO) << "     control_point_value= " << control_point_value;
  ABSL_LOG(INFO) << "     control_point_relative_time= "
                 << control_point_relative_time;
}

absl::Status AnimationBezierInt16::ValidateAndWrite(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteSigned16(start_point_value));
  RETURN_IF_NOT_OK(wb.WriteSigned16(end_point_value));
  RETURN_IF_NOT_OK(wb.WriteSigned16(control_point_value));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(control_point_relative_time, 8));
  return absl::OkStatus();
}

absl::Status AnimationBezierInt16::ReadAndValidate(ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(rb.ReadSigned16(start_point_value));
  RETURN_IF_NOT_OK(rb.ReadSigned16(end_point_value));
  RETURN_IF_NOT_OK(rb.ReadSigned16(control_point_value));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, control_point_relative_time));

  return absl::OkStatus();
}

absl::Status MixGainParameterData::ReadAndValidate(ReadBitBuffer& rb) {
  DecodedUleb128 animation_type_uleb;
  RETURN_IF_NOT_OK(rb.ReadULeb128(animation_type_uleb));
  const auto animation_type =
      static_cast<MixGainParameterData::AnimationType>(animation_type_uleb);
  switch (animation_type) {
    using enum MixGainParameterData::AnimationType;
    case kAnimateStep: {
      AnimationStepInt16 step_param_data;
      RETURN_IF_NOT_OK(step_param_data.ReadAndValidate(rb));
      param_data = step_param_data;
      return absl::OkStatus();
    }
    case kAnimateLinear: {
      AnimationLinearInt16 linear_param_data;
      RETURN_IF_NOT_OK(linear_param_data.ReadAndValidate(rb));
      param_data = linear_param_data;
      return absl::OkStatus();
    }
    case kAnimateBezier: {
      AnimationBezierInt16 bezier_param_data;
      RETURN_IF_NOT_OK(bezier_param_data.ReadAndValidate(rb));
      param_data = bezier_param_data;
      return absl::OkStatus();
    }
    default:
      return absl::UnimplementedError(
          absl::StrCat("Unknown animation type= ", animation_type_uleb));
  }
}

absl::Status MixGainParameterData::Write(WriteBitBuffer& wb) const {
  // Write the `animation_type` field.
  RETURN_IF_NOT_OK(
      wb.WriteUleb128(static_cast<DecodedUleb128>(GetAnimationType())));

  // Write the fields dependent on the `animation_type` field.
  return std::visit([&wb](const auto& arg) { return arg.ValidateAndWrite(wb); },
                    param_data);
}

MixGainParameterData::AnimationType MixGainParameterData::GetAnimationType()
    const {
  return std::visit(
      absl::Overload{
          [](const AnimationStepInt16&) { return kAnimateStep; },
          [](const AnimationLinearInt16&) { return kAnimateLinear; },
          [](const AnimationBezierInt16&) { return kAnimateBezier; }},
      param_data);
}

void MixGainParameterData::Print() const {
  ABSL_LOG(INFO) << "    animation_type= " << absl::StrCat(GetAnimationType());
  std::visit([](const auto& arg) { arg.Print(); }, param_data);
}

}  // namespace iamf_tools
