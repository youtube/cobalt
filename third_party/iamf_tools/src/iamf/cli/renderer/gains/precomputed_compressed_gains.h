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

#ifndef CLI_RENDERER_GAINS_PRECOMPUTED_COMPRESSED_GAINS_H_
#define CLI_RENDERER_GAINS_PRECOMPUTED_COMPRESSED_GAINS_H_

#include <cstdint>
#include <vector>

#include "absl/types/span.h"

namespace iamf_tools {

struct CompressedMatrix {
  // Flattened dense data (Float32).
  std::vector<float> dense_data;

  // Combined blob for sparse matrices.
  // Layout: [row_lengths] (num_rows bytes) + [col_indices] (M bytes) +
  // [codebook_indices] (M bytes), where M is the total number of non-zero
  // elements in the matrix.
  // - row_lengths: number of non-zero elements in each row. 1 byte per row.
  // Assumes that the number of non-zero elements in each row is less than 256.
  // - col_indices: column index of each non-zero element. 1 byte per element.
  // Assumes that the number of columns is less than 256.
  // - codebook_indices: codebook index of each non-zero element (1 byte per
  // element).
  std::vector<uint8_t> sparse_blob;
};

/*!\brief Represents an entry in the table of precomputed gains.
 *
 * Each entry maps an input and output layout pair to either a dense or
 * sparse precomputed compressed gain matrix.
 */
struct PrecomputedGainEntry {
  const char* input_layout;
  const char* output_layout;
  bool is_dense;
  union {
    const float* dense_data;
    const uint8_t* sparse_blob;
  };
  int size;
};

/*!\brief Gets the table of precomputed gains.
 *
 * \return A span of entries representing all precomputed gains.
 */
absl::Span<const PrecomputedGainEntry> GetPrecomputedGainsTable();

}  // namespace iamf_tools

#endif  // CLI_RENDERER_GAINS_PRECOMPUTED_COMPRESSED_GAINS_H_
