/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/obu/parameter_block.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/log/absl_log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/obu_util.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/mix_gain_parameter_data.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/param_definitions/subblock_schedule.h"
#include "iamf/obu/parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

absl::StatusOr<DecodedUleb128> ParameterBlockObu::PeekParameterId(
    ReadBitBuffer& rb) {
  auto initial_location = rb.Tell();
  DecodedUleb128 parameter_id;
  auto status = rb.ReadULeb128(parameter_id);
  RETURN_IF_NOT_OK(rb.Seek(initial_location));
  if (!status.ok()) {
    return status;
  }
  return parameter_id;
}

std::unique_ptr<ParameterBlockObu> ParameterBlockObu::CreateMode0(
    const ObuHeader& header, const ParamDefinition& param_definition) {
  if (param_definition.GetParamDefinitionMode() !=
      ParamDefinition::kModeScheduleInParamDefinition) {
    ABSL_LOG(WARNING) << "CreateMode0() should only be called when "
                         "param_definition_mode == 0.";
    return nullptr;
  }

  return absl::WrapUnique(new ParameterBlockObu(header, param_definition,
                                                /*schedule=*/std::nullopt));
}

std::unique_ptr<ParameterBlockObu> ParameterBlockObu::CreateMode1(
    const ObuHeader& header, const ParamDefinition& param_definition,
    const SubblockSchedule& schedule) {
  if (param_definition.GetParamDefinitionMode() !=
      ParamDefinition::kModeScheduleInParameterBlock) {
    ABSL_LOG(WARNING) << "CreateMode1() should only be called when "
                         "param_definition_mode == 1.";
    return nullptr;
  }
  return absl::WrapUnique(
      new ParameterBlockObu(header, param_definition, schedule));
}

absl::StatusOr<std::unique_ptr<ParameterBlockObu>>
ParameterBlockObu::CreateFromBuffer(const ObuHeader& header,
                                    int64_t payload_size,
                                    const ParamDefinition& param_definition,
                                    ReadBitBuffer& rb) {
  // TODO(b/338474387): Test reading in extension parameters.
  auto parameter_block_obu = absl::WrapUnique(new ParameterBlockObu(
      header, param_definition, /*schedule=*/std::nullopt));
  RETURN_IF_NOT_OK(
      parameter_block_obu->ReadAndValidatePayload(payload_size, rb));
  return parameter_block_obu;
}

ParameterBlockObu::ParameterBlockObu(
    const ObuHeader& header, const ParamDefinition& param_definition,
    const std::optional<SubblockSchedule>& schedule)
    : ObuBase(header, kObuIaParameterBlock),
      parameter_id_(param_definition.GetParameterId()),
      schedule_(schedule),
      param_definition_(param_definition) {
  subblocks_.resize(static_cast<size_t>(GetNumSubblocks()));
}

absl::Status ParameterBlockObu::InterpolateMixGainParameterData(
    const MixGainParameterData* mix_gain_parameter_data,
    InternalTimestamp start_time, InternalTimestamp end_time,
    InternalTimestamp target_time, float& target_mix_gain_db) {
  return InterpolateMixGainValue(
      mix_gain_parameter_data->GetAnimationType(),
      MixGainParameterData::kAnimateStep, MixGainParameterData::kAnimateLinear,
      MixGainParameterData::kAnimateBezier,
      [&mix_gain_parameter_data]() {
        return std::get<AnimationStepInt16>(mix_gain_parameter_data->param_data)
            .start_point_value;
      },
      [&mix_gain_parameter_data]() {
        return std::get<AnimationLinearInt16>(
                   mix_gain_parameter_data->param_data)
            .start_point_value;
      },
      [&mix_gain_parameter_data]() {
        return std::get<AnimationLinearInt16>(
                   mix_gain_parameter_data->param_data)
            .end_point_value;
      },
      [&mix_gain_parameter_data]() {
        return std::get<AnimationBezierInt16>(
                   mix_gain_parameter_data->param_data)
            .start_point_value;
      },
      [&mix_gain_parameter_data]() {
        return std::get<AnimationBezierInt16>(
                   mix_gain_parameter_data->param_data)
            .end_point_value;
      },
      [&mix_gain_parameter_data]() {
        return std::get<AnimationBezierInt16>(
                   mix_gain_parameter_data->param_data)
            .control_point_value;
      },
      [&mix_gain_parameter_data]() {
        return std::get<AnimationBezierInt16>(
                   mix_gain_parameter_data->param_data)
            .control_point_relative_time;
      },
      start_time, end_time, target_time, target_mix_gain_db);
}

DecodedUleb128 ParameterBlockObu::GetDuration() const {
  return schedule_.has_value() ? schedule_->GetDuration()
                               : param_definition_.GetDuration();
}

DecodedUleb128 ParameterBlockObu::GetConstantSubblockDuration() const {
  return schedule_.has_value()
             ? schedule_->GetConstantSubblockDuration()
             : param_definition_.GetConstantSubblockDuration();
}

DecodedUleb128 ParameterBlockObu::GetNumSubblocks() const {
  return schedule_.has_value() ? schedule_->GetNumSubblocks()
                               : param_definition_.GetNumSubblocks();
}

absl::StatusOr<DecodedUleb128> ParameterBlockObu::GetSubblockDuration(
    int subblock_index) const {
  return schedule_.has_value()
             ? schedule_->GetSubblockDuration(subblock_index)
             : param_definition_.GetSchedule()->GetSubblockDuration(
                   subblock_index);
}

absl::Status ParameterBlockObu::GetLinearMixGain(
    InternalTimestamp obu_relative_time, float& linear_mix_gain) const {
  if (param_definition_.GetType() !=
      ParamDefinition::kParameterDefinitionMixGain) {
    return absl::InvalidArgumentError("Expected Mix Gain Parameter Definition");
  }

  const DecodedUleb128 num_subblocks = GetNumSubblocks();
  int target_subblock_index = -1;
  InternalTimestamp subblock_relative_start_time = 0;
  InternalTimestamp subblock_relative_end_time = 0;
  for (DecodedUleb128 i = 0; i < num_subblocks; i++) {
    const auto subblock_duration = GetSubblockDuration(i);
    if (!subblock_duration.ok()) {
      return subblock_duration.status();
    }

    if (subblock_relative_start_time <= obu_relative_time &&
        obu_relative_time <
            (subblock_relative_start_time + subblock_duration.value())) {
      target_subblock_index = i;
      subblock_relative_end_time =
          subblock_relative_start_time + subblock_duration.value();
      break;
    }
    subblock_relative_start_time += subblock_duration.value();
  }

  if (target_subblock_index == -1) {
    return absl::UnknownError(
        absl::StrCat("Trying to get mix gain for target_subblock_index = -1, "
                     "with num_subblocks = ",
                     num_subblocks));
  }

  float mix_gain_db = 0;
  RETURN_IF_NOT_OK(InterpolateMixGainParameterData(
      static_cast<const MixGainParameterData*>(
          subblocks_[target_subblock_index].get()),
      subblock_relative_start_time, subblock_relative_end_time,
      obu_relative_time, mix_gain_db));

  // Mix gain data is in dB and stored in Q7.8. Convert to the linear value.
  linear_mix_gain = std::pow(10.0f, mix_gain_db / 20.0f);
  return absl::OkStatus();
}

void ParameterBlockObu::PrintObu() const {
  ABSL_LOG(INFO) << "Parameter Block OBU:";
  ABSL_LOG(INFO) << "  // param_definition:";
  param_definition_.Print();

  ABSL_LOG(INFO) << "  parameter_id= " << parameter_id_;
  if (schedule_.has_value()) {
    schedule_->Print();
  }

  const DecodedUleb128 num_subblocks = GetNumSubblocks();
  for (DecodedUleb128 i = 0; i < num_subblocks; i++) {
    ABSL_LOG(INFO) << "  subblocks[" << i << "]";
    subblocks_[i]->Print();
  }
}

absl::Status ParameterBlockObu::ValidateAndWritePayload(
    WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteUleb128(parameter_id_));

  // Validate the associated `param_definition`.
  RETURN_IF_NOT_OK(param_definition_.Validate());

  if (schedule_.has_value()) {
    return schedule_->Write(subblocks_, wb);
  }

  for (const auto& subblock : subblocks_) {
    RETURN_IF_NOT_OK(ValidateNotNull(subblock, "`parameter_data`"));
    RETURN_IF_NOT_OK(subblock->Write(wb));
  }

  return absl::OkStatus();
}

absl::Status ParameterBlockObu::ReadAndValidatePayloadDerived(
    int64_t /*payload_size*/, ReadBitBuffer& rb) {
  // Validate the associated `param_definition`.
  RETURN_IF_NOT_OK(param_definition_.Validate());
  // Make sure the parameter definition ID actually agrees with the parameter
  // definition ID in the bitstream.
  DecodedUleb128 bitstream_parameter_id;
  RETURN_IF_NOT_OK(rb.ReadULeb128(bitstream_parameter_id));
  if (bitstream_parameter_id != parameter_id_) {
    return absl::InvalidArgumentError(
        absl::StrCat("Parameter ID mismatch: ", bitstream_parameter_id, " vs ",
                     parameter_id_));
  }

  if (param_definition_.GetParamDefinitionMode() ==
      ParamDefinition::kModeScheduleInParamDefinition) {
    // Mode 0 in the spec. The schedule is in ParamDefinition. Only parameter
    // data is in the block.
    schedule_ = std::nullopt;

    const DecodedUleb128 num_subblocks = param_definition_.GetNumSubblocks();
    subblocks_.clear();
    subblocks_.reserve(static_cast<size_t>(num_subblocks));
    for (DecodedUleb128 i = 0; i < num_subblocks; ++i) {
      auto param_data = param_definition_.CreateParameterData();
      RETURN_IF_NOT_OK(param_data->ReadAndValidate(rb));
      subblocks_.push_back(std::move(param_data));
    }
    return absl::OkStatus();
  }

  // Mode 1 in the spec. Portions of the schedule are interlaced with the
  // parameter data in the block.
  auto schedule_and_data = SubblockSchedule::CreateFromBufferWithParameterData(
      [this]() { return param_definition_.CreateParameterData(); }, rb);
  if (!schedule_and_data.ok()) {
    return schedule_and_data.status();
  }

  schedule_ = std::move(schedule_and_data->schedule);
  subblocks_ = std::move(schedule_and_data->parameter_data);

  return absl::OkStatus();
}

}  // namespace iamf_tools
