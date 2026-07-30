/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/cli/channel_label.h"

#include <optional>
#include <string>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/map_utils.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"

namespace iamf_tools {

namespace {

absl::StatusOr<std::vector<ChannelLabel::Label>>
LookupEarChannelOrderFromNonExpandedLoudspeakerLayout(
    const ChannelAudioLayerConfig::LoudspeakerLayout& loudspeaker_layout) {
  using enum ChannelAudioLayerConfig::LoudspeakerLayout;
  using enum ChannelLabel::Label;
  static const absl::NoDestructor<
      absl::flat_hash_map<ChannelAudioLayerConfig::LoudspeakerLayout,
                          std::vector<ChannelLabel::Label>>>
      kSoundSystemToLoudspeakerLayout({
          {kLayoutMono, {kMono}},
          {kLayoutStereo, {kL2, kR2}},
          {kLayout5_1_ch, {kL5, kR5, kCentre, kLFE, kLs5, kRs5}},
          {kLayout5_1_2_ch,
           {kL5, kR5, kCentre, kLFE, kLs5, kRs5, kLtf2, kRtf2}},
          {kLayout5_1_4_ch,
           {kL5, kR5, kCentre, kLFE, kLs5, kRs5, kLtf4, kRtf4, kLtb4, kRtb4}},
          {kLayout7_1_ch,
           {kL7, kR7, kCentre, kLFE, kLss7, kRss7, kLrs7, kRrs7}},
          {kLayout7_1_2_ch,
           {kL7, kR7, kCentre, kLFE, kLss7, kRss7, kLrs7, kRrs7, kLtf2, kRtf2}},
          {kLayout7_1_4_ch,
           {kL7, kR7, kCentre, kLFE, kLss7, kRss7, kLrs7, kRrs7, kLtf4, kRtf4,
            kLtb4, kRtb4}},
          {kLayout3_1_2_ch, {kL3, kR3, kCentre, kLFE, kLtf3, kRtf3}},
          {kLayoutBinaural, {kL2, kR2}},
      });

  return LookupInMap(*kSoundSystemToLoudspeakerLayout, loudspeaker_layout,
                     "`ChannelLabel::Label` for `LoudspeakerLayout`");
}

void SetLabelsToOmittedExceptFor(
    const absl::flat_hash_set<ChannelLabel::Label>& labels_to_keep,
    std::vector<ChannelLabel::Label>& ordered_labels) {
  for (auto& label : ordered_labels) {
    if (!labels_to_keep.contains(label)) {
      label = ChannelLabel::kOmitted;
    }
  }
}

absl::StatusOr<std::vector<ChannelLabel::Label>>
LookupEarChannelOrderFromExpandedLoudspeakerLayout(
    const ChannelAudioLayerConfig::ExpandedLoudspeakerLayout&
        expanded_loudspeaker_layout) {
  using enum ChannelLabel::Label;
  static const absl::NoDestructor<std::vector<ChannelLabel::Label>>
      k9_1_6ChannelOrder({kFL, kFR, kFC, kLFE, kBL, kBR, kFLc, kFRc, kSiL, kSiR,
                          kTpFL, kTpFR, kTpBL, kTpBR, kTpSiL, kTpSiR});
  static const absl::NoDestructor<std::vector<ChannelLabel::Label>>
      k10_2_9_3ChannelOrder({kFL,    kFR,    kFC,   kLFE,  kBL,   kBR,
                             kFLc,   kFRc,   kBC,   kLFE2, kSiL,  kSiR,
                             kTpFL,  kTpFR,  kTpFC, kTpC,  kTpBL, kTpBR,
                             kTpSiL, kTpSiR, kTpBC, kBtFC, kBtFL, kBtFR});
  static const absl::NoDestructor<std::vector<ChannelLabel::Label>>
      k7_1_5_4ChannelOrder({kL7, kR7, kCentre, kLFE, kLss7, kRss7, kLrs7, kRrs7,
                            kLtf4, kRtf4, kLtb4, kRtb4, kTpC, kBtFL, kBtFR,
                            kBtBL, kBtBR});
  // Determine the related layout and then omit any irrelevant channels. This
  // ensures the permitted channels are in the same slot and allows downstream
  // processing to use the related layout's EAR matrix.
  absl::StatusOr<std::vector<ChannelLabel::Label>> related_labels;
  absl::flat_hash_set<ChannelLabel::Label> labels_to_keep;
  switch (expanded_loudspeaker_layout) {
    using enum ChannelAudioLayerConfig::ExpandedLoudspeakerLayout;
    using enum ChannelAudioLayerConfig::LoudspeakerLayout;
    case kExpandedLayoutLFE:
      related_labels = LookupEarChannelOrderFromNonExpandedLoudspeakerLayout(
          kLayout7_1_4_ch);
      labels_to_keep = {kLFE};
      break;
    case kExpandedLayoutStereoS:
      related_labels = LookupEarChannelOrderFromNonExpandedLoudspeakerLayout(
          kLayout5_1_4_ch);
      labels_to_keep = {kLs5, kRs5};
      break;
    case kExpandedLayoutStereoSS:
      related_labels = LookupEarChannelOrderFromNonExpandedLoudspeakerLayout(
          kLayout7_1_4_ch);
      labels_to_keep = {kLss7, kRss7};
      break;
    case kExpandedLayoutStereoRS:
      related_labels = LookupEarChannelOrderFromNonExpandedLoudspeakerLayout(
          kLayout7_1_4_ch);
      labels_to_keep = {kLrs7, kRrs7};
      break;
    case kExpandedLayoutStereoTF:
      related_labels = LookupEarChannelOrderFromNonExpandedLoudspeakerLayout(
          kLayout7_1_4_ch);
      labels_to_keep = {kLtf4, kRtf4};
      break;
    case kExpandedLayoutStereoTB:
      related_labels = LookupEarChannelOrderFromNonExpandedLoudspeakerLayout(
          kLayout7_1_4_ch);
      labels_to_keep = {kLtb4, kRtb4};
      break;
    case kExpandedLayoutTop4Ch:
      related_labels = LookupEarChannelOrderFromNonExpandedLoudspeakerLayout(
          kLayout7_1_4_ch);
      labels_to_keep = {kLtf4, kRtf4, kLtb4, kRtb4};
      break;
    case kExpandedLayout3_0_ch:
      related_labels = LookupEarChannelOrderFromNonExpandedLoudspeakerLayout(
          kLayout7_1_4_ch);
      labels_to_keep = {kL7, kR7, kCentre};
      break;
    case kExpandedLayout9_1_6_ch:
      return *k9_1_6ChannelOrder;
    case kExpandedLayoutStereoF:
      related_labels = *k9_1_6ChannelOrder;
      labels_to_keep = {kFL, kFR};
      break;
    case kExpandedLayoutStereoSi:
      related_labels = *k9_1_6ChannelOrder;
      labels_to_keep = {kSiL, kSiR};
      break;
    case kExpandedLayoutStereoTpSi:
      related_labels = *k9_1_6ChannelOrder;
      labels_to_keep = {kTpSiL, kTpSiR};
      break;
    case kExpandedLayoutTop6Ch:
      related_labels = *k9_1_6ChannelOrder;
      labels_to_keep = {kTpFL, kTpFR, kTpSiL, kTpSiR, kTpBL, kTpBR};
      break;
    case kExpandedLayout10_2_9_3:
      return *k10_2_9_3ChannelOrder;
    case kExpandedLayoutLfePair:
      related_labels = *k10_2_9_3ChannelOrder;
      labels_to_keep = {kLFE, kLFE2};
      break;
    case kExpandedLayoutBottom3Ch:
      related_labels = *k10_2_9_3ChannelOrder;
      labels_to_keep = {kBtFC, kBtFL, kBtFR};
      break;
    case kExpandedLayout7_1_5_4Ch:
      return *k7_1_5_4ChannelOrder;
    case kExpandedLayoutBottom4Ch:
      related_labels = *k7_1_5_4ChannelOrder;
      labels_to_keep = {kBtFL, kBtFR, kBtBL, kBtBR};
      break;
    case kExpandedLayoutTop1Ch:
      related_labels = *k7_1_5_4ChannelOrder;
      labels_to_keep = {kTpC};
      break;
    case kExpandedLayoutTop5Ch:
      related_labels = *k7_1_5_4ChannelOrder;
      labels_to_keep = {kLtf4, kRtf4, kLtb4, kRtb4, kTpC};
      break;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Reserved or unknown expanded_loudspeaker_layout= ",
                       expanded_loudspeaker_layout));
  }

