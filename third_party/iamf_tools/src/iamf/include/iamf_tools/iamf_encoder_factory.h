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
#ifndef INCLUDE_IAMF_TOOLS_IAMF_ENCODER_FACTORY_H_
#define INCLUDE_IAMF_TOOLS_IAMF_ENCODER_FACTORY_H_

#include <memory>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "iamf_encoder_interface.h"

namespace iamf_tools {
namespace api {

/*!\brief Factory functions for creating IAMF encoders.*/
class IamfEncoderFactory {
 public:
  /*!\brief Factory function to create an encoder for writing IAMF to a file.
   *
   * This encoder will automatically produce a standalone IAMF file according to
   * the spec (https://aomediacodec.github.io/iamf/#standalone) at the requested
   * path.
   *
   * \param serialized_user_metadata Input user metadata describing the IAMF
   *        stream, which is serialized as a `UserMetadata` protocol buffer.
   * \param output_file_name File name to write the IAMF file to.
   * \return Encoder on success, or a specific status on failure.
   */
  static absl::StatusOr<std::unique_ptr<IamfEncoderInterface>>
  CreateFileGeneratingIamfEncoder(absl::string_view serialized_user_metadata,
                                  absl::string_view output_file_name);

  /*!\brief Factory function to create an encoder for streaming IAMF.
   *
   * This encoder is useful to stream an IA Sequence to a client.
   *
   * \param serialized_user_metadata Input user metadata describing the IAMF
   *        stream, which is serialized as a `UserMetadata` protocol buffer.
   * \return Encoder on success, or a specific status on failure.
   */
  static absl::StatusOr<std::unique_ptr<IamfEncoderInterface>>
  CreateIamfEncoder(absl::string_view serialized_user_metadata);

  IamfEncoderFactory() = delete;
};

}  // namespace api
}  // namespace iamf_tools

#endif  // INCLUDE_IAMF_TOOLS_IAMF_ENCODER_FACTORY_H_
