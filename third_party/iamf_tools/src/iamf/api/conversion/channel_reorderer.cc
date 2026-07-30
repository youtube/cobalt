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

#include "iamf/api/conversion/channel_reorderer.h"

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

using ::absl::InvalidArgumentError;
using ::absl::OkStatus;
using ::absl::Status;

namespace {

// No transformation.
absl::Status NoOp(std::vector<absl::Span<const InternalSampleType>>& samples) {
  return OkStatus();
}

absl::Status SwapBackAndSides(
    std::vector<absl::Span<const InternalSampleType>>& samples) {
  if (samples.size() < 8) {
    return InvalidArgumentError(
        "Channel count smaller than expected for reorderer.");
  }
  // 7-something layout are ordered as [L, R, C, LFE, Lss, Rss, Lrs, Rrs].
  // Android needs rear before side surrounds.
  std::swap(samples[4], samples[6]);
  std::swap(samples[5], samples[7]);
  return OkStatus();
}

absl::Status ReorderSoundSystemFForAndroid(
    std::vector<absl::Span<const InternalSampleType>>& samples) {
  if (samples.size() < 11) {
    return InvalidArgumentError(
        "Channel count smaller than expected for reorderer.");
  }
  //             0  1  2   3   4   5   6   7   8   9    10    11
  // Ordered as [C, L, R, LH, RH, LS, RS, LB, RB, CH, LFE1, LFE2].
  // Android needs [L, R, C, LFE, BACK_LEFT, BACK_RIGHT, SIDE_LEFT, SIDE_RIGHT,
  // TOP_CENTER, TOP_FRONT_LEFT, TOP_FRONT_RIGHT, LOW_FREQUENCY_2]

  auto originals = samples;
  samples[0] = originals[1];
  samples[1] = originals[2];
  samples[2] = originals[0];
  samples[3] = originals[10];
  samples[4] = originals[7];
  samples[5] = originals[8];
  samples[6] = originals[5];
  samples[7] = originals[6];
  samples[8] = originals[9];
  samples[9] = originals[3];
  samples[10] = originals[4];
  // Channel 11 is the same.

  return OkStatus();
}

absl::Status ReorderSoundSystemGForAndroid(
    std::vector<absl::Span<const InternalSampleType>>& samples) {
  if (samples.size() < 14) {
    return InvalidArgumentError(
        "Channel count smaller than expected for reorderer.");
  }

  // Ordered as
  //  0  1  2    3    4    5    6    7    8    9   10   11   12   13
  // [L, R, C, LFE, Lss, Rss, Lrs, Rrs, Ltf, Rtf, Ltb, Rtb, Lsc, Rsc]
  // Android needs
  //  0  1  2    3          4           5                     6
  // [L, R, C, LFE, BACK_LEFT, BACK_RIGHT, FRONT_LEFT_OF_CENTER (for Lsc),
  //                     7                    8          9    10   11   12   13
  // FRONT_RIGHT_OF_CENTER (for Rsc), SIDE_LEFT, SIDE_RIGHT, Ltf, Rtf, Ltb, Rtb]

  auto originals = samples;
  // 0-3 are the same.
  samples[4] = originals[6];
  samples[5] = originals[7];
  samples[6] = originals[12];
  samples[7] = originals[13];
  samples[8] = originals[4];
  samples[9] = originals[5];
  samples[10] = originals[8];
  samples[11] = originals[9];
  samples[12] = originals[10];
  samples[13] = originals[11];

  return OkStatus();
}

absl::Status ReorderSoundSystemHForAndroid(
    std::vector<absl::Span<const InternalSampleType>>& samples) {
  if (samples.size() < 24) {
    return InvalidArgumentError(
        "Channel count smaller than expected for reorderer.");
  }

  // Ordered as
  //   0   1   2     3  4    5    6    7   8     9  10    11   12     13    14
  // [FL, FR, FC, LFE1, BL, BR, FLc, FRc, BC, LFE2, SiL, SiR, TpFL, TpFR, TpFC,
  //  15    16    17     18     19    20    21    22    23
  // TpC, TpBL, TpBR, TpSiL, TpSiR, TpBC, BtFC, BtFL, BtFR].

  // ANDROID needs
  //   0   1   2     3   4   5    6    7   8     9               10          11
  // [FL, FR, FC, LFE1, BL, BR, FLc, FRc, BC, SIDE_LEFT, SIDE_RIGHT, TOP_CENTER,
  //             12                13               14             15
  // TOP_FRONT_LEFT, TOP_FRONT_CENTER, TOP_FRONT_RIGHT, TOP_BACK_LEFT,
  //              16              17             18              19
  // TOP_BACK_CENTER, TOP_BACK_RIGHT, TOP_SIDE_LEFT, TOP_SIDE_RIGHT,
  //                 20                   21                  22    23
  //  BOTTOM_FRONT_LEFT, BOTTOM_FRONT_CENTER, BOTTOM_FRONT_RIGHT, LFE2]

  auto originals = samples;
  // 0-8 are the same.
  samples[9] = originals[10];
  samples[10] = originals[11];
  samples[11] = originals[15];
  // 12 is the same
  samples[13] = originals[14];
  samples[14] = originals[13];
  samples[15] = originals[16];
  samples[16] = originals[20];
  // 17-19 are the same.
  samples[20] = originals[22];
  // 21 is the same.
  samples[22] = originals[23];
  samples[23] = originals[9];

  return OkStatus();
}

std::function<absl::Status(std::vector<absl::Span<const InternalSampleType>>&)>
MakeFunction(LoudspeakersSsConventionLayout::SoundSystem original_layout,
             ChannelReorderer::RearrangementScheme scheme) {
  switch (scheme) {
    case ChannelReorderer::RearrangementScheme::kDefaultNoOp:
    default:
      return NoOp;
    case ChannelReorderer::RearrangementScheme::kReorderForAndroid: {
      switch (original_layout) {
        // For these, Android matches
        case LoudspeakersSsConventionLayout::SoundSystem::kSoundSystemA_0_2_0:
        case LoudspeakersSsConventionLayout::SoundSystem::kSoundSystemB_0_5_0:
        case LoudspeakersSsConventionLayout::SoundSystem::kSoundSystemC_2_5_0:
        case LoudspeakersSsConventionLayout::SoundSystem::kSoundSystemD_4_5_0:
        case LoudspeakersSsConventionLayout::SoundSystem::kSoundSystemE_4_5_1:
        case LoudspeakersSsConventionLayout::SoundSystem::kSoundSystem11_2_3_0:
        case LoudspeakersSsConventionLayout::SoundSystem::kSoundSystem12_0_1_0:
        case LoudspeakersSsConventionLayout::SoundSystem::kSoundSystem13_6_9_0:
        default:
          return NoOp;
        // These just need to have back L/R before side L/R.
        case LoudspeakersSsConventionLayout::SoundSystem::kSoundSystemI_0_7_0:
        case LoudspeakersSsConventionLayout::SoundSystem::kSoundSystemJ_4_7_0:
        case LoudspeakersSsConventionLayout::SoundSystem::kSoundSystem10_2_7_0:
          return SwapBackAndSides;
        case LoudspeakersSsConventionLayout::SoundSystem::kSoundSystemF_3_7_0:
          return ReorderSoundSystemFForAndroid;
        case LoudspeakersSsConventionLayout::SoundSystem::kSoundSystemG_4_9_0:
          return ReorderSoundSystemGForAndroid;
        case LoudspeakersSsConventionLayout::SoundSystem::kSoundSystemH_9_10_3:
          return ReorderSoundSystemHForAndroid;
      }
    }
  }
}

}  // namespace

ChannelReorderer::ChannelReorderer(
    std::function<Status(std::vector<absl::Span<const InternalSampleType>>&)>
        reorder_function)
    : reorder_function_(reorder_function) {}

ChannelReorderer ChannelReorderer::Create(
    LoudspeakersSsConventionLayout::SoundSystem original_layout,
    RearrangementScheme scheme) {
  if (scheme == RearrangementScheme::kDefaultNoOp) {
    return ChannelReorderer(NoOp);
  }
  return ChannelReorderer(MakeFunction(original_layout, scheme));
}

absl::Status ChannelReorderer::Reorder(
    std::vector<absl::Span<const InternalSampleType>>& audio_frame) {
  return reorder_function_(audio_frame);
}

}  // namespace iamf_tools