  // Leave the labels to keep in their original slot, but filter out all other
  // labels.
  if (!related_labels.ok()) {
    return related_labels.status();
  }
  SetLabelsToOmittedExceptFor(labels_to_keep, *related_labels);
  return related_labels;
}

}  // namespace

absl::StatusOr<ChannelLabel::Label>
ChannelLabel::AmbisonicsChannelNumberToLabel(int ambisonics_channel_number) {
  using enum ChannelLabel::Label;
  static const absl::NoDestructor<absl::flat_hash_map<int, ChannelLabel::Label>>
      kAmbisonicsChannelNumberToLabel({
          {0, kA0},   {1, kA1},   {2, kA2},   {3, kA3},   {4, kA4},
          {5, kA5},   {6, kA6},   {7, kA7},   {8, kA8},   {9, kA9},
          {10, kA10}, {11, kA11}, {12, kA12}, {13, kA13}, {14, kA14},
          {15, kA15}, {16, kA16}, {17, kA17}, {18, kA18}, {19, kA19},
          {20, kA20}, {21, kA21}, {22, kA22}, {23, kA23}, {24, kA24},
      });

  return LookupInMap(*kAmbisonicsChannelNumberToLabel,
                     ambisonics_channel_number,
                     "`Channel::Label` for `ambisonics_channel_number`");
}

