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

#ifndef CLI_AUDIO_ELEMENT_WITH_DATA_H_
#define CLI_AUDIO_ELEMENT_WITH_DATA_H_

#include <list>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "iamf/cli/channel_label.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/param_definitions/recon_gain_param_definition.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

typedef absl::flat_hash_map<DecodedUleb128, std::list<ChannelLabel::Label>>
    SubstreamIdLabelsMap;
typedef absl::flat_hash_map<ChannelLabel::Label, float> LabelGainMap;

struct AudioElementWithData {
  friend bool operator==(const AudioElementWithData& lhs,
                         const AudioElementWithData& rhs) = default;
  AudioElementObu obu;
  const CodecConfigObu* codec_config;
  SubstreamIdLabelsMap substream_id_to_labels;
  LabelGainMap label_to_output_gain;
  std::vector<ChannelNumbers> channel_numbers_for_layers;
};

}  // namespace iamf_tools

#endif  // CLI_AUDIO_ELEMENT_WITH_DATA_H_
