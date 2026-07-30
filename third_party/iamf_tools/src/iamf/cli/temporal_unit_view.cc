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

#include "iamf/cli/temporal_unit_view.h"

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

// Common statistics about a temporal unit.
struct TemporalUnitStatistics {
  uint32_t num_samples_per_frame;
  uint32_t num_samples_to_trim_at_end;
  uint32_t num_samples_to_trim_at_start;
  uint32_t num_untrimmed_samples;
  InternalTimestamp start_timestamp;
  InternalTimestamp end_timestamp;
  bool should_contain_an_invalid_arbitrary_obu;
};

absl::StatusOr<TemporalUnitStatistics>
ComputeTemporalUnitStatisticsFromAudioFrame(
    const AudioFrameWithData* first_audio_frame) {
  RETURN_IF_NOT_OK(ValidateNotNull(first_audio_frame, "`audio_frames`"));
  RETURN_IF_NOT_OK(ValidateNotNull(first_audio_frame->audio_element_with_data,
                                   "`audio_frame.audio_element_with_data`"));
  RETURN_IF_NOT_OK(
      ValidateNotNull(first_audio_frame->audio_element_with_data->codec_config,
                      "`audio_frame.audio_element_with_data.codec_config`"));
  TemporalUnitStatistics statistics{
      .num_samples_per_frame = first_audio_frame->audio_element_with_data
                                   ->codec_config->GetNumSamplesPerFrame(),
      .num_samples_to_trim_at_end =
          first_audio_frame->obu.header_.num_samples_to_trim_at_end,
      .num_samples_to_trim_at_start =
          first_audio_frame->obu.header_.num_samples_to_trim_at_start,
      .start_timestamp = first_audio_frame->start_timestamp,
      .end_timestamp = first_audio_frame->end_timestamp,
      .should_contain_an_invalid_arbitrary_obu = false,
  };

  // Check the trim in the first frame is plausible. I.e. there are not at least
  // 0 samples. This also prevents underflow when subtracting later.
  const uint32_t cumulative_trim = statistics.num_samples_to_trim_at_start +
                                   statistics.num_samples_to_trim_at_end;
  RETURN_IF_NOT_OK(Validate(cumulative_trim, std::less_equal<uint32_t>(),
                            statistics.num_samples_per_frame,
                            "cumulative trim is <= `num_samples_per_frame`"));
  statistics.num_untrimmed_samples =
      statistics.num_samples_per_frame - cumulative_trim;

  return statistics;
}

// Construct an arbitrary temporal unit based on arbitrary OBUs, usually
// temporal units must have an audio frame, so we expect one of the arbitrary
// OBUs to be marked as invalidating the bitstream
absl::StatusOr<TemporalUnitStatistics>
ComputeTemporalUnitStatisticsFromArbitraryObu(
    const ArbitraryObu* first_arbitrary_obu) {
  RETURN_IF_NOT_OK(ValidateNotNull(first_arbitrary_obu, "`arbitrary_obus`"));
  RETURN_IF_NOT_OK(ValidateHasValue(first_arbitrary_obu->insertion_tick_,
                                    "`arbitrary_obu->insertion_tick_`"));
  return TemporalUnitStatistics{
      .num_samples_per_frame = 0,
      .num_samples_to_trim_at_end = 0,
      .num_samples_to_trim_at_start = 0,
      .num_untrimmed_samples = 0,
      .start_timestamp = *first_arbitrary_obu->insertion_tick_,
      .end_timestamp = *first_arbitrary_obu->insertion_tick_,
      .should_contain_an_invalid_arbitrary_obu = true,
  };
}

absl::Status ValidateAllParameterBlocksMatchStatistics(
    absl::Span<const ParameterBlockWithData* const> parameter_blocks,
    const TemporalUnitStatistics& statistics) {
  absl::flat_hash_set<uint32_t> seen_parameter_ids;
  for (const auto* parameter_block : parameter_blocks) {
    RETURN_IF_NOT_OK(ValidateNotNull(parameter_block, "`parameter_block`"));
    const auto& [unused_iter, inserted] =
        seen_parameter_ids.insert(parameter_block->obu->parameter_id_);
    if (!inserted) {
      return absl::InvalidArgumentError(
          "A temporal unit must not have multiple parameter blocks with the "
          "same parameter ID.");
    }

    RETURN_IF_NOT_OK(ValidateEqual(
        parameter_block->start_timestamp, statistics.start_timestamp,
        "`start_timestamp` must be the same for all parameter blocks"));
    RETURN_IF_NOT_OK(ValidateEqual(
        parameter_block->end_timestamp, statistics.end_timestamp,
        "`end_timestamp` must be the same for all parameter blocks"));
  }
  return absl::OkStatus();
}

