/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef CLI_OBU_SEQUENCER_STREAMING_IAMF_H_
#define CLI_OBU_SEQUENCER_STREAMING_IAMF_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/cli/obu_sequencer_base.h"
#include "iamf/common/leb_generator.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief OBU sequencer for streaming standalone IAMF.
 *
 * This class is useful to directly process serialized IAMF in memory. For
 * example, OBUs may be pushed in one at a time and then collected and sent out
 * over the network by the user of this class.
 *
 * The class usage is similar to the `ObuSequencerBase` class, but the user of
 * this class should retrieve the serialized OBUs using
 * `GetSerializedDescriptorObus` and `GetPreviousSerializedTemporalUnit` and do
 * something with them.
 */
class ObuSequencerStreamingIamf : public ObuSequencerBase {
 public:
  /*!\brief Constructor.
   *
   * \param include_temporal_delimiters Whether the serialized data should
   *        include a temporal delimiter.
   * \param leb_generator Leb generator to use when writing OBUs.
   */
  ObuSequencerStreamingIamf(bool include_temporal_delimiters,
                            const LebGenerator& leb_generator);

  ~ObuSequencerStreamingIamf() override = default;

  /*!\brief Returns the serialized descriptor OBUs.
   *
   * \return Serialized descriptor OBUs, or an empty span if descriptor OBUs
   *         are not available
   */
  absl::Span<const uint8_t> GetSerializedDescriptorObus() const;

  /*!\brief Returns the previous serialized temporal unit OBUs.
   *
   * \return Serialized OBUS from the previous temporal unit, or an empty span
   *         if a temporal unit is not available.
   */
  absl::Span<const uint8_t> GetPreviousSerializedTemporalUnit() const;

 private:
  /*!\brief Pushes the descriptor OBUs to some output.
   *
   * \param common_samples_per_frame Ignored.
   * \param common_sample_rate Ignored.
   * \param common_bit_depth Ignored.
   * \param first_untrimmed_timestamp Ignored.
   * \param num_channels Ignored..
   * \param descriptor_obus Serialized descriptor OBUs to write.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status PushSerializedDescriptorObus(
      uint32_t /*common_samples_per_frame*/, uint32_t /*common_sample_rate*/,
      uint8_t /*common_bit_depth*/,
      std::optional<InternalTimestamp> /*first_untrimmed_timestamp*/,
      int /*num_channels*/, absl::Span<const uint8_t> descriptor_obus) override;

  /*!\brief Pushes a single temporal unit to some output.
   *
   * \param timestamp Ignored.
   * \param num_samples Ignored.
   * \param is_key_frame Ignored.
   * \param temporal_unit Temporal unit to push.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status PushSerializedTemporalUnit(
      InternalTimestamp /*timestamp*/, int /*num_samples*/,
      bool /*is_key_frame*/, absl::Span<const uint8_t> temporal_unit) override;

  /*!\brief Pushes the finalized descriptor OBUs to the IAMF file.
   *
   * \param descriptor_obus Serialized finalized descriptor OBUs to push.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status PushFinalizedDescriptorObus(
      absl::Span<const uint8_t> descriptor_obus) override;

  /*!\brief Signals that no more data is coming. */
  void CloseDerived() override;

  /*!\brief Aborts writing the output.
   *
   * Cleans up the output file if it exists.
   */
  void AbortDerived() override;

  std::vector<uint8_t> serialized_descriptor_obus_;
  std::vector<uint8_t> previous_serialized_temporal_unit_;
};

}  // namespace iamf_tools

#endif  // CLI_OBU_SEQUENCER_STREAMING_IAMF_H_
