/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#ifndef CLI_LABELED_FRAME_H_
#define CLI_LABELED_FRAME_H_

#include <cstdint>
#include <vector>

#include "absl/container/node_hash_map.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/cli/channel_label.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

typedef absl::node_hash_map<ChannelLabel::Label, std::vector<double>>
    LabelSamplesMap;

struct LabeledFrame {
  uint32_t samples_to_trim_at_end;
  uint32_t samples_to_trim_at_start;
  LabelSamplesMap label_to_samples;
  DownMixingParams demixing_params;
  ReconGainInfoParameterData recon_gain_info_parameter_data;
  // Vector of length `num_layers`. Only populated for scalable channel audio.
  std::vector<ChannelAudioLayerConfig::LoudspeakerLayout>
      loudspeaker_layout_per_layer;
};

/*!\brief Searches the input map for the target samples or demixed samples.
 *
 * \param label Label of the channel (or its demixed version) to search for.
 * \param label_to_samples Map to search in.
 * \return Span to the samples if found, or a specific status on failure.
 */
absl::Status FindSamplesOrDemixedSamples(
    ChannelLabel::Label label, const LabelSamplesMap& label_to_samples,
    absl::Span<const InternalSampleType>& samples);

}  // namespace iamf_tools

#endif  // CLI_LABELED_FRAME_H_
