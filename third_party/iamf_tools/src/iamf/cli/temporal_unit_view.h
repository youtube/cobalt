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
#ifndef CLI_TEMPORAL_UNIT_VIEW_H_
#define CLI_TEMPORAL_UNIT_VIEW_H_

#include <sys/types.h>

#include <cstdint>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief A view of all OBUs in a single temporal unit as defined by the spec.
 *
 * A temporal unit is defined as a set of all audio frames with the same start
 * timestamp and the same duration from all substreams and all parameter blocks
 * with the start timestamp within the duration.
 *
 * This class provides functionality to create a view of the OBUs in a temporal
 * unit. The factory function validates the input data is self consistent and
 * could be valid in some IA Sequence.
 */
// TODO(b/397637224): Be stricter about the expected parameter block and
//                    substream IDs, or validate that in `ObuSequencerBase`.
struct TemporalUnitView {
  /*!\brief Creates a `TemporalUnitView` from the input data.
   *
   * \param parameter_blocks Parameter blocks to include in the view.
   * \param audio_frames Audio frames to include in the view.
   * \param arbitrary_obus Arbitrary OBUs to include in the view.
   * \return `TemporalUnitView` on success. A specific status on failure.
   */
  static absl::StatusOr<TemporalUnitView> CreateFromPointers(
      absl::Span<const ParameterBlockWithData* const> parameter_blocks,
      absl::Span<const AudioFrameWithData* const> audio_frames,
      absl::Span<const ArbitraryObu* const> arbitrary_obus);

  /*!\brief Creates a `TemporalUnitView` from the input data.
   *
   * Adapter to the pointer-based `Create` function above, but usable with any
   * container of input data.
   *
   * \param parameter_blocks Parameter blocks to include in the view.
   * \param audio_frames Audio frames to include in the view.
   * \param arbitrary_obus Arbitrary OBUs to include in the view.
   * \return `TemporalUnitView` on success. A specific status on failure.
   */
  template <typename ParameterBlockWithDataContainer,
            typename AudioFrameWithDataContainer,
            typename ArbitraryObuContainer>
  static absl::StatusOr<TemporalUnitView> Create(
      const ParameterBlockWithDataContainer& parameter_blocks,
      const AudioFrameWithDataContainer& audio_frames,
      const ArbitraryObuContainer& arbitrary_obus) {
    std::vector<const ParameterBlockWithData*> parameter_blocks_ptrs;
    parameter_blocks_ptrs.reserve(parameter_blocks.size());
    for (const auto& parameter_block : parameter_blocks) {
      parameter_blocks_ptrs.push_back(&parameter_block);
    }

    std::vector<const AudioFrameWithData*> audio_frames_ptrs;
    audio_frames_ptrs.reserve(audio_frames.size());
    for (const auto& audio_frame : audio_frames) {
      audio_frames_ptrs.push_back(&audio_frame);
    }
    std::vector<const ArbitraryObu*> arbitrary_obus_ptrs;
    arbitrary_obus_ptrs.reserve(arbitrary_obus.size());
    for (const auto& arbitrary_obu : arbitrary_obus) {
      arbitrary_obus_ptrs.push_back(&arbitrary_obu);
    }
    return CreateFromPointers(absl::MakeConstSpan(parameter_blocks_ptrs),
                              absl::MakeConstSpan(audio_frames_ptrs),
                              absl::MakeConstSpan(arbitrary_obus_ptrs));
  }

  const std::vector<const ParameterBlockWithData*> parameter_blocks_;
  const std::vector<const AudioFrameWithData*> audio_frames_;
  const std::vector<const ArbitraryObu*> arbitrary_obus_;

  // Common statistics for this temporal unit.
  const InternalTimestamp start_timestamp_;
  const InternalTimestamp end_timestamp_;
  const uint32_t num_samples_to_trim_at_start_;
  const uint32_t num_untrimmed_samples_;

 private:
  /*!\brief Private constructor.
   *
   * Used only by the `Create` factory functions.
   *
   * \param parameter_blocks Parameter blocks to include in the view.
   * \param audio_frames Audio frames to include in the view.
   * \param arbitrary_obus Arbitrary OBUs to include in the view.
   * \param start_timestamp Start timestamp of the temporal unit.
   * \param end_timestamp End timestamp of the temporal unit.
   * \param num_samples_to_trim_at_start Number of samples to trim at the start
   *        of the audio frames.
   * \param num_untrimmed_samples Number of samples in the audio frames before
   *        trimming.
   */
  TemporalUnitView(
      std::vector<const ParameterBlockWithData*>&& parameter_blocks,
      std::vector<const AudioFrameWithData*>&& audio_frames,
      std::vector<const ArbitraryObu*>&& arbitrary_obus,
      InternalTimestamp start_timestamp, InternalTimestamp end_timestamp,
      uint32_t num_samples_to_trim_at_start, uint32_t num_untrimmed_samples);
};

}  // namespace iamf_tools

#endif  // CLI_TEMPORAL_UNIT_VIEW_H_
