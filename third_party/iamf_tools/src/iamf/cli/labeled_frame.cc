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

#include "iamf/cli/labeled_frame.h"

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/cli/channel_label.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

using absl::MakeConstSpan;

absl::Status FindSamplesOrDemixedSamples(
    ChannelLabel::Label label, const LabelSamplesMap& label_to_samples,
    absl::Span<const InternalSampleType>& samples) {
  if (auto it = label_to_samples.find(label); it != label_to_samples.end()) {
    samples = MakeConstSpan(it->second);
    return absl::OkStatus();
  }

  auto demixed_label = ChannelLabel::GetDemixedLabel(label);
  if (!demixed_label.ok()) {
    return demixed_label.status();
  }
  if (auto it = label_to_samples.find(*demixed_label);
      it != label_to_samples.end()) {
    samples = MakeConstSpan(it->second);
    return absl::OkStatus();
  } else {
    return absl::InvalidArgumentError(
        absl::StrCat("Channel ", label, " or ", *demixed_label, " not found"));
  }
}

}  // namespace iamf_tools
