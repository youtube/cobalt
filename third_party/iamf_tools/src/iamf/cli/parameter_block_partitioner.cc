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
#include "iamf/cli/parameter_block_partitioner.h"

#include <algorithm>
#include <cstdint>
#include <list>
#include <vector>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/proto/ia_sequence_header.pb.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto/parameter_block.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/cli/proto_conversion/proto_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/common/utils/obu_util.h"
#include "iamf/obu/param_definitions/subblock_schedule.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

using iamf_tools_cli_proto::ParameterBlockObuMetadata;

namespace {

absl::Status InterpolateMixGainParameterData(
    const iamf_tools_cli_proto::MixGainParameterData& mix_gain_parameter_data,
    InternalTimestamp start_time, InternalTimestamp end_time,
    InternalTimestamp target_time, int16_t& target_mix_gain) {
  const auto& param_data = mix_gain_parameter_data.param_data();
  float target_mix_gain_db = 0;
  using enum iamf_tools_cli_proto::AnimatedParameterDataInt16::
      ParameterDataCase;
  RETURN_IF_NOT_OK(InterpolateMixGainValue(
      mix_gain_parameter_data.param_data().parameter_data_case(), kStep,
      kLinear, kBezier,
      [&param_data]() {
        return static_cast<int16_t>(param_data.step().start_point_value());
      },
      [&param_data]() {
        return static_cast<int16_t>(param_data.linear().start_point_value());
      },
      [&param_data]() {
        return static_cast<int16_t>(param_data.linear().end_point_value());
      },
      [&param_data]() {
        return static_cast<int16_t>(param_data.bezier().start_point_value());
      },
      [&param_data]() {
        return static_cast<int16_t>(param_data.bezier().end_point_value());
      },
      [&param_data]() {
        return static_cast<int16_t>(param_data.bezier().control_point_value());
      },
      [&param_data]() {
        return static_cast<int16_t>(
            param_data.bezier().control_point_relative_time());
      },
      start_time, end_time, target_time, target_mix_gain_db));

  // Interpolated values are in dB in float; convert back to Q7.8 to store in
  // the partitioned parameter block.
  return FloatToQ7_8(target_mix_gain_db, target_mix_gain);
}

/*!\brief Partitions a `MixGainParameterData`
 *
 * Partitions a `MixGainParameterData` including the nested fields that describe
 * animation.
 *
 * \param subblock_mix_gain Input mix gain to partition.
 * \param subblock_start_time Start time of the subblock being partitioned.
 * \param subblock_end_time End time of the subblock being partitioned.
 * \param partition_start_time Start time of the output partitioned parameter
 *        block.
 * \param partitioned_end_time End time of the output partitioned parameter
 *        block.
 * \param partitioned_subblock Output argument with `MixGainParameterData`
 *        filled in.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
absl::Status PartitionMixGain(
    const iamf_tools_cli_proto::MixGainParameterData& subblock_mix_gain,
    InternalTimestamp subblock_start_time, InternalTimestamp subblock_end_time,
    InternalTimestamp partitioned_start_time,
    InternalTimestamp partitioned_end_time,
    iamf_tools_cli_proto::ParameterSubblock& partitioned_subblock) {
  auto* mix_gain_param_data =
      partitioned_subblock.mutable_mix_gain_parameter_data();

  // Partition the animated parameter.
  switch (subblock_mix_gain.param_data().parameter_data_case()) {
    using enum iamf_tools_cli_proto::AnimatedParameterDataInt16::
        ParameterDataCase;
    case kStep: {
      int16_t start_point_value;
      RETURN_IF_NOT_OK(InterpolateMixGainParameterData(
          subblock_mix_gain, subblock_start_time, subblock_end_time,
          partitioned_start_time, start_point_value));
      mix_gain_param_data->mutable_param_data()
          ->mutable_step()
          ->set_start_point_value(static_cast<int32_t>(start_point_value));
      return absl::OkStatus();
    }
    case kLinear: {
      // Set partitioned start time to the value of the parameter at that time.
      ABSL_LOG_FIRST_N(INFO, 3)
          << subblock_start_time << " " << subblock_end_time << " "
          << partitioned_start_time << " " << partitioned_end_time;
      // Calculate the subblock's parameter value at the start of the partition.
      int16_t start_point_value;
      int16_t end_point_value;
      auto status = InterpolateMixGainParameterData(
          subblock_mix_gain, subblock_start_time, subblock_end_time,
          partitioned_start_time, start_point_value);

      // Calculate the subblock's parameter value at the end of the partition.
      status.Update(InterpolateMixGainParameterData(
          subblock_mix_gain, subblock_start_time, subblock_end_time,
          partitioned_end_time, end_point_value));

      if (!status.ok()) {
        return absl::Status(
            status.code(),
            absl::StrCat("Failed to interpolate mix gain values. "
                         "`InterpolateMixGainParameterData` returned: ",
                         status.message()));
      }
      auto* linear =
          mix_gain_param_data->mutable_param_data()->mutable_linear();
      linear->set_start_point_value(static_cast<int32_t>(start_point_value));
      linear->set_end_point_value(static_cast<int32_t>(end_point_value));
      return absl::OkStatus();
    }
    case kBezier: {
      if (subblock_start_time == partitioned_start_time &&
          subblock_end_time == partitioned_end_time) {
        // Handle the simplest case where the subblock is aligned and does not
        // need partitioning.
        *mix_gain_param_data->mutable_param_data() =
            subblock_mix_gain.param_data();
        return absl::OkStatus();
      }
      // TODO(b/279581032): Carefully split the bezier curve. Be careful with
      //                    Q7.8 format.
      return absl::InvalidArgumentError(absl::StrCat(
          "The encoder does not fully support partitioning bezier ",
          "parameters yet."));
    }
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unrecognized parameter data case = ",
                       subblock_mix_gain.param_data().parameter_data_case()));
  }
}

/*!\brief Gets the subblocks that overlap with the input times.
 *
 * Finds all subblocks in the input Parameter Block that overlap with
 * `partitioned_start_time` and `partitioned_end_time.
 *
 * \param full_parameter_block Input full parameter block.
 * \param partitioned_start_time Start time of the output partitioned parameter
 *        block.
 * \param partitioned_end_time End time of the output partitioned parameter
 *        block.
 * \param partitioned_subblocks Output partitioned subblocks.
 * \param constant_subblock_duration Output argument which corresponds to the
 *        value of `constant_subblock_duration` in the OBU or metadata.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
absl::Status GetPartitionedSubblocks(
    const ParameterBlockObuMetadata& full_parameter_block,
    InternalTimestamp partitioned_start_time,
    InternalTimestamp partitioned_end_time,
    std::list<iamf_tools_cli_proto::ParameterSubblock>& partitioned_subblocks,
    uint32_t& constant_subblock_duration) {
  ABSL_LOG_FIRST_N(INFO, 1) << "   full_parameter_block=\n"
                            << full_parameter_block;

  InternalTimestamp current_time = full_parameter_block.start_timestamp();

  // Track that the split subblocks cover the whole partition.
  InternalTimestamp total_covered_duration = 0;

  // Get the `SubblockSchedule` for this OBU, this implies the OBU is mode 1.
  // The partitioner is only designed to work on mode 1 parameter blocks.
  auto schedule = CreateSubblockScheduleFromMetadata(full_parameter_block);
  if (!schedule.ok()) {
    return schedule.status();
  }

  // Loop through all subblocks in the original Parameter Block.
  const auto num_subblocks = full_parameter_block.subblocks_size();
  for (int i = 0; i < num_subblocks; ++i) {
    // Get the start and end time of this subblock.
    const InternalTimestamp subblock_start_time = current_time;

    const auto subblock_duration = schedule->GetSubblockDuration(i);
    if (!subblock_duration.ok()) {
      return subblock_duration.status();
    }
    const InternalTimestamp subblock_end_time =
        subblock_start_time +
        static_cast<InternalTimestamp>(*subblock_duration);
    current_time = subblock_end_time;

    if (subblock_end_time <= partitioned_start_time ||
        partitioned_end_time <= subblock_start_time) {
      // The subblock ends before the partition or starts after. It can't
      // overlap.
      continue;
    }

    // Found an overlapping subblock. Create a new one for the partition that
    // represents the overlapped time.
    iamf_tools_cli_proto::ParameterSubblock partitioned_subblock;
    const InternalTimestamp partitioned_subblock_start =
        std::max(partitioned_start_time, subblock_start_time);
    const InternalTimestamp partitioned_subblock_end =
        std::min(partitioned_end_time, subblock_end_time);
    const auto partitioned_subblock_duration = static_cast<uint32_t>(
        partitioned_subblock_end - partitioned_subblock_start);
    total_covered_duration += partitioned_subblock_duration;
    partitioned_subblock.set_subblock_duration(partitioned_subblock_duration);

    const auto& subblock_i = full_parameter_block.subblocks(i);
    if (subblock_i.has_mix_gain_parameter_data()) {
      // Mix Gain animated parameters need to be partitioned.
      RETURN_IF_NOT_OK(PartitionMixGain(
          subblock_i.mix_gain_parameter_data(), subblock_start_time,
          subblock_end_time, partitioned_subblock_start,
          partitioned_subblock_end, partitioned_subblock));
    } else if (subblock_i.has_demixing_info_parameter_data()) {
      if (!partitioned_subblocks.empty()) {
        return absl::InvalidArgumentError(
            "There should only be one subblock for demixing info.");
      }
      *partitioned_subblock.mutable_demixing_info_parameter_data() =
          subblock_i.demixing_info_parameter_data();
    } else if (subblock_i.has_recon_gain_info_parameter_data()) {
      if (!partitioned_subblocks.empty()) {
        return absl::InvalidArgumentError(
            "There should only be one subblock for recon gain info.");
      }
      *partitioned_subblock.mutable_recon_gain_info_parameter_data() =
          subblock_i.recon_gain_info_parameter_data();
    } else {
      return absl::InvalidArgumentError("Unknown subblock type.");
    }
    partitioned_subblocks.push_back(partitioned_subblock);

    if (subblock_end_time >= partitioned_end_time) {
      // Subblock overlap is over. No more to find for this partition.
      break;
    }
  }

  const auto status = CompareTimestamps(
      partitioned_end_time - partitioned_start_time, total_covered_duration);
  if (!status.ok()) {
    return absl::Status(
        status.code(),
        absl::StrCat(
            "Unable to find enough subblocks to totally cover the duration "
            "of the partitioned Parameter Block OBU. Possible gap in the "
            "sequence. `CompareTimestamps` returned: ",
            status.message()));
  }

  // Get the value of `constant_subblock_duration`.
  std::vector<uint32_t> subblock_durations;
  subblock_durations.reserve(partitioned_subblocks.size());
  for (const auto& subblock : partitioned_subblocks) {
    subblock_durations.push_back(subblock.subblock_duration());
  }

  constant_subblock_duration =
      ParameterBlockPartitioner::FindConstantSubblockDuration(
          subblock_durations);

  return absl::OkStatus();
}

}  // namespace

uint32_t ParameterBlockPartitioner::FindConstantSubblockDuration(
    const std::vector<uint32_t>& subblock_durations) {
  if (subblock_durations.empty()) {
    return 0;
  }
  const auto constant_subblock_duration = subblock_durations[0];

  for (int i = 0; i < subblock_durations.size(); ++i) {
    if (i == subblock_durations.size() - 1 &&
        subblock_durations[i] <= constant_subblock_duration) {
      // The duration of the final subblock can be implicitly calculated. As
      // long as it has equal or smaller duration than
      // `constant_subblock_duration`.
      continue;
    }

    // Other than the special case all subblocks must have the same duration.
    if (subblock_durations[i] != constant_subblock_duration) {
      return 0;
    }
  }

  return constant_subblock_duration;
}

absl::Status ParameterBlockPartitioner::FindPartitionDuration(
    const iamf_tools_cli_proto::ProfileVersion primary_profile,
    const iamf_tools_cli_proto::CodecConfigObuMetadata&
        codec_config_obu_metadata,
    uint32_t& partition_duration) {
  using enum iamf_tools_cli_proto::ProfileVersion;
  if (primary_profile != PROFILE_VERSION_SIMPLE &&
      primary_profile != PROFILE_VERSION_BASE &&
      primary_profile != PROFILE_VERSION_BASE_ENHANCED) {
    // This function only implements limitations described in IAMF v1.1.0 for
    // simple, base, base-enhanced profile.
    return absl::InvalidArgumentError(
        absl::StrCat("FindPartitionDuration() only works with Simple, Base, or "
                     "Base-Enhanced profile"));
  }

  // TODO(b/283281856): Set the duration to a different value when
  //                    `parameter_rate != sample rate`.
  partition_duration =
      codec_config_obu_metadata.codec_config().num_samples_per_frame();
  return absl::OkStatus();
}

absl::Status ParameterBlockPartitioner::PartitionParameterBlock(
    const ParameterBlockObuMetadata& full_parameter_block,
    InternalTimestamp partitioned_start_time,
    InternalTimestamp partitioned_end_time,
    ParameterBlockObuMetadata& partitioned) {
  if (partitioned_start_time >= partitioned_end_time) {
    return absl::InvalidArgumentError(
        absl::StrCat("Cannot partition a parameter block with < 1 duration. "
                     "(partitioned_start_time= ",
                     partitioned_start_time,
                     " partitioned_end_time=", partitioned_end_time));
  }

  // Find the subblocks that overlap this partition.
  std::list<iamf_tools_cli_proto::ParameterSubblock> partitioned_subblocks;

  uint32_t constant_subblock_duration;
  RETURN_IF_NOT_OK(GetPartitionedSubblocks(
      full_parameter_block, partitioned_start_time, partitioned_end_time,
      partitioned_subblocks, constant_subblock_duration));

  // Create the partitioned parameter block OBU metadata. The first few fields
  // are always the same as the full metadata.
  partitioned.set_parameter_id(full_parameter_block.parameter_id());
  partitioned.set_duration(
      static_cast<uint32_t>(partitioned_end_time - partitioned_start_time));
  partitioned.set_constant_subblock_duration(constant_subblock_duration);
  *partitioned.mutable_obu_header() = full_parameter_block.obu_header();
  for (const auto& subblock : partitioned_subblocks) {
    *partitioned.add_subblocks() = subblock;
  }
  partitioned.set_start_timestamp(partitioned_start_time);

  return absl::OkStatus();
}

absl::Status ParameterBlockPartitioner::PartitionFrameAligned(
    uint32_t partition_duration,
    const ParameterBlockObuMetadata& full_parameter_block,
    std::list<ParameterBlockObuMetadata>& partitioned_parameter_blocks) {
  // Partition this parameter block into several blocks with the same duration.
  const InternalTimestamp end_timestamp =
      full_parameter_block.start_timestamp() + full_parameter_block.duration();
  for (InternalTimestamp t = full_parameter_block.start_timestamp();
       t < end_timestamp; t += partition_duration) {
    ABSL_LOG_EVERY_N_SEC(INFO, 10)
        << "Partitioning parameter blocks at timestamp= " << t;
    ParameterBlockObuMetadata partitioned_parameter_block;
    RETURN_IF_NOT_OK(PartitionParameterBlock(full_parameter_block, t,
                                             t + partition_duration,
                                             partitioned_parameter_block));
    partitioned_parameter_blocks.push_back(partitioned_parameter_block);
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools
