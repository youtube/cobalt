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

#include "iamf/cli/renderer/gains/get_gains.h"

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/renderer/gains/precomputed_compressed_gains.h"
#include "iamf/cli/renderer/gains/precomputed_compressed_gains_decoder.h"

namespace iamf_tools {

absl::StatusOr<std::vector<std::vector<double>>> GetGainsForLayoutPair(
    absl::string_view input_key, absl::string_view output_key) {
  for (const auto& entry : GetPrecomputedGainsTable()) {
    if (entry.input_layout == input_key && entry.output_layout == output_key) {
      CompressedMatrix compressed_matrix;
      if (entry.is_dense) {
        compressed_matrix.dense_data.assign(entry.dense_data,
                                            entry.dense_data + entry.size);
      } else {
        compressed_matrix.sparse_blob.assign(entry.sparse_blob,
                                             entry.sparse_blob + entry.size);
      }
      return DecompressMatrix(std::string(input_key), std::string(output_key),
                              compressed_matrix);
    }
  }

  return absl::NotFoundError(
      absl::StrCat("Precomputed gains not found for input_key= ", input_key,
                   " and output_key= ", output_key));
}

}  // namespace iamf_tools
