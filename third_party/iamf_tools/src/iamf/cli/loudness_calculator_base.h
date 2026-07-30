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
#ifndef CLI_LOUDNESS_CALCULATOR_BASE_H_
#define CLI_LOUDNESS_CALCULATOR_BASE_H_

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Abstract class for calculate loudness from an input audio stream.
 *
 * - Call the constructor with an input `MixPresentationLayout`.
 * - Call `AccumulateLoudnessForSamples()` to accumlate interleaved audio
 * samples to measure loudness on.
 * - Call `QueryLoudness()` to query the current loudness. The types to be
 * measured are determined from the constructor argument.
 */
class LoudnessCalculatorBase {
 public:
  /*!\brief Destructor. */
  virtual ~LoudnessCalculatorBase() = 0;

  /*!\brief Accumulates samples to be measured.
   *
   * \param channel_time_samples Samples to push arranged in (channel, time).
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status AccumulateLoudnessForSamples(
      absl::Span<const absl::Span<const InternalSampleType>>
          channel_time_samples) = 0;

  /*!\brief Outputs the measured loudness.
   *
   * \return Measured loudness on success. A specific status on failure.
   */
  virtual absl::StatusOr<LoudnessInfo> QueryLoudness() const = 0;

 protected:
  /*!\brief Constructor. */
  LoudnessCalculatorBase() {}
};

}  // namespace iamf_tools

#endif  // CLI_LOUDNESS_CALCULATOR_BASE_H_
