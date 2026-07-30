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

#ifndef API_CONVERSION_CHANNEL_REORDERER_H_
#define API_CONVERSION_CHANNEL_REORDERER_H_

#include <functional>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Reorders output audio samples for a given configuration. */
class ChannelReorderer {
 public:
  enum class RearrangementScheme {
    kDefaultNoOp = 0,
    kReorderForAndroid = 1,
  };

  /*!\brief Factory function.
   *
   * \param original_layout The Layout of the audio samples before
   *        rearrangement.
   * \param scheme The rearrangement scheme to apply to all calls of Reorder.
   * \return `ChannelRenderer` with the requested configuration.
   */
  static ChannelReorderer Create(
      LoudspeakersSsConventionLayout::SoundSystem original_layout,
      RearrangementScheme scheme);

  /*!\brief Re-orders the audio in-place.
   *
   * \param audio_frame Samples arranged in (channel, time) axes to reorder in
   *        place.
   */
  absl::Status Reorder(
      std::vector<absl::Span<const InternalSampleType>>& audio_frame);

 private:
  explicit ChannelReorderer(
      std::function<
          absl::Status(std::vector<absl::Span<const InternalSampleType>>&)>
          reorder_function);

  std::function<absl::Status(
      std::vector<absl::Span<const InternalSampleType>>&)>
      reorder_function_;
};

}  // namespace iamf_tools

#endif  // API_CONVERSION_CHANNEL_REORDERER_H_
