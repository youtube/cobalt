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

#ifndef CLI_GLOBAL_TIMING_MODULE_H_
#define CLI_GLOBAL_TIMING_MODULE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/obu/param_definitions/param_definition_variant.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

class GlobalTimingModule {
 public:
  /*!\brief Creates a Global Timing Module.
   *
   * \param audio_elements Audio Element OBUs with data to search for sample
   *        rates.
   * \param param_definition_variants Parameter definitions keyed by parameter
   *        IDs.
   * \return `GlobalTimingModule` on success. Null pointer on failure.
   */
  static std::unique_ptr<GlobalTimingModule> Create(
      const DescriptorObus::AudioElementsById& audio_elements,
      const absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>&
          param_definition_variants);

  /*!\brief Gets the start and end timestamps of the next Audio Frame.
   *
   * \param audio_substream_id Substream ID of the Audio Frame.
   * \param duration Duration of this frame measured in ticks.
   * \param start_timestamp Output start timestamp.
   * \param end_timestamp Output end timestamp.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status GetNextAudioFrameTimestamps(DecodedUleb128 audio_substream_id,
                                           uint32_t duration,
                                           InternalTimestamp& start_timestamp,
                                           InternalTimestamp& end_timestamp);

  /*!\brief Gets the start and end timestamps of the next Parameter Block.
   *
   * \param parameter_id ID of the Parameter Block.
   * \param input_start_timestamp Start timestamp specified by the user. Will be
   *        used to check if there are gaps.
   * \param duration Duration of this Parameter Block measured in ticks.
   * \param start_timestamp Output start timestamp.
   * \param end_timestamp Output end timestamp.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status GetNextParameterBlockTimestamps(
      DecodedUleb128 parameter_id, InternalTimestamp input_start_timestamp,
      uint32_t duration, InternalTimestamp& start_timestamp,
      InternalTimestamp& end_timestamp);

  // TODO(b/291732058): Bring back parameter block coverage validation.

  /*!\brief Gets the global timestamp shared by all current Audio Frames.
   *
   * May fill in `std::nullopt` if some Audio Frames have different timestamps
   * (typically because the current temporal unit has not been processed fully).
   *
   * \param global_timestamp Global timestamp if it's shared by the current
   *        Audio Frames; `std::nullopt` if not.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status GetGlobalAudioFrameTimestamp(
      std::optional<InternalTimestamp>& global_timestamp) const;

 private:
  struct TimingData {
    // Ticks per second, used by audio sample rate and parameter rate.
    const uint32_t rate;

    // Measured in ticks implied by `rate`.
    InternalTimestamp timestamp;
  };

  /*!\brief Constructor.
   *
   * Used only by `Create()`.
   *
   * \param audio_frame_timing_data Timing data for Audio Frames keyed by
   *        substream ID.
   * \param parameter_block_timing_data Timing data for Parameter Blocks keyed
   *        by parameter ID.
   */
  GlobalTimingModule(
      absl::flat_hash_map<DecodedUleb128, TimingData>&& audio_frame_timing_data,
      absl::flat_hash_map<DecodedUleb128, TimingData>&&
          parameter_block_timing_data)
      : audio_frame_timing_data_(std::move(audio_frame_timing_data)),
        parameter_block_timing_data_(std::move(parameter_block_timing_data)) {}

  absl::Status GetTimestampsForId(
      DecodedUleb128 id, uint32_t duration,
      absl::flat_hash_map<DecodedUleb128, TimingData>& id_to_timing_data,
      InternalTimestamp& start_timestamp, InternalTimestamp& end_timestamp);

  absl::flat_hash_map<DecodedUleb128, TimingData> audio_frame_timing_data_;
  absl::flat_hash_map<DecodedUleb128, TimingData> parameter_block_timing_data_;
};

}  // namespace iamf_tools

#endif  // CLI_GLOBAL_TIMING_MODULE_H_