std::string ChannelLabel::LabelToStringForDebugging(Label label) {
  using enum ChannelLabel::Label;
  switch (label) {
    case kOmitted:
      return "Omitted";
    case kMono:
      return "M";
    case kL2:
      return "L2";
    case kR2:
      return "R2";
    case kDemixedR2:
      return "DemixedR2";
    case kObjectChannel0:
      return "ObjectChannel0";
    case kObjectChannel1:
      return "ObjectChannel1";
    case kCentre:
      return "C";
    case kLFE:
      return "LFE";
    case kL3:
      return "L3";
    case kR3:
      return "R3";
    case kRtf3:
      return "Rtf3";
    case kLtf3:
      return "Ltf3";
    case kDemixedL3:
      return "DemixedL3";
    case kDemixedR3:
      return "DemixedR3";
    case kL5:
      return "L5";
    case kR5:
      return "R5";
    case kLs5:
      return "Ls5";
    case kRs5:
      return "Rs5";
    case kDemixedL5:
      return "DemixedL5";
    case kDemixedR5:
      return "DemixedR5";
    case kDemixedLs5:
      return "DemixedLs5";
    case kDemixedRs5:
      return "DemixedRs5";
    case kLtf2:
      return "Ltf2";
    case kRtf2:
      return "Rtf2";
    case kDemixedRtf2:
      return "DemixedRtf2";
    case kDemixedLtf2:
      return "DemixedLtf2";
    case kLtf4:
      return "Ltf4";
    case kRtf4:
      return "Rtf4";
    case kLtb4:
      return "Ltb4";
    case kRtb4:
      return "Rtb4";
    case kDemixedLtb4:
      return "DemixedLtb4";
    case kDemixedRtb4:
      return "DemixedRtb4";
    case kL7:
      return "L7";
    case kR7:
      return "R7";
    case kLss7:
      return "Lss7";
    case kRss7:
      return "Rss7";
    case kLrs7:
      return "Lrs7";
    case kRrs7:
      return "Rrs7";
    case kDemixedL7:
      return "DemixedL7";
    case kDemixedR7:
      return "DemixedR7";
    case kDemixedLrs7:
      return "DemixedLrs7";
    case kDemixedRrs7:
      return "DemixedRrs7";
    case kFLc:
      return "FLc";
    case kFC:
      return "FC";
    case kFRc:
      return "FRc";
    case kFL:
      return "FL";
    case kFR:
      return "FR";
    case kSiL:
      return "SiL";
    case kSiR:
      return "SiR";
    case kBL:
      return "BL";
    case kBR:
      return "BR";
    case kTpFL:
      return "TpFL";
    case kTpFR:
      return "TpFR";
    case kTpSiL:
      return "TpSiL";
    case kTpSiR:
      return "TpSiR";
    case kTpBL:
      return "TpBL";
    case kTpBR:
      return "TpBR";
    case kBC:
      return "BC";
    case kLFE2:
      return "LFE2";
    case kTpFC:
      return "TpFC";
    case kTpC:
      return "TpC";
    case kTpBC:
      return "TpBC";
    case kBtFC:
      return "BtFC";
    case kBtFL:
      return "BtFL";
    case kBtFR:
      return "BtFR";
    case kBtBL:
      return "BtBL";
    case kBtBR:
      return "BtBR";
    case kA0:
      return "A0";
    case kA1:
      return "A1";
    case kA2:
      return "A2";
    case kA3:
      return "A3";
    case kA4:
      return "A4";
    case kA5:
      return "A5";
    case kA6:
      return "A6";
    case kA7:
      return "A7";
    case kA8:
      return "A8";
    case kA9:
      return "A9";
    case kA10:
      return "A10";
    case kA11:
      return "A11";
    case kA12:
      return "A12";
    case kA13:
      return "A13";
    case kA14:
      return "A14";
    case kA15:
      return "A15";
    case kA16:
      return "A16";
    case kA17:
      return "A17";
    case kA18:
      return "A18";
    case kA19:
      return "A19";
    case kA20:
      return "A20";
    case kA21:
      return "A21";
    case kA22:
      return "A22";
    case kA23:
      return "A23";
    case kA24:
      return "A24";
  }

  // The above switch statement is exhaustive.
  ABSL_LOG(FATAL) << "Enum out of range.";
}

