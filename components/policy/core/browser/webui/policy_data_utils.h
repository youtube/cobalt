// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_WEBUI_POLICY_DATA_UTILS_H_
#define COMPONENTS_POLICY_CORE_BROWSER_WEBUI_POLICY_DATA_UTILS_H_

#include <string>

#include "components/policy/policy_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {
class CloudPolicyManager;
}

namespace policy {

// Gets the domain that manages the policy.
POLICY_EXPORT absl::optional<std::string> GetManagedBy(
    const policy::CloudPolicyManager* manager);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_WEBUI_POLICY_DATA_UTILS_H_
