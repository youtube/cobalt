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
#ifndef CLI_ENCODER_MAIN_LIB_H_
#define CLI_ENCODER_MAIN_LIB_H_

#include <string>

#include "absl/status/status.h"
#include "iamf/cli/proto/user_metadata.pb.h"

/*!\brief Writes an IAMF bitstream and wav files to the output files.
 *
 * \param user_metadata Input user metadata describing the IAMF stream.
 * \param input_wav_directory Directory which contains the input wav files.
 * \param output_iamf_directory Directory to output IAMF files to.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
namespace iamf_tools {
absl::Status TestMain(const iamf_tools_cli_proto::UserMetadata& user_metadata,
                      const std::string& input_wav_directory,
                      const std::string& output_iamf_directory);
}

#endif  // CLI_ENCODER_MAIN_LIB_H_
