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
#include "iamf/obu/substream_channel_count.h"

#include <cstddef>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"

namespace iamf_tools {

SubstreamChannelCount SubstreamChannelCount::MakeSingular() {
  return SubstreamChannelCount(1);
}

SubstreamChannelCount SubstreamChannelCount::MakeCoupled() {
  return SubstreamChannelCount(2);
}

absl::StatusOr<SubstreamChannelCount> SubstreamChannelCount::Create(
    int num_channels) {
  if (num_channels == 1) {
    return SubstreamChannelCount::MakeSingular();
  } else if (num_channels == 2) {
    return SubstreamChannelCount::MakeCoupled();
  } else {
    return absl::InvalidArgumentError(
        absl::StrCat("Num channels must be 1 or 2: ", num_channels));
  }
}

size_t SubstreamChannelCount::num_channels() const { return num_channels_; }

SubstreamChannelCount::SubstreamChannelCount(size_t num_channels)
    : num_channels_(num_channels) {}

}  // namespace iamf_tools
