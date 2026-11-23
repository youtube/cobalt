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

#ifndef CLI_RENDERER_GAINS_PRECOMPUTED_COMPRESSED_GAINS_DECODER_H_
#define CLI_RENDERER_GAINS_PRECOMPUTED_COMPRESSED_GAINS_DECODER_H_

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "iamf/cli/renderer/gains/precomputed_compressed_gains.h"

namespace iamf_tools {

/*!\brief Decompresses a CompressedMatrix object back into standard vector of
 *        vectors.
 *
 * \param input_layout_name Name of the input layout (e.g., "0+2+0", "A3").
 * \param output_layout_name Name of the output layout.
 * \param compressed_matrix The compressed matrix to expand.
 * \return Uncompressed 2D vector of doubles.
 */
absl::StatusOr<std::vector<std::vector<double>>> DecompressMatrix(
    const std::string& input_layout_name, const std::string& output_layout_name,
    const CompressedMatrix& compressed_matrix);

}  // namespace iamf_tools

#endif  // CLI_RENDERER_GAINS_PRECOMPUTED_COMPRESSED_GAINS_DECODER_H_
