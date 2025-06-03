// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_INSTALLER_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_INSTALLER_H_

#include <string>

namespace bruschetta {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BruschettaInstallResult {
  kUnknown = 0,
  kSuccess = 1,
  kInstallationProhibited = 2,
  kToolsDlcInstallError = 3,
  kDownloadError = 4,
  // Deprecated: kInvalidFirmware = 5,
  kInvalidBootDisk = 6,
  kInvalidPflash = 7,
  kUnableToOpenImages = 8,
  kCreateDiskError = 9,
  kStartVmFailed = 10,
  kInstallPflashError = 11,
  kFirmwareDlcInstallError = 12,
  kVmAlreadyExists = 13,
  kMaxValue = kVmAlreadyExists,
};

// Returns the string name of the BruschettaResult.
const char16_t* BruschettaInstallResultString(
    const BruschettaInstallResult error);

class BruschettaInstaller {
 public:
  enum class State {
    kInstallStarted,
    kToolsDlcInstall,
    kFirmwareDlcInstall,
    kBootDiskDownload,
    kPflashDownload,
    kOpenFiles,
    kCreateVmDisk,
    kInstallPflash,
    kStartVm,
    kLaunchTerminal,
  };

  class Observer {
   public:
    virtual void StateChanged(State state) = 0;
    virtual void Error(BruschettaInstallResult error) = 0;
  };

  virtual ~BruschettaInstaller() = default;

  virtual void Cancel() = 0;
  virtual void Install(std::string vm_name, std::string config_id) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 private:
};

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_INSTALLER_H_