absl::Status ValidateAllAudioFramesMatchStatistics(
    absl::Span<const AudioFrameWithData* const> audio_frames,
    const TemporalUnitStatistics& statistics) {
  absl::flat_hash_set<uint32_t> seen_substream_ids;
  for (const auto* audio_frame : audio_frames) {
    RETURN_IF_NOT_OK(ValidateNotNull(audio_frame, "`audio_frame`"));
    const auto& [unused_iter, inserted] =
        seen_substream_ids.insert(audio_frame->obu.GetSubstreamId());
    if (!inserted) {
      return absl::InvalidArgumentError(
          "A temporal unit must not have multiple audio with the same "
          "substream ID.");
    }

    RETURN_IF_NOT_OK(ValidateNotNull(audio_frame->audio_element_with_data,
                                     "`audio_frame.audio_element_with_data`"));
    RETURN_IF_NOT_OK(
        ValidateNotNull(audio_frame->audio_element_with_data->codec_config,
                        "`audio_frame.audio_element_with_data.codec_config`"));
    RETURN_IF_NOT_OK(ValidateEqual(
        audio_frame->obu.header_.num_samples_to_trim_at_end,
        statistics.num_samples_to_trim_at_end,
        "`num_samples_to_trim_at_end` must be the same for all audio frames"));
    RETURN_IF_NOT_OK(
        ValidateEqual(audio_frame->obu.header_.num_samples_to_trim_at_start,
                      statistics.num_samples_to_trim_at_start,
                      "`num_samples_to_trim_at_start` must be the same for all "
                      "audio frames"));
    RETURN_IF_NOT_OK(ValidateEqual(
        audio_frame->start_timestamp, statistics.start_timestamp,
        "`start_timestamp` must be the same for all audio frames"));
    RETURN_IF_NOT_OK(
        ValidateEqual(audio_frame->end_timestamp, statistics.end_timestamp,
                      "`end_timestamp` must be the same for all audio frames"));
  }
  return absl::OkStatus();
}

absl::Status ValidateAllArbitraryObusMatchStatistics(
    absl::Span<const ArbitraryObu* const> arbitrary_obus,
    const TemporalUnitStatistics& statistics) {
  bool has_invalid_obu = false;
  for (const auto* arbitrary_obu : arbitrary_obus) {
    RETURN_IF_NOT_OK(ValidateNotNull(arbitrary_obu, "`arbitrary_obu`"));
    RETURN_IF_NOT_OK(
        ValidateHasValue(arbitrary_obu->insertion_tick_, "`arbitrary_obu`"));
    RETURN_IF_NOT_OK(ValidateEqual(
        *arbitrary_obu->insertion_tick_, statistics.start_timestamp,
        "`insertion_tick` must be the same for  all arbitrary OBUs"));
    if (arbitrary_obu->invalidates_bitstream_) {
      has_invalid_obu = true;
    }
  }
  if (statistics.should_contain_an_invalid_arbitrary_obu && !has_invalid_obu) {
    return absl::InvalidArgumentError(
        "Expected this temporal unit to have an invalid arbitrary OBU, but "
        "none were found.");
  }

  return absl::OkStatus();
}

bool CompareParameterId(const ParameterBlockWithData* a,
                        const ParameterBlockWithData* b) {
  // These were sanitized elsewhere in the class.
  ABSL_CHECK_NE(a, nullptr);
  ABSL_CHECK_NE(b, nullptr);
  return a->obu->parameter_id_ < b->obu->parameter_id_;
}

