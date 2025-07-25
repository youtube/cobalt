// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_POLICY_WIN_GROUP_POLICY_MANAGER_H_
#define CHROME_UPDATER_POLICY_WIN_GROUP_POLICY_MANAGER_H_

#include <string>

#include "chrome/updater/policy/policy_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

// The GroupPolicyManager returns policies for domain-joined machines.
class GroupPolicyManager : public PolicyManager {
 public:
  GroupPolicyManager(
      bool should_take_policy_critical_section,
      const absl::optional<bool>& override_is_managed_device = absl::nullopt);
  GroupPolicyManager(const GroupPolicyManager&) = delete;
  GroupPolicyManager& operator=(const GroupPolicyManager&) = delete;

  bool CloudPolicyOverridesPlatformPolicy() const;

  // Overrides for PolicyManagerInterface.
  std::string source() const override;
  bool HasActiveDevicePolicies() const override;

 private:
  ~GroupPolicyManager() override;

  const bool is_managed_device_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_POLICY_WIN_GROUP_POLICY_MANAGER_H_
