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

#include "iamf/api/conversion/profile_conversion.h"

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"
#include "iamf/obu/ia_sequence_header.h"

namespace iamf_tools {

typedef ::iamf_tools::ProfileVersion InternalProfileVersion;

InternalProfileVersion ApiToInternalType(
    api::ProfileVersion api_profile_version) {
  switch (api_profile_version) {
    case api::ProfileVersion::kIamfSimpleProfile:
      return InternalProfileVersion::kIamfSimpleProfile;
    case api::ProfileVersion::kIamfBaseProfile:
      return InternalProfileVersion::kIamfBaseProfile;
    case api::ProfileVersion::kIamfBaseEnhancedProfile:
      return InternalProfileVersion::kIamfBaseEnhancedProfile;
  }
  // Switch above is exhaustive.
  ABSL_LOG(FATAL) << "Invalid profile version= "
                  << static_cast<int>(api_profile_version);
}

absl::StatusOr<api::ProfileVersion> InternalToApiType(
    InternalProfileVersion profile_version) {
  switch (profile_version) {
    case InternalProfileVersion::kIamfSimpleProfile:
      return api::ProfileVersion::kIamfSimpleProfile;
    case InternalProfileVersion::kIamfBaseProfile:
      return api::ProfileVersion::kIamfBaseProfile;
    case InternalProfileVersion::kIamfBaseEnhancedProfile:
      return api::ProfileVersion::kIamfBaseEnhancedProfile;
    default:
      // Some internal profiles are not intended for use in the API.
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid profile version= ", profile_version));
  }
}

}  // namespace iamf_tools
