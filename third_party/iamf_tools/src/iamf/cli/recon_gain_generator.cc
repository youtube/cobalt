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
#include "iamf/cli/recon_gain_generator.h"

#include <cmath>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/map_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

// Returns the Root Mean Square (RMS) power of input `samples`.
double ComputeSignalPower(absl::Span<const InternalSampleType> samples) {
  double mean_square = 0.0;
  const double scale = 1.0 / static_cast<double>(samples.size());
  for (const auto s : samples) {
    mean_square += scale * s * s;
  }
  return std::sqrt(mean_square);
}

// Find relevant samples. E.g. Computation of kDemixedLrs7 uses kLs5 and kLss7.
// Spec says "relevant mixed channel of the down-mixed audio for CL #i-1." So
// Level Mk is the signal power or kLs5. kLss7 is from CL #i and does not
// contribute to Level Mk.
absl::Status FindRelevantMixedSamples(
    const bool additional_logging, ChannelLabel::Label label,
    const LabelSamplesMap& label_to_samples,
    absl::Span<const InternalSampleType>& relevant_mixed_samples) {
  using enum ChannelLabel::Label;
  static const absl::NoDestructor<
      absl::flat_hash_map<ChannelLabel::Label, ChannelLabel::Label>>
      kLabelToRelevantMixedLabel({{kDemixedL7, kL5},
                                  {kDemixedR7, kR5},
                                  {kDemixedLrs7, kLs5},
                                  {kDemixedRrs7, kRs5},
                                  {kDemixedLtb4, kLtf2},
                                  {kDemixedRtb4, kRtf2},
                                  {kDemixedL5, kL3},
                                  {kDemixedR5, kR3},
                                  {kDemixedLs5, kL3},
                                  {kDemixedRs5, kR3},
                                  {kDemixedLtf2, kLtf3},
                                  {kDemixedRtf2, kRtf3},
                                  {kDemixedL3, kL2},
                                  {kDemixedR3, kR2},
                                  {kDemixedR2, kMono}});

  ChannelLabel::Label relevant_mixed_label;
  RETURN_IF_NOT_OK(
      CopyFromMap(*kLabelToRelevantMixedLabel, label,
                  "`relevant_mixed_label` for demixed `ChannelLabel::Label`",
                  relevant_mixed_label));

  ABSL_LOG_IF(INFO, additional_logging)
      << "Relevant mixed samples has label: " << relevant_mixed_label;
  return FindSamplesOrDemixedSamples(relevant_mixed_label, label_to_samples,
                                     relevant_mixed_samples);
}

}  // namespace

absl::Status ReconGainGenerator::ComputeReconGain(
    ChannelLabel::Label label, const LabelSamplesMap& label_to_samples,
    const LabelSamplesMap& label_to_decoded_samples,
    const bool additional_logging, double& recon_gain) {
  // Gather information about the original samples.
  absl::Span<const InternalSampleType> original_samples;
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(label, label_to_samples, original_samples));
  ABSL_LOG_IF(INFO, additional_logging)
      << "[" << label
      << "] original_samples.size()= " << original_samples.size();

  // Level Ok in the Spec.
  const double original_power = ComputeSignalPower(original_samples);

  // TODO(b/289064747): Investigate if the recon gain mismatches are resolved
  //                    after we switched to representing data in [-1, +1].
  // If 10*log10(level Ok / maxL^2) is less than the first threshold value
  // (e.g. -80dB), Recon_Gain (k, i) = 0. Where, maxL = 32767 for 16bits.
  // In this codebase we represent the `InternalSampleType` as a `double` in the
  // range of [-1, +1], so we use maxL = 1.0 instead.
  constexpr InternalSampleType kMaxLSquared = 1.0 * 1.0;
  const double original_power_db = 10 * log10(original_power / kMaxLSquared);
  ABSL_LOG_IF(INFO, additional_logging)
      << "Level OK (dB) " << original_power_db;
  if (original_power_db < -80) {
    recon_gain = 0;
    return absl::OkStatus();
  }

  // Gather information about mixed samples.
  absl::Span<const InternalSampleType> relevant_mixed_samples;
  RETURN_IF_NOT_OK(FindRelevantMixedSamples(
      additional_logging, label, label_to_samples, relevant_mixed_samples));
  ABSL_LOG_IF(INFO, additional_logging)
      << "[" << label
      << "] relevant_mixed_samples.size()= " << relevant_mixed_samples.size();

  // Level Mk in the Spec.
  const double relevant_mixed_power =
      ComputeSignalPower(relevant_mixed_samples);
  const double mixed_power_db = 10 * log10(relevant_mixed_power / kMaxLSquared);
  ABSL_LOG_IF(INFO, additional_logging) << "Level MK (dB) " << mixed_power_db;

  // If 10*log10(level Ok / level Mk ) is less than the second threshold
  // value (e.g. -6dB), Recon_Gain (k, i) is set to the value which makes
  // level Ok = Recon_Gain (k, i)^2 x level Dk.
  double original_mixed_ratio_db =
      10 * log10(original_power / relevant_mixed_power);
  ABSL_LOG_IF(INFO, additional_logging)
      << "Level Ok (dB) / Level Mk (dB) " << original_mixed_ratio_db;

  // Otherwise, Recon_Gain (k, i) = 1.
  if (original_mixed_ratio_db >= -6) {
    recon_gain = 1;
    return absl::OkStatus();
  }

  // Gather information about the demixed samples.
  absl::Span<const InternalSampleType> demixed_samples;
  RETURN_IF_NOT_OK(FindSamplesOrDemixedSamples(label, label_to_decoded_samples,
                                               demixed_samples));
  ABSL_LOG_IF(INFO, additional_logging)
      << "[" << label << "] demixed_samples.size()= " << demixed_samples.size();

  // Level Dk in the Spec.
  const double demixed_power = ComputeSignalPower(demixed_samples);

  // Set recon gain to the value implied by the spec.
  double demixed_power_ratio_db = 10 * log10(demixed_power / mixed_power_db);
  ABSL_LOG_IF(INFO, additional_logging)
      << "Level DK (dB) " << demixed_power_ratio_db;
  recon_gain = std::sqrt(original_power / demixed_power);

  return absl::OkStatus();
}

}  // namespace iamf_tools
