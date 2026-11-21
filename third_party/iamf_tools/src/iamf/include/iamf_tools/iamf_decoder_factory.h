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

#ifndef INCLUDE_IAMF_TOOLS_IAMF_DECODER_FACTORY_H_
#define INCLUDE_IAMF_TOOLS_IAMF_DECODER_FACTORY_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_set>

#include "iamf_decoder_interface.h"
#include "iamf_tools_api_types.h"

namespace iamf_tools {
namespace api {

/*!\brief Factory functions for creating IAMF decoders. */
class IamfDecoderFactory {
 public:
  /*!\brief Settings for the `IamfDecoderInterface`. */
  struct Settings {
    // Specifies the desired output Mix Presentation ID and/or layout.
    // The selected result will be retrievable after Descriptor OBUs have been
    // processed.
    RequestedMix requested_mix;

    // Specify a different ordering for the output samples.  Only specific
    // orderings are available, custom or granular control is not possible.
    ChannelOrdering channel_ordering = ChannelOrdering::kIamfOrdering;

    // Specifies the desired profile versions. Clients should explicitly provide
    // the profiles they are interested in. Otherwise, the default value will
    // evolve in the future, based on recommendations or additions to the IAMF
    // spec.
    //
    // If the descriptor OBUs do not contain a mix presentation which is
    // suitable for one of the matching profiles the decoder will return an
    // error. Typically all profiles the client is capable of handling should
    // be provided, to ensure compatibility with as many mixes as possible.
    std::unordered_set<ProfileVersion> requested_profile_versions = {
        ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile,
        ProfileVersion::kIamfBaseEnhancedProfile};

    // Specifies the desired bit depth for the output samples.
    OutputSampleType requested_output_sample_type =
        OutputSampleType::kInt32LittleEndian;

    TrimmingSettings trimming_settings;
  };

  /*!\brief Creates an IamfDecoderInterface.
   *
   * This function should be used for pure streaming applications in which the
   * descriptor OBUs are not known in advance.
   *
   * \param settings Settings to configure the decoder.
   * \return A unique_ptr to the IamfDecoderInterface upon success. nullptr
   *         if an error occurred.
   */
  static std::unique_ptr<IamfDecoderInterface> Create(const Settings& settings);

  /*!\brief Creates an IamfDecoderInterface from a known set of descriptor OBUs.
   *
   * This function should be used for applications in which the descriptor OBUs
   * are known in advance. When creating the decoder via this mode, future calls
   * to decode must pass complete temporal units. This is useful when decoding
   * mp4.
   *
   * \param settings Settings to configure the decoder.
   * \param input_buffer Bitstream containing all the descriptor OBUs and
   *        only descriptor OBUs.
   * \param input_buffer_size Size in bytes of the input buffer.
   * \return A unique_ptr to the IamfDecoderInterface upon success. nullptr if
   *         an error occurred.
   */
  static std::unique_ptr<IamfDecoderInterface> CreateFromDescriptors(
      const Settings& settings, const uint8_t* input_buffer,
      size_t input_buffer_size);
};

}  // namespace api
}  // namespace iamf_tools

#endif  // INCLUDE_IAMF_TOOLS_IAMF_DECODER_FACTORY_H_
