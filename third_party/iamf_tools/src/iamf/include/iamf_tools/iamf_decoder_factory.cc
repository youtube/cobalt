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
#include "iamf/include/iamf_tools/iamf_decoder_factory.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "absl/log/absl_log.h"
#include "iamf/api/decoder/iamf_decoder.h"
#include "iamf/include/iamf_tools/iamf_decoder_interface.h"
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"

namespace iamf_tools {
namespace api {
namespace {
IamfDecoder::Settings ApiToInternalSettings(
    const IamfDecoderFactory::Settings& settings) {
  IamfDecoder::Settings internal_settings = {
      .requested_mix = settings.requested_mix,
      .channel_ordering = settings.channel_ordering,
      .requested_profile_versions = settings.requested_profile_versions,
      .requested_output_sample_type = settings.requested_output_sample_type,
      .trimming_settings = settings.trimming_settings,
  };
  return internal_settings;
}
}  // namespace

std::unique_ptr<IamfDecoderInterface> IamfDecoderFactory::Create(
    const IamfDecoderFactory::Settings& settings) {
  std::unique_ptr<IamfDecoder> output_decoder;
  IamfStatus status =
      IamfDecoder::Create(ApiToInternalSettings(settings), output_decoder);
  if (!status.ok()) {
    ABSL_LOG(ERROR) << "Failed to create decoder: " << status.error_message;
    return nullptr;
  }
  return std::move(output_decoder);
}

std::unique_ptr<IamfDecoderInterface> IamfDecoderFactory::CreateFromDescriptors(
    const IamfDecoderFactory::Settings& settings, const uint8_t* input_buffer,
    size_t input_buffer_size) {
  std::unique_ptr<IamfDecoder> output_decoder;
  IamfStatus status = IamfDecoder::CreateFromDescriptors(
      ApiToInternalSettings(settings), input_buffer, input_buffer_size,
      output_decoder);
  if (!status.ok()) {
    ABSL_LOG(ERROR) << "Failed to create decoder: " << status.error_message;
    return nullptr;
  }
  return std::move(output_decoder);
}

}  // namespace api
}  // namespace iamf_tools
