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
#ifndef CLI_PARAMETER_BLOCK_PARTITIONER_H_
#define CLI_PARAMETER_BLOCK_PARTITIONER_H_

#include <cstdint>
#include <list>
#include <vector>

#include "absl/status/status.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/proto/ia_sequence_header.pb.h"
#include "iamf/cli/proto/parameter_block.pb.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Static functions to partition `ParameterBlockObuMetadata`.
 *
 * Useful when the `ParameterBlockObuMetadata` are specified as "virtual"
 * parameter blocks which do not necessarily have durations and alignment that
 * comply with the IAMF specification.
 *
 * This class provides functions to assist splitting
 * `ParameterBlockObuMetadata`s which can be used to ensure they obey IAMF
 * requirements (e.g. frame alignment, number of subblocks, etc.).
 */
class ParameterBlockPartitioner {
 public:
  /*!\brief Finds the `constant_subblock_duration` for the input durations.
   *
   * \param subblock_durations Vector of subblock durations.
   * \return `constant_subblock_duration` which results in the best bit-rate.
   */
  static uint32_t FindConstantSubblockDuration(
      const std::vector<uint32_t>& subblock_durations);

  /*!\brief Finds the desired duration of partitioned parameter blocks.
   *
   * \param primary_profile Input primary profile version.
   * \param codec_config_obu_metadata Input codec config OBU metadata.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::Status FindPartitionDuration(
      iamf_tools_cli_proto::ProfileVersion primary_profile,
      const iamf_tools_cli_proto::CodecConfigObuMetadata&
          codec_config_obu_metadata,
      uint32_t& partition_duration);

  /*!\brief Partitions the input parameter block into a smaller one.
   *
   * \param full_parameter_block Input full parameter block OBU metadata.
   * \param partitioned_start_time Start time of the output partitioned
   *        parameter block.
   * \param partitioned_end_time End time of the output partitioned parameter
   *        block.
   * \param partitioned_parameter_block Output partitioned parameter block OBU
   *        metadata.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::Status PartitionParameterBlock(
      const iamf_tools_cli_proto::ParameterBlockObuMetadata&
          full_parameter_block,
      InternalTimestamp partitioned_start_time,
      InternalTimestamp partitioned_end_time,
      iamf_tools_cli_proto::ParameterBlockObuMetadata&
          partitioned_parameter_block);

  /*!\brief Partitions the input parameter block into frame-aligned ones.
   *
   * \param partition_duration Duration of each partitioned parameter block.
   * \param full_parameter_block Input full parameter block.
   * \param partitioned_parameter_blocks Output list to append partitioned
   *        parameter blocks to.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::Status PartitionFrameAligned(
      uint32_t partition_duration,
      const iamf_tools_cli_proto::ParameterBlockObuMetadata&
          full_parameter_block,
      std::list<iamf_tools_cli_proto::ParameterBlockObuMetadata>&
          partitioned_parameter_blocks);
};

}  // namespace iamf_tools
#endif  // CLI_PARAMETER_BLOCK_PARTITIONER_H_
