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

#ifndef CLI_RENDERER_GAINS_GET_GAINS_H_
#define CLI_RENDERER_GAINS_GET_GAINS_H_

#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace iamf_tools {

/*!\brief Gets precomputed gains for a given input and output layout pair.
 *
 * \param input_key Input layout name (e.g., "0+5+0", "A1").
 * \param output_key Output layout name (e.g., "Stereo", "7.1.2").
 * \return 2D vector of gains if successful, or an error status.
 */
absl::StatusOr<std::vector<std::vector<double>>> GetGainsForLayoutPair(
    absl::string_view input_key, absl::string_view output_key);

}  // namespace iamf_tools

#endif  // CLI_RENDERER_GAINS_GET_GAINS_H_