bool CompareAudioElementIdAudioSubstreamId(const AudioFrameWithData* a,
                                           const AudioFrameWithData* b) {
  // These were sanitized elsewhere in the class.
  ABSL_CHECK_NE(a, nullptr);
  ABSL_CHECK_NE(a->audio_element_with_data, nullptr);
  ABSL_CHECK_NE(b, nullptr);
  ABSL_CHECK_NE(b->audio_element_with_data, nullptr);
  ABSL_CHECK_NE(b, nullptr);
  if (a->audio_element_with_data->obu.GetAudioElementId() !=
      b->audio_element_with_data->obu.GetAudioElementId()) {
    return a->audio_element_with_data->obu.GetAudioElementId() <
           b->audio_element_with_data->obu.GetAudioElementId();
  }
  return a->obu.GetSubstreamId() < b->obu.GetSubstreamId();
}

}  // namespace

absl::StatusOr<TemporalUnitView> TemporalUnitView::CreateFromPointers(
    absl::Span<const ParameterBlockWithData* const> parameter_blocks,
    absl::Span<const AudioFrameWithData* const> audio_frames,
    absl::Span<const ArbitraryObu* const> arbitrary_obus) {
  absl::StatusOr<TemporalUnitStatistics> statistics =
      absl::InternalError("All branches should set this, or return.");
  if (!audio_frames.empty()) {
    // Infer some statistics based on the first audio frame.
    statistics =
        ComputeTemporalUnitStatisticsFromAudioFrame(audio_frames.front());
  } else if (!arbitrary_obus.empty()) {
    // Ok, typically temporal units require an audio frame, but we allow
    // one with some arbitrary OBUs. This helps support creation of files in the
    // test suite.
    statistics =
        ComputeTemporalUnitStatisticsFromArbitraryObu(arbitrary_obus.front());
  } else {
    return absl::InvalidArgumentError(
        "Every temporal unit must have an audio frame or an arbitrary OBU.");
  }
  if (!statistics.ok()) {
    return statistics.status();
  }

  // Check that all OBUs agree with the statistics.  All frames must have the
  // same trimming information and timestamps as of IAMF v1.1.0.
  RETURN_IF_NOT_OK(
      ValidateAllAudioFramesMatchStatistics(audio_frames, *statistics));
  RETURN_IF_NOT_OK(
      ValidateAllParameterBlocksMatchStatistics(parameter_blocks, *statistics));
  RETURN_IF_NOT_OK(
      ValidateAllArbitraryObusMatchStatistics(arbitrary_obus, *statistics));

  // Sort the OBUS into a canonical order.
  // TODO(b/332956880): Support a custom ordering of parameter blocks and
  // substreams.
  std::vector<const ParameterBlockWithData*> sorted_parameter_blocks(
      parameter_blocks.begin(), parameter_blocks.end());
  std::vector<const AudioFrameWithData*> sorted_audio_frames(
      audio_frames.begin(), audio_frames.end());
  std::vector<const ArbitraryObu*> copied_arbitrary_obus(arbitrary_obus.begin(),
                                                         arbitrary_obus.end());
  absl::c_sort(sorted_parameter_blocks, CompareParameterId);
  absl::c_sort(sorted_audio_frames, CompareAudioElementIdAudioSubstreamId);
  return TemporalUnitView(
      std::move(sorted_parameter_blocks), std::move(sorted_audio_frames),
      std::move(copied_arbitrary_obus), statistics->start_timestamp,
      statistics->end_timestamp, statistics->num_samples_to_trim_at_start,
      statistics->num_untrimmed_samples);
}

TemporalUnitView::TemporalUnitView(
    std::vector<const ParameterBlockWithData*>&& parameter_blocks,
    std::vector<const AudioFrameWithData*>&& audio_frames,
    std::vector<const ArbitraryObu*>&& arbitrary_obus,
    InternalTimestamp start_timestamp, InternalTimestamp end_timestamp,
    uint32_t num_samples_to_trim_at_start, uint32_t num_untrimmed_samples)
    : parameter_blocks_(std::move(parameter_blocks)),
      audio_frames_(std::move(audio_frames)),
      arbitrary_obus_(std::move(arbitrary_obus)),
      start_timestamp_(start_timestamp),
      end_timestamp_(end_timestamp),
      num_samples_to_trim_at_start_(num_samples_to_trim_at_start),
      num_untrimmed_samples_(num_untrimmed_samples) {}

}  // namespace iamf_tools