absl::StatusOr<ChannelLabel::Label> ChannelLabel::GetDemixedLabel(
    ChannelLabel::Label label) {
  using enum ChannelLabel::Label;
  static const absl::NoDestructor<
      absl::flat_hash_map<ChannelLabel::Label, ChannelLabel::Label>>
      kChannelLabelToDemixedLabel({{kR2, kDemixedR2},
                                   {kL3, kDemixedL3},
                                   {kR3, kDemixedR3},
                                   {kL5, kDemixedL5},
                                   {kR5, kDemixedR5},
                                   {kLs5, kDemixedLs5},
                                   {kRs5, kDemixedRs5},
                                   {kLtf2, kDemixedLtf2},
                                   {kRtf2, kDemixedRtf2},
                                   {kLtb4, kDemixedLtb4},
                                   {kRtb4, kDemixedRtb4},
                                   {kL7, kDemixedL7},
                                   {kR7, kDemixedR7},
                                   {kLrs7, kDemixedLrs7},
                                   {kRrs7, kDemixedRrs7}});
  return LookupInMap(*kChannelLabelToDemixedLabel, label,
                     "Demixed label for `ChannelLabel::Label`");
}

absl::StatusOr<std::vector<ChannelLabel::Label>>
ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
    ChannelAudioLayerConfig::LoudspeakerLayout loudspeaker_layout,
    const std::optional<ChannelAudioLayerConfig::ExpandedLoudspeakerLayout>&
        expanded_loudspeaker_layout) {
  if (loudspeaker_layout == ChannelAudioLayerConfig::kLayoutExpanded) {
    RETURN_IF_NOT_OK(ValidateHasValue(expanded_loudspeaker_layout,
                                      "expanded_loudspeaker_layout"));
    return LookupEarChannelOrderFromExpandedLoudspeakerLayout(
        *expanded_loudspeaker_layout);
  } else {
    return LookupEarChannelOrderFromNonExpandedLoudspeakerLayout(
        loudspeaker_layout);
  }
}

