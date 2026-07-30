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

#ifndef CLI_AUDIO_FRAME_WITH_DATA_H_
#define CLI_AUDIO_FRAME_WITH_DATA_H_

#include <optional>
#include <vector>

#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

struct AudioFrameWithData {
  friend bool operator==(const AudioFrameWithData& lhs,
                         const AudioFrameWithData& rhs) = default;
  AudioFrameObu obu;
  InternalTimestamp start_timestamp;  // Start time of this frame. Measured in
                                      // ticks from the Global Timing Module.
  InternalTimestamp end_timestamp;  // End time of this frame. Measured in ticks
                                    // from the Global Timing Module.

  // The samples (in internal sample type) used to encode this audio frame,
  // if known. This is useful to calculate recon gain.
  std::optional<std::vector<std::vector<InternalSampleType>>> encoded_samples;
  // Decoded samples. Includes any samples that will be trimmed in processing.
  // Points to the memory location where samples were first produced.
  // TODO(b/4107595837): Find a more robust data model so that the span is
  //                     guaranteed to point to correct samples.
  absl::Span<const std::vector<InternalSampleType>> decoded_samples;

  // Down-mixing parameters used to create this audio frame.
  DownMixingParams down_mixing_params;

  // Recon gain info parameter data used to adjust the gain of this audio frame.
  ReconGainInfoParameterData recon_gain_info_parameter_data;

  // The audio element with data associated with this frame.
  const AudioElementWithData* audio_element_with_data;
};

}  // namespace iamf_tools

#endif  // CLI_AUDIO_FRAME_WITH_DATA_H_
