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

#ifndef API_CONVERSION_PROFILE_CONVERSION_H_
#define API_CONVERSION_PROFILE_CONVERSION_H_

#include "absl/status/statusor.h"
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"
#include "iamf/obu/ia_sequence_header.h"

namespace iamf_tools {

/*!\brief Converts the API ProfileVersion to an internal ProfileVersion.
 *
 * \param api_profile_version API-requested profile version.
 * \return Internal profile version.
 */
iamf_tools::ProfileVersion ApiToInternalType(
    api::ProfileVersion api_profile_version);

/*!\brief Converts the internal IAMF ProfileVersion to the API ProfileVersion.
 *
 * \param profile_version Internal profile version.
 * \return API profile version, or an error if the internal profile version is
 *         not intended for use in the API.
 */
absl::StatusOr<iamf_tools::api::ProfileVersion> InternalToApiType(
    iamf_tools::ProfileVersion profile_version);

}  // namespace iamf_tools

#endif  // API_CONVERSION_PROFILE_CONVERSION_H_
