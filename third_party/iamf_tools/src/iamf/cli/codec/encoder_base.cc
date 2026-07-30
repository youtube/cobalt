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
#include "iamf/cli/codec/encoder_base.h"

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"

namespace iamf_tools {

EncoderBase::~EncoderBase() {}

absl::Status EncoderBase::Initialize(bool validate_codec_delay) {
  RETURN_IF_NOT_OK(InitializeEncoder());

  // Some encoders depend on `InitializeEncoder` being called before
  // `SetNumberOfSamplesToDelayAtStart`.
  RETURN_IF_NOT_OK(SetNumberOfSamplesToDelayAtStart(validate_codec_delay));
  return absl::OkStatus();
}

absl::Status EncoderBase::ValidateInputSamples(
    const std::vector<std::vector<int32_t>>& samples) const {
  RETURN_IF_NOT_OK(
      ValidateEqual(samples.size(), channel_count_.num_channels(),
                    "number of channels in input samples vs channel_count_"));
  if (samples.empty()) {
    return absl::InvalidArgumentError("samples cannot be empty.");
  }

  if (samples[0].size() != num_samples_per_frame_) {
    auto error_message = absl::StrCat("Found ", samples[0].size(),
                                      " samples per channels. Expected ",
                                      num_samples_per_frame_, ".");
    return absl::InvalidArgumentError(error_message);
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools
