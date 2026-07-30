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
#include "iamf/cli/obu_sequencer_streaming_iamf.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <vector>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/cli/obu_sequencer_base.h"
#include "iamf/common/leb_generator.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

// This sequencer does not care about the delay or timing information.
constexpr bool kDoNotDelayDescriptorsUntilFirstUntrimmedSample = false;

void CopySpanToVector(absl::Span<const uint8_t> span,
                      std::vector<uint8_t>& vector) {
  vector.clear();
  vector.reserve(span.size());
  std::copy(span.begin(), span.end(), std::back_inserter(vector));
}

}  // namespace

ObuSequencerStreamingIamf::ObuSequencerStreamingIamf(
    bool include_temporal_delimiters, const LebGenerator& leb_generator)
    : ObuSequencerBase(leb_generator, include_temporal_delimiters,
                       kDoNotDelayDescriptorsUntilFirstUntrimmedSample) {}

absl::Span<const uint8_t>
ObuSequencerStreamingIamf::GetSerializedDescriptorObus() const {
  return absl::MakeConstSpan(serialized_descriptor_obus_);
}

absl::Span<const uint8_t>
ObuSequencerStreamingIamf::GetPreviousSerializedTemporalUnit() const {
  return absl::MakeConstSpan(previous_serialized_temporal_unit_);
}

absl::Status ObuSequencerStreamingIamf::PushSerializedDescriptorObus(
    uint32_t /*common_samples_per_frame*/, uint32_t /*common_sample_rate*/,
    uint8_t /*common_bit_depth*/,
    std::optional<InternalTimestamp> /*first_untrimmed_timestamp*/,
    int /*num_channels*/, absl::Span<const uint8_t> descriptor_obus) {
  CopySpanToVector(descriptor_obus, serialized_descriptor_obus_);
  return absl::OkStatus();
}

absl::Status ObuSequencerStreamingIamf::PushSerializedTemporalUnit(
    InternalTimestamp /*timestamp*/, int /*num_samples*/, bool /*is_key_frame*/,
    absl::Span<const uint8_t> temporal_unit) {
  CopySpanToVector(temporal_unit, previous_serialized_temporal_unit_);
  return absl::OkStatus();
}

absl::Status ObuSequencerStreamingIamf::PushFinalizedDescriptorObus(
    absl::Span<const uint8_t> descriptor_obus) {
  CopySpanToVector(descriptor_obus, serialized_descriptor_obus_);
  return absl::OkStatus();
}

void ObuSequencerStreamingIamf::CloseDerived() {
  // Leave the descriptor OBUs in place, so the user can retrieve the updated
  // descriptors if available.
  previous_serialized_temporal_unit_.clear();
}

void ObuSequencerStreamingIamf::AbortDerived() {
  ABSL_LOG(INFO) << "Aborting ObuSequencerStreamingIamf.";
  serialized_descriptor_obus_.clear();
  previous_serialized_temporal_unit_.clear();
}

}  // namespace iamf_tools
