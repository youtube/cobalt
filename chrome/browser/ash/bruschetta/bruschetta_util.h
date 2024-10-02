// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_UTIL_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_UTIL_H_

#include "base/files/file_path.h"
#include "chrome/browser/ash/guest_os/guest_id.h"

#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace bruschetta {

extern const char kToolsDlc[];

extern const char kBruschettaVmName[];
extern const char kBruschettaDisplayName[];

extern const char kBiosPath[];
extern const char kPflashPath[];

extern const char kBruschettaPolicyId[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BruschettaResult {
  kUnknown = 0,
  kSuccess = 1,
  kDlcInstallError = 2,
  kBiosNotAccessible = 3,
  kStartVmFailed = 4,
  kTimeout = 5,
  kForbiddenByPolicy = 6,
  kMaxValue = kForbiddenByPolicy,
};

// The launch-time policy that applies to a specific VM. This is used to
// decide if we need to force a VM to shutdown after it's policy changed.
struct RunningVmPolicy {
  bool vtpm_enabled;
};

// Returns the string name of the BruschettaResult.
const char* BruschettaResultString(const BruschettaResult res);

guest_os::GuestId GetBruschettaAlphaId();

guest_os::GuestId MakeBruschettaId(std::string vm_name);

absl::optional<const base::Value::Dict*> GetRunnableConfig(
    const Profile* profile,
    const std::string& config_id);

base::FilePath BruschettaChromeOSBaseDirectory();

absl::optional<const base::Value::Dict*> GetInstallableConfig(
    const Profile* profile,
    const std::string& config_id);

base::flat_map<std::string, base::Value::Dict> GetInstallableConfigs(
    const Profile* profile);

// Returns true if an installable config for Bruschetta is present in the
// enterprise policy. (e.g. kBruschettaPolicyId)
bool HasInstallableConfig(const Profile* profile, const std::string& config_id);

// Returns true if Bruschetta is installed.
bool IsInstalled(Profile* profile, const guest_os::GuestId& guest_id);

absl::optional<RunningVmPolicy> GetLaunchPolicyForConfig(Profile* profile,
                                                         std::string config_id);

std::string GetVmUsername(const Profile* profile);

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_UTIL_H_