absl::StatusOr<absl::flat_hash_set<ChannelLabel::Label>>
ChannelLabel::LookupLabelsToReconstructFromScalableLoudspeakerLayout(
    ChannelAudioLayerConfig::LoudspeakerLayout loudspeaker_layout,
    const std::optional<ChannelAudioLayerConfig::ExpandedLoudspeakerLayout>&
        expanded_loudspeaker_layout) {
  if (loudspeaker_layout == ChannelAudioLayerConfig::kLayoutExpanded) {
    RETURN_IF_NOT_OK(ValidateHasValue(expanded_loudspeaker_layout,
                                      "expanded_loudspeaker_layout"));
    // OK. Expanded layouts may only exist in a single-layer and thus never need
    // to be reconstructed as of IAMF v1.1.0.
    return absl::flat_hash_set<ChannelLabel::Label>{};
  }
  // Reconstruct the highest layer.
  const auto ordered_labels =
      ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
          loudspeaker_layout, expanded_loudspeaker_layout);
  if (!ordered_labels.ok()) {
    return ordered_labels.status();
  } else {
    return absl::flat_hash_set<ChannelLabel::Label>(ordered_labels->begin(),
                                                    ordered_labels->end());
  }
}

absl::StatusOr<ChannelLabel::Label>
ChannelLabel::GetDemixedChannelLabelForReconGain(
    const ChannelAudioLayerConfig::LoudspeakerLayout& layout,
    const ReconGainElement::ReconGainFlagBitmask& flag) {
  switch (flag) {
    using enum ReconGainElement::ReconGainFlagBitmask;
    using enum ChannelLabel::Label;
    using enum ChannelAudioLayerConfig::LoudspeakerLayout;
    case kReconGainFlagL:
      if (layout == kLayout5_1_ch || layout == kLayout5_1_2_ch ||
          layout == kLayout5_1_4_ch) {
        return kDemixedL5;
      } else if (layout == kLayout7_1_ch || layout == kLayout7_1_2_ch ||
                 layout == kLayout7_1_4_ch) {
        return kDemixedL7;
      } else if (layout == kLayout3_1_2_ch) {
        return kDemixedL3;
      } else {
        ABSL_LOG(WARNING)
            << "Unexpected recon gain flag. No corresponding channel label.";
        return absl::InvalidArgumentError("Unexpected recon gain flag.");
      }
    case kReconGainFlagC:
      ABSL_LOG(WARNING)
          << "Unexpected recon gain flag. No corresponding channel label.";
      return absl::InvalidArgumentError("Unexpected recon gain flag.");
    case kReconGainFlagR:
      if (layout == kLayoutStereo) {
        return kDemixedR2;
      } else if (layout == kLayout5_1_ch || layout == kLayout5_1_2_ch ||
                 layout == kLayout5_1_4_ch) {
        return kDemixedR5;
      } else if (layout == kLayout7_1_ch || layout == kLayout7_1_2_ch ||
                 layout == kLayout7_1_4_ch) {
        return kDemixedR7;
      } else if (layout == kLayout3_1_2_ch) {
        return kDemixedR3;
      } else {
        ABSL_LOG(WARNING)
            << "Unexpected recon gain flag. No corresponding channel label.";
        return absl::InvalidArgumentError("Unexpected recon gain flag.");
      }
    case kReconGainFlagLss:
      return kDemixedLs5;
    case kReconGainFlagRss:
      return kDemixedRs5;
    case kReconGainFlagLtf:
      return kDemixedLtf2;
    case kReconGainFlagRtf:
      return kDemixedRtf2;
    case kReconGainFlagLrs:
      return kDemixedLrs7;
    case kReconGainFlagRrs:
      return kDemixedRrs7;
    case kReconGainFlagLtb:
      return kDemixedLtb4;
    case kReconGainFlagRtb:
      return kDemixedRtb4;
    case kReconGainFlagLfe:
    default:
      ABSL_LOG(WARNING)
          << "Unexpected recon gain flag. No corresponding channel label.";
      return absl::InvalidArgumentError("Unexpected recon gain flag.");
  }
}

}  // namespace iamf_tools
