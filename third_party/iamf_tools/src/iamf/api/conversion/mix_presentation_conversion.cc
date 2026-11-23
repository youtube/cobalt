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

#include "iamf/api/conversion/mix_presentation_conversion.h"

#include <optional>
#include <variant>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {
namespace {

absl::StatusOr<api::OutputLayout> InternalLayoutToApiLayout(
    const LoudspeakersReservedOrBinauralLayout& specific_layout,
    const Layout::LayoutType layout_type) {
  switch (layout_type) {
    using enum Layout::LayoutType;
    case kLayoutTypeBinaural:
      return api::OutputLayout::kIAMF_Binaural;
    case kLayoutTypeLoudspeakersSsConvention:
      return absl::InvalidArgumentError(
          absl::StrCat("Mismatching specific layout and layout type; expecting "
                       "layout_type in {",
                       kLayoutTypeReserved0, ", ", kLayoutTypeReserved1, ", ",
                       kLayoutTypeBinaural, "} but got ", layout_type));
    case kLayoutTypeReserved0:
    case kLayoutTypeReserved1:
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unsupported layout type: ", layout_type));
  }
}

absl::StatusOr<api::OutputLayout> InternalLayoutToApiLayout(
    const LoudspeakersSsConventionLayout& specific_layout,
    const Layout::LayoutType layout_type) {
  if (layout_type != Layout::kLayoutTypeLoudspeakersSsConvention) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Mismatching specific layout and layout type; expecting layout_type = ",
        Layout::kLayoutTypeLoudspeakersSsConvention, " but got ", layout_type));
  }

  switch (specific_layout.sound_system) {
    case LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0:
      return api::OutputLayout::kItu2051_SoundSystemA_0_2_0;
    case LoudspeakersSsConventionLayout::kSoundSystemB_0_5_0:
      return api::OutputLayout::kItu2051_SoundSystemB_0_5_0;
    case LoudspeakersSsConventionLayout::kSoundSystemC_2_5_0:
      return api::OutputLayout::kItu2051_SoundSystemC_2_5_0;
    case LoudspeakersSsConventionLayout::kSoundSystemD_4_5_0:
      return api::OutputLayout::kItu2051_SoundSystemD_4_5_0;
    case LoudspeakersSsConventionLayout::kSoundSystemE_4_5_1:
      return api::OutputLayout::kItu2051_SoundSystemE_4_5_1;
    case LoudspeakersSsConventionLayout::kSoundSystemF_3_7_0:
      return api::OutputLayout::kItu2051_SoundSystemF_3_7_0;
    case LoudspeakersSsConventionLayout::kSoundSystemG_4_9_0:
      return api::OutputLayout::kItu2051_SoundSystemG_4_9_0;
    case LoudspeakersSsConventionLayout::kSoundSystemH_9_10_3:
      return api::OutputLayout::kItu2051_SoundSystemH_9_10_3;
    case LoudspeakersSsConventionLayout::kSoundSystemI_0_7_0:
      return api::OutputLayout::kItu2051_SoundSystemI_0_7_0;
    case LoudspeakersSsConventionLayout::kSoundSystemJ_4_7_0:
      return api::OutputLayout::kItu2051_SoundSystemJ_4_7_0;
    case LoudspeakersSsConventionLayout::kSoundSystem10_2_7_0:
      return api::OutputLayout::kIAMF_SoundSystemExtension_2_7_0;
    case LoudspeakersSsConventionLayout::kSoundSystem11_2_3_0:
      return api::OutputLayout::kIAMF_SoundSystemExtension_2_3_0;
    case LoudspeakersSsConventionLayout::kSoundSystem12_0_1_0:
      return api::OutputLayout::kIAMF_SoundSystemExtension_0_1_0;
    case LoudspeakersSsConventionLayout::kSoundSystem13_6_9_0:
      return api::OutputLayout::kIAMF_SoundSystemExtension_6_9_0;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid layout: ", specific_layout.sound_system));
  };
}

Layout MakeLayout(LoudspeakersSsConventionLayout::SoundSystem sound_system) {
  return {.layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
          .specific_layout =
              LoudspeakersSsConventionLayout{.sound_system = sound_system}};
}

Layout MakeBinauralLayout() {
  return {
      .layout_type = Layout::kLayoutTypeBinaural,
      .specific_layout = LoudspeakersReservedOrBinauralLayout{.reserved = 0}};
}

}  // namespace

std::optional<Layout> ApiToInternalType(
    std::optional<api::OutputLayout> api_output_layout) {
  if (!api_output_layout.has_value()) {
    return std::nullopt;
  }
  switch (*api_output_layout) {
    case api::OutputLayout::kItu2051_SoundSystemA_0_2_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0);
    case api::OutputLayout::kItu2051_SoundSystemB_0_5_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystemB_0_5_0);
    case api::OutputLayout::kItu2051_SoundSystemC_2_5_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystemC_2_5_0);
    case api::OutputLayout::kItu2051_SoundSystemD_4_5_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystemD_4_5_0);
    case api::OutputLayout::kItu2051_SoundSystemE_4_5_1:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystemE_4_5_1);
    case api::OutputLayout::kItu2051_SoundSystemF_3_7_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystemF_3_7_0);
    case api::OutputLayout::kItu2051_SoundSystemG_4_9_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystemG_4_9_0);
    case api::OutputLayout::kItu2051_SoundSystemH_9_10_3:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystemH_9_10_3);
    case api::OutputLayout::kItu2051_SoundSystemI_0_7_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystemI_0_7_0);
    case api::OutputLayout::kItu2051_SoundSystemJ_4_7_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystemJ_4_7_0);
    case api::OutputLayout::kIAMF_SoundSystemExtension_2_7_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystem10_2_7_0);
    case api::OutputLayout::kIAMF_SoundSystemExtension_2_3_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystem11_2_3_0);
    case api::OutputLayout::kIAMF_SoundSystemExtension_0_1_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystem12_0_1_0);
    case api::OutputLayout::kIAMF_SoundSystemExtension_6_9_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystem13_6_9_0);
    case api::OutputLayout::kIAMF_Binaural:
      return MakeBinauralLayout();
  };
  // Switch above is exhaustive.
  ABSL_LOG(FATAL) << "Invalid output layout.";
}

absl::StatusOr<api::OutputLayout> InternalToApiType(Layout internal_layout) {
  return std::visit(
      [=](const auto& specific_layout) {
        return InternalLayoutToApiLayout(specific_layout,
                                         internal_layout.layout_type);
      },
      internal_layout.specific_layout);
}

}  // namespace iamf_tools
