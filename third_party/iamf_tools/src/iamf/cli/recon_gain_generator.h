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

#ifndef CLI_RECON_GAIN_GENERATOR_H_
#define CLI_RECON_GAIN_GENERATOR_H_

#include "absl/status/status.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/labeled_frame.h"

namespace iamf_tools {

class ReconGainGenerator {
 public:
  /*!\brief Computes the recon gain for the input channel.
   *
   * \param label Label of the channel to compute.
   * \param label_to_samples Mapping from channel labels to original samples.
   * \param label_to_decoded_samples Mapping from channel labels to decoded
   *        samples.
   * \param additional_logging Whether to enable additinal logging.
   * \param recon_gain Result in the range [0, 1].
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::Status ComputeReconGain(
      ChannelLabel::Label label, const LabelSamplesMap& label_to_samples,
      const LabelSamplesMap& label_to_decoded_samples, bool additional_logging,
      double& recon_gain);
};

}  // namespace iamf_tools

#endif  // CLI_RECON_GAIN_GENERATOR_H_
