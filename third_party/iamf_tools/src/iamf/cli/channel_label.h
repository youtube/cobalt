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

#ifndef CLI_CHANNEL_LABEL_H_
#define CLI_CHANNEL_LABEL_H_

#include <optional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"

namespace iamf_tools {

/*!\brief Enums and static functions to help process channel labels.*/
class ChannelLabel {
 public:
  /*!\brief Labels associated with input or output channels.
   *
   * Labels naming conventions are based on the IAMF spec
   * (https://aomediacodec.github.io/iamf/#processing-downmixmatrix,
   * https://aomediacodec.github.io/iamf/#iamfgeneration-scalablechannelaudio-downmixmechanism).
   */
  enum Label {
    kOmitted,
    // Mono channels.
    kMono,
    // Stereo or binaural channels.
    kL2,
    kR2,
    kDemixedR2,
    // Object channels.
    kObjectChannel0,
    kObjectChannel1,
    // Centre channel common to several layouts (e.g. 3.1.2, 5.x.y, 7.x.y).
    kCentre,
    // LFE channel common to several layouts (e.g. 3.1.2, 5.1.y, 7.1.y, 9.1.6).
    kLFE,
    // 3.1.2 surround channels.
    kL3,
    kR3,
    kLtf3,
    kRtf3,
    kDemixedL3,
    kDemixedR3,
    // 5.x.y surround channels.
    kL5,
    kR5,
    kLs5,
    kRs5,
    kDemixedL5,
    kDemixedR5,
    kDemixedLs5,
    kDemixedRs5,
    // Common channels between 5.1.2 and 7.1.2.
    kLtf2,
    kRtf2,
    kDemixedLtf2,
    kDemixedRtf2,
    // Common channels between 5.1.4 and 7.1.4.
    kLtf4,
    kRtf4,
    kLtb4,
    kRtb4,
    kDemixedLtb4,
    kDemixedRtb4,
    // 7.x.y surround channels.
    kL7,
    kR7,
    kLss7,
    kRss7,
    kLrs7,
    kRrs7,
    kDemixedL7,
    kDemixedR7,
    kDemixedLrs7,
    kDemixedRrs7,
    // Common channels between 9.1.6 and 10.2.9.3.
    kFLc,
    kFC,
    kFRc,
    kFL,
    kFR,
    kSiL,
    kSiR,
    kBL,
    kBR,
    kTpFL,
    kTpFR,
    kTpSiL,
    kTpSiR,
    kTpBL,
    kTpBR,
    // Extra channels for 10.2.9.3.
    kBC,
    kLFE2,
    kTpFC,
    kTpC,
    kTpBC,
    kBtFC,
    kBtFL,
    kBtFR,
    // Extra channels for 7.5.1.4.
    kBtBL,
    kBtBR,
    // Ambisonics channels.
    kA0,
    kA1,
    kA2,
    kA3,
    kA4,
    kA5,
    kA6,
    kA7,
    kA8,
    kA9,
    kA10,
    kA11,
    kA12,
    kA13,
    kA14,
    kA15,
    kA16,
    kA17,
    kA18,
    kA19,
    kA20,
    kA21,
    kA22,
    kA23,
    kA24,
  };

  template <typename Sink>
  friend void AbslStringify(Sink& sink, Label e) {
    sink.Append(LabelToStringForDebugging(e));
  }

  /*!\brief Converts the `Label` to a debugging string.
   *
   * \param label Label to convert.
   * \return Converted label.
   */
  static std::string LabelToStringForDebugging(Label label);

  /*!\brief Gets the channel label for an ambisonics channel number.
   *
   * \param ambisonics_channel_number Ambisonics channel number to get the label
   *        of.
   * \return Converted label. A specific status on failure.
   */
  static absl::StatusOr<Label> AmbisonicsChannelNumberToLabel(
      int ambisonics_channel_number);

  /*!\brief Returns the demixed version of a channel label.
   *
   * \param label Label to get the demixed version of.
   * \return Converted label on success. A specific status if the channel is not
   *         suitable for demixing. A specific status on other failures.
   */
  static absl::StatusOr<Label> GetDemixedLabel(Label label);

  /*!\brief Gets the channel ordering to use for the associated input layout.
   *
   * The output is ordered to agree with the "precomputed" EAR matrices. Certain
   * layouts are based on other layouts. The channels which are excluded are
   * represented by `ChannelLabel::Label::kOmitted`.
   *
   * \param loudspeaker_layout Layout to get the channel ordering from.
   * \param expanded_loudspeaker_layout Associated expanded loudspeaker layout
   *        or `std::nullopt` when it is not relevant.
   * \return Channel ordering associated with the layout if known. Or a specific
   *         status on failure.
   */
  static absl::StatusOr<std::vector<ChannelLabel::Label>>
  LookupEarChannelOrderFromScalableLoudspeakerLayout(
      ChannelAudioLayerConfig::LoudspeakerLayout loudspeaker_layout,
      const std::optional<ChannelAudioLayerConfig::ExpandedLoudspeakerLayout>&
          expanded_loudspeaker_layout);

  /*!\brief Gets the labels related to reconstructing the input layout.
   *
   * Returns the labels that may be needed to reconstruct the
   * `loudspeaker_layout`. This function is useful when audio frames represent
   * channels which do agree with the `loudspeaker_layout`. Usually this occurs
   * when there are multiple layers in a scalable channel audio element.
   *
   * \param loudspeaker_layout Layout to get the labels to reconstruct from.
   * \param expanded_loudspeaker_layout Associated expanded loudspeaker layout
   *        or `std::nullopt` when it is not relevant.
   * \return Labels to reconstruct the associated layout if known. Or a specific
   *         status on failure.
   */
  static absl::StatusOr<absl::flat_hash_set<ChannelLabel::Label>>
  LookupLabelsToReconstructFromScalableLoudspeakerLayout(
      ChannelAudioLayerConfig::LoudspeakerLayout loudspeaker_layout,
      const std::optional<ChannelAudioLayerConfig::ExpandedLoudspeakerLayout>&
          expanded_loudspeaker_layout);

  /*!\brief Gets the demixed labels for a given recon gain flag and layout.
   *
   * \param loudspeaker_layout Layout of the layer to get the labels from.
   * \param recon_gain_flag Specifies the recon gain to get the labels for.
   * \return Demixed channel labels. Or a specific status on failure.
   */
  static absl::StatusOr<ChannelLabel::Label> GetDemixedChannelLabelForReconGain(
      const ChannelAudioLayerConfig::LoudspeakerLayout& layout,
      const ReconGainElement::ReconGainFlagBitmask& recon_gain_flag);
};

}  // namespace iamf_tools

#endif  // CLI_CHANNEL_LABEL_H_
