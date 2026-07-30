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

#include "iamf/cli/renderer/gains/precomputed_compressed_gains_decoder.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/cli/renderer/gains/compressed_gains_codebook.h"
#include "iamf/cli/renderer/gains/precomputed_compressed_gains.h"

namespace iamf_tools {
namespace {

int GetChannelCount(const std::string& layout_name) {
  static const auto* kLayoutMap = new absl::flat_hash_map<std::string, int>{
      {"0+1+0", 1},    {"0+2+0", 2},  {"0+5+0", 6},  {"2+5+0", 8},
      {"4+5+0", 10},   {"0+7+0", 8},  {"4+7+0", 12}, {"9+10+3", 24},
      {"7.1.5.4", 17}, {"3.1.2", 6},  {"7.1.2", 10}, {"9.1.6", 16},
      {"4+5+1", 11},   {"3+7+0", 12}, {"4+9+0", 14}, {"A0", 1},
      {"A1", 4},       {"A2", 9},     {"A3", 16},    {"A4", 25},
  };

  auto layout = kLayoutMap->find(layout_name);
  if (layout != kLayoutMap->end()) {
    return layout->second;
  }
  return 0;
}

}  // namespace

absl::StatusOr<std::vector<std::vector<double>>> DecompressMatrix(
    const std::string& input_layout_name, const std::string& output_layout_name,
    const CompressedMatrix& compressed) {
  int num_input_channels = GetChannelCount(input_layout_name);
  int num_output_channels = GetChannelCount(output_layout_name);

  if (num_input_channels == 0 || num_output_channels == 0) {
    return absl::InvalidArgumentError("Invalid layout name.");
  }

  std::vector<std::vector<double>> decompressed(
      num_input_channels, std::vector<double>(num_output_channels, 0.0));

  if (!compressed.dense_data.empty()) {
    // Dense decompression. Pull float values from flattened vector, cast to
    // doubles and place in matrix in the correct position.
    size_t idx = 0;
    for (int i = 0; i < num_input_channels; ++i) {
      for (int j = 0; j < num_output_channels; ++j) {
        if (idx < compressed.dense_data.size()) {
          decompressed[i][j] =
              static_cast<double>(compressed.dense_data[idx++]);
        }
      }
    }
  } else if (!compressed.sparse_blob.empty()) {
    // Sparse decompression. Pull values from the combined blob.
    // Layout: [row_lengths] (num_input_channels bytes) + [col_indices] (M
    // bytes) + [codebook_indices] (M bytes), where M is the total number of
    // non-zero elements in the matrix.
    // - row_lengths: number of non-zero elements in each row.
    // - col_indices: column index of each non-zero element.
    // - codebook_indices: codebook index of each non-zero element.
    int num_rows = num_input_channels;
    if (compressed.sparse_blob.size() < static_cast<size_t>(num_rows)) {
      return absl::InvalidArgumentError("Sparse blob size too small.");
    }
    if ((compressed.sparse_blob.size() - num_rows) % 2 != 0) {
      return absl::InvalidArgumentError(
          "Mismatch between col_indices and codebook_indices size. These must "
          "match exactly.");
    }

    int total_non_zero_elements =
        (compressed.sparse_blob.size() - num_rows) / 2;

    int col_indices_start = num_rows;
    int codebook_indices_start = num_rows + total_non_zero_elements;

    // Running counter of total non-zero elements processed across all rows.
    int non_zero_element_counter = 0;
    const std::vector<double>& codebook = iamf_tools::GetCodebook();

    for (int i = 0; i < num_rows; ++i) {
      // row_lengths are the first num_rows bytes.
      int row_length = compressed.sparse_blob[i];

      for (int j = 0; j < row_length; ++j) {
        // Index of the current non-zero element in the flattened arrays.
        int element_idx = non_zero_element_counter++;
        if (element_idx >= total_non_zero_elements) {
          return absl::InvalidArgumentError("Corrupted sparse blob.");
        }

        int col = compressed.sparse_blob[col_indices_start + element_idx];
        uint8_t val_idx =
            compressed.sparse_blob[codebook_indices_start + element_idx];

        if (col < num_output_channels &&
            static_cast<size_t>(val_idx) < codebook.size()) {
          decompressed[i][col] = codebook[val_idx];
        }
      }
    }
  }

  return decompressed;
}

}  // namespace iamf_tools
